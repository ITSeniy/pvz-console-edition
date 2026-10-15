#include "display_sync.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
REXCVAR_DEFINE_BOOL(pvz_present_trace, false, "Diagnostics", "Log DXGI present cadence every two seconds");
#ifdef _WIN32
#include <rex/ui/d3d12/d3d12_provider.h>
#include <wrl/client.h>
#include <string>

namespace {
// With pvz_present_trace, summarizes the game window's presents every two
// seconds from DXGI frame statistics: presents and new game frames, display
// refreshes and presents that reached the screen, and the time from a
// finished game frame to its first refresh. With VSync, a display up to
// 100 Hz should show one new frame per refresh and a faster one every second
// or third refresh.
// When recent guest frames were ready, before the frame clock wait.
std::atomic<int64_t> ready_qpc[256];
struct PresentTrace {
  using Clock = std::chrono::steady_clock;
  Clock::time_point start{};
  uint32_t presents = 0, fresh = 0, last_frame = 0;
  // The game frame of recent present ids, and the latest one on screen.
  UINT ids[64]{}; uint32_t frames[64]{};
  UINT seen_id = 0, first_refresh = 0, first_shown = 0; uint32_t shown_frame = 0;
  bool counting = false;
  double latency_sum = 0, latency_max = 0; uint32_t latency_count = 0;
  void Note(IDXGISwapChain3* chain, uint32_t frame) {
    const auto now = Clock::now();
    if (start == Clock::time_point{}) start = now;
    ++presents;
    if (frame != last_frame) { ++fresh; last_frame = frame; }
    UINT id = 0;
    if (SUCCEEDED(chain->GetLastPresentCount(&id))) { ids[id % 64] = id; frames[id % 64] = frame; }
    DXGI_FRAME_STATISTICS stats{};
    const bool have_stats = SUCCEEDED(chain->GetFrameStatistics(&stats));
    if (have_stats && !counting) { first_refresh = stats.SyncRefreshCount; first_shown = stats.PresentCount; counting = true; }
    if (have_stats && stats.PresentCount != seen_id && ids[stats.PresentCount % 64] == stats.PresentCount) {
      seen_id = stats.PresentCount;
      const uint32_t shown = frames[stats.PresentCount % 64];
      // A frame first shown on the latest refresh: time from ready to screen.
      if (shown != shown_frame && stats.PresentRefreshCount == stats.SyncRefreshCount) {
        LARGE_INTEGER frequency; QueryPerformanceFrequency(&frequency);
        const int64_t ready = ready_qpc[shown % 256].load(std::memory_order_relaxed);
        const double ms = double(stats.SyncQPCTime.QuadPart - ready) * 1000.0 / double(frequency.QuadPart);
        if (ready && ms > 0 && ms < 500) { latency_sum += ms; latency_max = std::max(latency_max, ms); ++latency_count; }
      }
      shown_frame = shown;
    }
    const double elapsed = std::chrono::duration<double>(now - start).count();
    if (elapsed < 2) return;
    DXGI_SWAP_CHAIN_DESC1 desc{}; chain->GetDesc1(&desc);
    REXLOG_INFO("PvZ present trace: {:.2f}s, thread {}, {}x{}: {} presents, {} new frames; {} refreshes, {} presents shown; "
        "ready to screen {:.1f} ms, max {:.1f}", elapsed, GetCurrentThreadId(), desc.Width, desc.Height, presents, fresh,
        have_stats && counting ? int64_t(stats.SyncRefreshCount - first_refresh) : -1,
        have_stats && counting ? int64_t(stats.PresentCount - first_shown) : -1,
        latency_count ? latency_sum / latency_count : 0.0, latency_max);
    start = now; presents = fresh = 0; latency_sum = latency_max = 0; latency_count = 0; counting = false;
  }
};
using CreateFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND,
    const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT);
