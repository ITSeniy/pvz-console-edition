#include "display_sync.h"
#include <rex/logging.h>
#include <atomic>
#include <mutex>
#include <vector>
#ifdef _WIN32
#include <rex/ui/d3d12/d3d12_provider.h>

namespace {
using CreateFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND,
    const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT);
std::atomic<HWND> target_window{nullptr};
std::atomic<bool> synchronize{true};
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
    interval = enabled ? 1 : 0;
    if (enabled) flags &= ~(DXGI_PRESENT_ALLOW_TEARING | DXGI_PRESENT_RESTART);
    static std::atomic<int> reported{-1};
    if (reported.exchange(enabled ? 1 : 0) != (enabled ? 1 : 0))
      REXLOG_INFO("PvZ DXGI Present: sync interval {}, flags {:X}", interval, flags);
  }
  return original ? original(chain, interval, flags) : E_UNEXPECTED;
}
HRESULT STDMETHODCALLTYPE Create(IDXGIFactory2* factory, IUnknown* queue, HWND window,
    const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen,
    IDXGIOutput* output, IDXGISwapChain1** result) {
  const HRESULT status = create_original(factory, queue, window, desc, fullscreen, output, result);
  if (SUCCEEDED(status) && result && *result && window == target_window.load()) {
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
