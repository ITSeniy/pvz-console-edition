// T08 optional diagnostic overlay. Build instructions and Xbox evidence:
// docs/native-port/render-boundary.md. This TU is inert in a normal build.
// Existing port hooks are delegated through renamed wrappers in derived TUs;
// their source files and the generated guest code are never edited.
#ifdef PVZ_RENDER_PROBE_OVERLAY
#include "guest_memory.h"
#include "guest_layout.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <fmt/format.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

REXCVAR_DEFINE_STRING(pvz_render_probe, "", "Diagnostics", "T08 JSONL rendering counters; empty disables recording");
REXCVAR_DEFINE_STRING(pvz_render_probe_scene, "menu", "Diagnostics", "T08 scene gate: menu/adventure/aquarium/versus");
REXCVAR_DEFINE_UINT32(pvz_render_probe_frames, 180, "Diagnostics", "T08 complete present intervals to record");

// Names are intentionally absent from the machine trace: some low candidates
// are probes for discovery. Only Xbox-proven operations receive names in docs.
// The last argument preserves any existing port wrapper, including its own
// conditional delegation. No render/input/game registers are changed here.
#define T08_POINTS(X) \
 X(8249B1A0, true, __imp__sub_8249B1A0) \
 X(8249B1D8, true, __imp__sub_8249B1D8) \
 X(8249B258, true, __imp__sub_8249B258) \
 X(8249B2D8, true, __imp__sub_8249B2D8) \
 X(8249B408, true, t08_body_sub_8249B408) \
 X(8249B478, true, __imp__sub_8249B478) \
 X(8249B4A0, true, __imp__sub_8249B4A0) \
 X(8249B4E8, true, __imp__sub_8249B4E8) \
 X(8249B508, true, __imp__sub_8249B508) \
 X(8249B588, true, __imp__sub_8249B588) \
 X(8249B650, true, __imp__sub_8249B650) \
 X(8249BC50, true, t08_body_sub_8249BC50) \
 X(8249BEF0, true, __imp__sub_8249BEF0) \
 X(8249C058, true, t08_body_sub_8249C058) \
 X(8249C150, true, __imp__sub_8249C150) \
 X(8249C230, true, __imp__sub_8249C230) \
 X(8249C378, true, __imp__sub_8249C378) \
 X(8249C430, true, t08_body_sub_8249C430) \
 X(8249C4E0, true, __imp__sub_8249C4E0) \
 X(8249C590, true, __imp__sub_8249C590) \
 X(8249C5D0, true, __imp__sub_8249C5D0) \
 X(8249C690, true, __imp__sub_8249C690) \
 X(8249C6A8, true, t08_body_sub_8249C6A8) \
 X(8249C760, true, __imp__sub_8249C760) \
 X(8249D0E8, true, t08_body_sub_8249D0E8) \
 X(8249D250, true, t08_body_sub_8249D250) \
 X(8249D298, true, __imp__sub_8249D298) \
 X(8249D388, true, t08_body_sub_8249D388) \
 X(8249D458, true, __imp__sub_8249D458) \
 X(8249D588, true, __imp__sub_8249D588) \
 X(8249D688, true, __imp__sub_8249D688) \
 X(8249CFB8, true, t08_body_sub_8249CFB8) \
 X(8249DA90, true, t08_body_sub_8249DA90) \
 X(8249DB38, true, __imp__sub_8249DB38) \
 X(8249DB80, true, __imp__sub_8249DB80) \
 X(8249DBE0, true, __imp__sub_8249DBE0) \
 X(824CB1C0, false, __imp__sub_824CB1C0) \
 X(824CB300, false, __imp__sub_824CB300) \
 X(824CB088, false, __imp__sub_824CB088) \
 X(824CAF18, false, __imp__sub_824CAF18) \
 X(824CC9D0, false, __imp__sub_824CC9D0) \
 X(824CCBD8, false, __imp__sub_824CCBD8) \
 X(824CCD90, false, __imp__sub_824CCD90) \
 X(824D0DD0, false, __imp__sub_824D0DD0) \
 X(824CFDB0, false, __imp__sub_824CFDB0) \
 X(824D5ED0, false, __imp__sub_824D5ED0) \
 X(824D76E0, false, __imp__sub_824D76E0) \
 X(824D7700, false, __imp__sub_824D7700) \
 X(824A3FB8, false, __imp__sub_824A3FB8) \
 X(824A4370, false, __imp__sub_824A4370) \
 X(824A45C0, false, __imp__sub_824A45C0) \
 X(824A2A80, false, __imp__sub_824A2A80) \
 X(824A2C58, false, __imp__sub_824A2C58) \
 X(824A2EA8, false, __imp__sub_824A2EA8) \
 X(824A0FF8, false, __imp__sub_824A0FF8) \
 X(824A1228, false, __imp__sub_824A1228) \
 X(824A3BE0, false, __imp__sub_824A3BE0) \
 X(8255D4D8, false, __imp__sub_8255D4D8) \
 X(8255CE78, false, __imp__sub_8255CE78) \
 X(8255BD80, false, __imp__sub_8255BD80) \
 X(82186540, false, __imp__sub_82186540) \
 X(821E0828, false, __imp__sub_821E0828) \
 X(821E1560, false, t08_body_sub_821E1560) \
 X(8227C190, false, t08_body_sub_8227C190) \
 X(8227C240, false, t08_body_sub_8227C240) \
 X(82480430, false, __imp__sub_82480430) \
 X(824B2D20, false, t08_body_sub_824B2D20) \
 X(822B2138, false, t08_body_sub_822B2138)