std::atomic<HWND> target_window{nullptr};
std::atomic<bool> synchronize{true};
std::atomic<uint32_t> guest_frames{0};
// The fewest game presents waiting for the screen over the last 32 presents,
// published once per window and consumed by the frame clock.
std::atomic<uint32_t> queued_presents{0};
CreateFn create_original = nullptr;
struct Binding { void** slot; void* original; void* replacement; };
std::mutex binding_mutex;
std::vector<Binding> bindings;
bool Bind(void** slot, void* replacement) {
  std::lock_guard lock(binding_mutex);
  if (*slot == replacement) return true;
  DWORD protection;
  if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protection)) return false;
  const auto original = *slot;
  bindings.push_back({slot, original, replacement});
  InterlockedExchangePointer(slot, replacement);
  DWORD ignored; VirtualProtect(slot, sizeof(void*), protection, &ignored);
  return true;
}
HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain3* chain, UINT interval, UINT flags) {
  static thread_local IDXGISwapChain3* synchronized_chain = nullptr;
  static thread_local uint32_t synchronized_frame = 0;
  uint32_t frame = 0;
  bool game_present = false;
  auto** table = *reinterpret_cast<void***>(chain);
  PresentFn original = nullptr;
  {
    std::lock_guard lock(binding_mutex);
    for (const auto& binding : bindings) if (binding.slot == table + 8) {
      original = reinterpret_cast<PresentFn>(binding.original); break;
    }
  }
  HWND window = nullptr;
  if (SUCCEEDED(chain->GetHwnd(&window)) && window == target_window.load()) {
    const bool enabled = synchronize.load();
    // While an ImGui dialog is open, the presenter repaints from the UI thread
    // on every display refresh; otherwise it presents each guest frame once
    // from the GPU thread. Only a new guest frame waits for vertical blank:
    // a sync interval 1 present also stalls the shared GPU queue, and while
    // loading the guest command processor waits on that queue hundreds of
    // times. Without tearing, a repeated frame presented with interval 0 is
    // still composed on vertical blank.
    frame = guest_frames.load(std::memory_order_relaxed);
    game_present = true;
    interval = enabled && (chain != synchronized_chain || frame != synchronized_frame) ? 1 : 0;
    if (enabled) flags &= ~(DXGI_PRESENT_ALLOW_TEARING | DXGI_PRESENT_RESTART);
    static std::atomic<int> reported{-1};
    if (reported.exchange(enabled ? 1 : 0) != (enabled ? 1 : 0))
      REXLOG_INFO("PvZ DXGI Present policy: new frame interval {}, repeated frame interval 0, flags {:X}", enabled ? 1 : 0, flags);
  }
  const auto status = original ? original(chain, interval, flags) : E_UNEXPECTED;
  // DO_NOT_WAIT may reject a busy queue. Retry synchronization until DXGI
  // accepts this frame, including the first frame of a recreated swap chain.
  if (game_present && SUCCEEDED(status)) {
    synchronized_chain = chain;
    synchronized_frame = frame;
    UINT last = 0; DXGI_FRAME_STATISTICS stats{};
    static uint32_t fewest = UINT32_MAX, sampled = 0;
    if (SUCCEEDED(chain->GetLastPresentCount(&last)) && SUCCEEDED(chain->GetFrameStatistics(&stats))) {
      fewest = std::min<uint32_t>(fewest, last - stats.PresentCount);
      if (++sampled == 32) { queued_presents.store(fewest, std::memory_order_relaxed); fewest = UINT32_MAX; sampled = 0; }
    }
    if (REXCVAR_GET(pvz_present_trace)) {
      static PresentTrace trace;
      trace.Note(chain, frame);
    }
  }
  return status;
}
HRESULT STDMETHODCALLTYPE Create(IDXGIFactory2* factory, IUnknown* queue, HWND window,
    const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
    IDXGIOutput* output, IDXGISwapChain1** result) {
  const HRESULT status = create_original(factory, queue, window, desc, fullscreen, output, result);
  if (SUCCEEDED(status) && result && *result && window == target_window.load()) {
    REXLOG_INFO("PvZ swap chain created: {}x{} flags {:X}, window visible {} foreground {}", desc->Width, desc->Height,
        desc->Flags, IsWindowVisible(window) != 0, GetForegroundWindow() == window);
    IDXGISwapChain3* chain = nullptr;
    if (SUCCEEDED((*result)->QueryInterface(IID_PPV_ARGS(&chain)))) {
      auto** table = *reinterpret_cast<void***>(chain);
      if (!Bind(table + 8, reinterpret_cast<void*>(&Present)))
        REXLOG_ERROR("PvZ cannot bind DXGI presentation synchronization");
      chain->Release();
    }
  }
  return status;
}
}
#endif
#ifdef _WIN32
namespace {
// With VSync the game frame clock follows the refresh of the display that
// shows the window. The game simulates in 10 ms steps and draws at most 100
// frames per second without changing its speed, so a 75 Hz display gets 75
// frames and a 100 Hz display one step per frame. A fixed 60 FPS clock
// repeats every fourth frame at 75 Hz and fills the present queue on a
// display slightly slower than 60 Hz, adding two frames of input latency.
// Faster displays wait for every second or third refresh.
struct DisplayClock {
  Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
  Microsoft::WRL::ComPtr<IDXGIOutput> output;
  HMONITOR monitor = nullptr;
  std::wstring device;
  unsigned hz = 0, refreshes = 0;
  std::chrono::steady_clock::time_point checked{}, last{};
  bool Update(HMONITOR target) {
    const auto now = std::chrono::steady_clock::now();
    if (target == monitor && output && now - checked < std::chrono::seconds(1)) return true;
    checked = now;
    if (target != monitor || !output || !factory || !factory->IsCurrent()) {
      output.Reset(); monitor = nullptr;
      if ((!factory || !factory->IsCurrent()) && FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
        return false;
      Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
      for (UINT a = 0; !output && SUCCEEDED(factory->EnumAdapters(a, adapter.ReleaseAndGetAddressOf())); ++a) {
        Microsoft::WRL::ComPtr<IDXGIOutput> candidate;
        for (UINT o = 0; SUCCEEDED(adapter->EnumOutputs(o, candidate.ReleaseAndGetAddressOf())); ++o) {
          DXGI_OUTPUT_DESC desc;
          if (SUCCEEDED(candidate->GetDesc(&desc)) && desc.Monitor == target) {
            output = candidate; device = desc.DeviceName; break;
          }
        }
      }
      if (!output) return false;
      monitor = target;
    }
    // The fullscreen setting may switch the same monitor to 60 Hz.
    DEVMODEW mode{}; mode.dmSize = sizeof(mode);
    hz = EnumDisplaySettingsW(device.c_str(), ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1 ?
        mode.dmDisplayFrequency : 60;
    const unsigned next = (hz + 99) / 100;
    if (next != refreshes) REXLOG_INFO("PvZ frame pacing: {} Hz display, a frame every {} refresh(es)", hz, next);
    refreshes = next;
    return true;
  }
};
}
#endif
bool pvz::WaitForDisplayRefresh() {
#ifdef _WIN32
  const HWND window = target_window.load();
  if (!window || !synchronize.load()) return false;
  static thread_local DisplayClock clock;
  const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
  if (!monitor || IsIconic(window) || !clock.Update(monitor)) return false;
  // A call right at a vertical blank returns at once, and a frame drawn in
  // the same refresh as the previous one would queue up behind it. Wait until
  // at least half of the frame period has passed since the previous frame.
  const auto period = std::chrono::nanoseconds(1000000000ll * clock.refreshes / clock.hz);
  // Frames queued during loading stay queued at the display rate and delay
  // input by a frame each. A present is shown two refreshes after the wait;
  // when more than two stayed pending for 32 presents, skip one refresh.
  const uint32_t queued = queued_presents.exchange(0, std::memory_order_relaxed);
  const unsigned refreshes = clock.refreshes + (queued > 2 && queued < 8 ? 1 : 0);
  for (unsigned waits = 0; ; ++waits) {
    for (unsigned i = 0; i < refreshes; ++i)
      if (FAILED(clock.output->WaitForVBlank())) { clock.output.Reset(); return false; }
    const auto now = std::chrono::steady_clock::now();
    if (now - clock.last >= period / 2) { clock.last = now; return true; }
    // A sleeping or remote display may not wait at all; use the timer then.
    if (waits) { clock.last = now; return false; }
  }
#else
  return false;
#endif
}
bool pvz::InstallDisplaySync(rex::ui::GraphicsProvider* provider, void* window, bool enabled) {
#ifdef _WIN32
  auto* d3d = dynamic_cast<rex::ui::d3d12::D3D12Provider*>(provider);
  if (!d3d || !window) return false;
  target_window.store(static_cast<HWND>(window)); synchronize.store(enabled);
  auto** table = *reinterpret_cast<void***>(d3d->GetDXGIFactory());
  if (!create_original) create_original = reinterpret_cast<CreateFn>(table[15]);
  // The bundled 0.10 presenter hard-codes Present(0). Bind the documented
  // DXGI methods in this process, scoped to the game HWND. No SDK DLL or
  // driver code is modified; recreation/resizing keeps using the same path.
  return Bind(table + 15, reinterpret_cast<void*>(&Create));
#else
  return false;
#endif
}
#ifdef _WIN32
namespace { thread_local int64_t frame_ready_qpc = 0; }
#endif
void pvz::NoteFrameReady() {
#ifdef _WIN32
  LARGE_INTEGER now; QueryPerformanceCounter(&now); frame_ready_qpc = now.QuadPart;
#endif
}
void pvz::NoteGuestFrame() {
  const uint32_t frame = guest_frames.fetch_add(1, std::memory_order_relaxed) + 1;
#ifdef _WIN32
  if (REXCVAR_GET(pvz_present_trace)) ready_qpc[frame % 256].store(frame_ready_qpc, std::memory_order_relaxed);
#endif
}
void pvz::RemoveDisplaySync() {
#ifdef _WIN32
  target_window.store(nullptr);
  std::lock_guard lock(binding_mutex);
  for (auto it = bindings.rbegin(); it != bindings.rend(); ++it) {
    DWORD protection;
    if (*it->slot == it->replacement && VirtualProtect(it->slot, sizeof(void*), PAGE_READWRITE, &protection)) {
      InterlockedExchangePointer(it->slot, it->original);
      DWORD ignored; VirtualProtect(it->slot, sizeof(void*), protection, &ignored);
    }
  }
#endif
}