namespace {
using pvz::guest::Read;
constexpr uint32_t addresses[]{
#define T08_ADDRESS(address, graphics, original) 0x##address,
T08_POINTS(T08_ADDRESS)
#undef T08_ADDRESS
};
constexpr size_t kCount = std::size(addresses);
struct Counter {
  std::atomic<uint64_t> calls{}, outside_graphics{}, lifetime{};
  std::array<std::atomic<uint32_t>, 128> callers{};
  std::atomic<uint64_t> caller_overflow{};
};
std::array<Counter, kCount> counters;
thread_local unsigned graphics_depth = 0;
std::mutex output_mutex;
std::ofstream output;
uint32_t frame = 0;
bool gated = false, finished = false, attempted = false;

template<uint32_t Address> consteval size_t Index() {
  for (size_t i = 0; i < kCount; ++i) if (addresses[i] == Address) return i;
  throw "missing render probe address";
}
bool Enabled() {
  static const bool enabled = !REXCVAR_GET(pvz_render_probe).empty();
  return enabled;
}
bool Scene(uint8_t* base, uint32_t& mode, uint32_t& scene, bool& board) {
  const auto app = Read(base, 0x82D0BC88);
  if (!app) return false;
  mode = Read(base, app + 0x9CC); scene = Read(base, app + 0x9D0);
  const auto candidate = Read(base, app + 0x900);
  board = candidate && Read(base, candidate) == pvz::layout::kBoard;
  const auto wanted = REXCVAR_GET(pvz_render_probe_scene);
  if (wanted == "menu") return scene == 1 && !board;
  const auto wanted_mode = wanted == "adventure" ? 0u : wanted == "aquarium" ? 24u : wanted == "versus" ? 76u : ~0u;
  return board && scene == 3 && mode == wanted_mode && !base[candidate + 0x1D9];
}
void Present(uint8_t* base) {
  if (!Enabled() || finished) return;
  std::lock_guard lock(output_mutex);
  uint32_t mode = 0, scene = 0; bool board = false;
  const bool matches = Scene(base, mode, scene, board);
  if (matches && !attempted) {
    attempted = true;
    const auto path = std::filesystem::path(REXCVAR_GET(pvz_render_probe));
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    output.open(path, std::ios::binary | std::ios::trunc);
    if (!output) {
      finished = true; REXLOG_ERROR("T08 could not open {}", path.string()); return;
    }
    output << "{\"record\":\"header\",\"schema\":1,\"xbox_sha256\":\"27e8e044a0b7e232ce43e1de2cf06b377040f3d24e67a86047b337bb46d257ea\",\"scene\":\""
           << REXCVAR_GET(pvz_render_probe_scene) << "\",\"addresses\":[";
    for (size_t i = 0; i < kCount; ++i) output << (i ? "," : "") << addresses[i];
    output << "]}\n";
  }
  // Discard the first matching partial interval; count only complete intervals
  // bounded by two presents in the selected scene. Atomic exchange includes
  // work on other threads; nesting ('outside') is local to each calling thread.
  const bool record = matches && gated && output;
  if (record) output << fmt::format("{{\"record\":\"frame\",\"frame\":{},\"mode\":{},\"scene\":{},\"board\":{},\"calls\":[", frame, mode, scene, board);
  std::array<uint64_t, kCount> outside{};
  for (size_t i = 0; i < kCount; ++i) {
    const auto count = counters[i].calls.exchange(0, std::memory_order_relaxed);
    outside[i] = counters[i].outside_graphics.exchange(0, std::memory_order_relaxed);
    if (record) output << (i ? "," : "") << count;
  }
  if (record) {
    output << "],\"outside_graphics\":[";
    for (size_t i = 0; i < kCount; ++i) output << (i ? "," : "") << outside[i];
    output << "]}\n"; output.flush(); ++frame;
    if (frame >= REXCVAR_GET(pvz_render_probe_frames)) {
      output << "{\"record\":\"totals\",\"lifetime_calls\":[";
      for (size_t i = 0; i < kCount; ++i) output << (i ? "," : "") << counters[i].lifetime.load();
      output << "],\"callers\":[";
      for (size_t i = 0; i < kCount; ++i) {
        output << (i ? ",[" : "["); bool comma = false;
        for (const auto& caller : counters[i].callers) if (caller.load()) {
          output << (comma ? "," : "") << caller.load(); comma = true;
        }
        output << "]";
      }
      output << "],\"caller_overflow\":[";
      for (size_t i = 0; i < kCount; ++i) output << (i ? "," : "") << counters[i].caller_overflow.load();
      output << "]}\n"; output.close(); finished = true;
      REXLOG_INFO("T08 recorded {} complete present intervals", frame);
    }
  }
  gated = matches;
}
template<uint32_t Address, bool Graphics>
void Trace(PPCFunc* original, PPCContext& ctx, uint8_t* base) {
  if (!Enabled()) { original(ctx, base); return; }
  auto& counter = counters[Index<Address>()];
  counter.calls.fetch_add(1, std::memory_order_relaxed);
  counter.lifetime.fetch_add(1, std::memory_order_relaxed);
  if (!graphics_depth) counter.outside_graphics.fetch_add(1, std::memory_order_relaxed);
  bool stored = false;
  const auto caller = uint32_t(ctx.lr);
  for (auto& slot : counter.callers) {
    auto old = slot.load(std::memory_order_relaxed);
    if (old == caller || (!old && slot.compare_exchange_strong(old, caller, std::memory_order_relaxed))) { stored = true; break; }
  }
  if (!stored) counter.caller_overflow.fetch_add(1, std::memory_order_relaxed);
  if constexpr (Graphics) ++graphics_depth;
  original(ctx, base);
  if constexpr (Graphics) --graphics_depth;
  if constexpr (Address == 0x822B2138) Present(base);
}
}

#define T08_HOOK(address, graphics, original) \
REX_EXTERN(original); \
REX_HOOK_RAW(sub_##address) { Trace<0x##address, graphics>(original, ctx, base); }
T08_POINTS(T08_HOOK)
#undef T08_HOOK
#endif // PVZ_RENDER_PROBE_OVERLAY
