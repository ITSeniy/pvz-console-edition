#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include "guest_memory.h"
#include <atomic>
#include <charconv>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <Windows.h>

// An empty string is disabled; decimal zero is a valid MT seed.
REXCVAR_DEFINE_STRING(pvz_deterministic_seed, "", "Diagnostics", "Override the local Sexy RNG seed wrapper with a decimal u32");
REXCVAR_DEFINE_STRING(pvz_deterministic_clock, "", "Diagnostics", "Override the Xbox CRT clock: decimal Unix seconds plus 10 ms per logical update");
REXCVAR_DEFINE_BOOL(pvz_replay_frame_locked, false, "Diagnostics", "Deliver replay rows synchronously on logical update steps");
REXCVAR_DEFINE_BOOL(pvz_replay_record_steps, false, "Diagnostics", "Record the logical delivery step of each seconds replay row");
REXCVAR_DEFINE_STRING(pvz_replay_rng_trace, "", "Diagnostics", "T05b investigation: JSONL global MT calls with host thread and logical step");

namespace pvz {
void ReplayLogicalStep(uint64_t step);
std::atomic<uint64_t> replay_step{0};
}

namespace {
struct ClockOverride {
  bool enabled = false;
  uint64_t file_time = 0;
};
const ClockOverride& DeterministicClock() {
  static const auto clock = [] {
    const auto text = REXCVAR_GET(pvz_deterministic_clock);
    if (text.empty()) return ClockOverride{};
    uint32_t seconds = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), seconds);
    // Xbox's 825F9600 is time32: it returns -1 past 0x7FFFD27F seconds.
    if (error != std::errc{} || end != text.data() + text.size() || seconds > 0x7FFFD27F) {
      REXLOG_ERROR("PvZ deterministic clock is not decimal Unix seconds in [0, 2147471999]: {}", text);
      return ClockOverride{};
    }
    constexpr uint64_t epoch_offset = 116444736000000000ULL;
    REXLOG_INFO("PvZ deterministic clock thunk 82616558 epoch {} step_ns 10000000", seconds);
    return ClockOverride{true, epoch_offset + uint64_t(seconds) * 10000000ULL};
  }();
  return clock;
}
}

REX_EXTERN(__imp__sub_824943A0);
REX_EXTERN(__imp__sub_8247FFC0);
REX_EXTERN(__imp__sub_824AC4C0);
REX_EXTERN(__imp__sub_82478108);
REX_EXTERN(__imp__sub_824752F0);
REX_EXTERN(__imp__sub_82616558);

// Xbox time32 825F9600 -> 82616558 -> KeQuerySystemTime (8275A65C).
// Override this CRT-only file-time thunk, not kernel clocks used by the host
// scheduler/GPU/audio. 825F9600 retains its original epoch conversion, range
// check, optional output pointer and signed return ABI.
REX_HOOK_RAW(sub_82616558) {
  const auto& clock = DeterministicClock();
  if (!clock.enabled) { __imp__sub_82616558(ctx, base); return; }
  const auto ticks = clock.file_time + pvz::replay_step.load() * 100000ULL;
  pvz::guest::Write(base, ctx.r3.u32, uint32_t(ticks >> 32));
  pvz::guest::Write(base, ctx.r3.u32 + 4, uint32_t(ticks));
}

// DoUpdateFrames calls LawnApp's virtual Update at 82478220..30 and returns
// false on its existing no-update path. CutScene::Update (8217C7D8..E0)
// waits for Board+560C (draw count) before placing preview zombies. Hold the
// NEXT whole app update until a real first draw, rather than forging that
// field or depending on how many logic updates fit before a host present.
REX_HOOK_RAW(sub_82478108) {
  static const bool locked = REXCVAR_GET(pvz_replay_frame_locked);
  if (locked) {
    const auto board = pvz::guest::Read(base, ctx.r3.u32 + 0x900);
    if (board && pvz::guest::Read(base, board) == 0x820B728C &&
        pvz::guest::Read(base, board + 0x560C) == 0) {
      ctx.r3.u64 = 0; return;
    }
  }
  __imp__sub_82478108(ctx, base);
}

// Real-time fractional callbacks are replaced by one native 1.0-step call
// after each root logic update below. 824752F0 gates app+6B4/+5A7 and forwards
// f1 to WidgetManager::UpdateF (824A6198); no global kernel clock is altered.
REX_HOOK_RAW(sub_824752F0) {
  static const bool locked = REXCVAR_GET(pvz_replay_frame_locked);
  if (!locked) __imp__sub_824752F0(ctx, base);
}

// MT global wrappers 82494370/80/90 use 828924E8. Trace the actual generator,
// including calls made by a background guest thread; never change its return.
REX_HOOK_RAW(sub_824AC4C0) {
  static const auto path = REXCVAR_GET(pvz_replay_rng_trace);
  const auto rng = ctx.r3.u32;
  if (path.empty() || rng != 0x828924E8) { __imp__sub_824AC4C0(ctx, base); return; }
  const auto step = pvz::replay_step.load();
  const auto caller = uint32_t(ctx.lr);
  // Both MT range wrappers save their caller LR at sp+0x58 (0x60 frame).
  const auto outer = caller == 0x824AC610 || caller == 0x824AC688 ?
      pvz::guest::Read(base, ctx.r1.u32 + 0x58) : caller;
  const auto before = pvz::guest::Read(base, rng + 0x9C0);
  __imp__sub_824AC4C0(ctx, base);
  static std::mutex mutex;
  static std::ofstream output;
  std::lock_guard lock(mutex);
  if (!output.is_open()) {
    const auto target = std::filesystem::path(path);
    if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path());
    output.open(target, std::ios::binary);
  }
  output << "{\"step\":" << step << ",\"thread\":" << GetCurrentThreadId()
         << ",\"lr\":" << caller << ",\"outer\":" << outer << ",\"before\":" << before
         << ",\"after\":" << pvz::guest::Read(base, rng + 0x9C0)
         << ",\"result\":" << ctx.r3.u32 << "}\n";
  output.flush();
}

REX_HOOK_RAW(sub_824943A0) {
  static const auto seed = REXCVAR_GET(pvz_deterministic_seed);
  if (!seed.empty()) {
    uint64_t value = 0;
    for (const char c : seed) {
      if (c < '0' || c > '9' || (value = value * 10 + (c - '0')) > UINT32_MAX) {
        REXLOG_ERROR("PvZ deterministic seed is not a decimal u32: {}", seed);
        __imp__sub_824943A0(ctx, base); return;
      }
    }
    ctx.r3.u64 = value;
    REXLOG_INFO("PvZ deterministic seed wrapper 824943A0 seed {}", value);
  }
  __imp__sub_824943A0(ctx, base);
}

// Xbox 8247FFE8 checks app+5A7, then 8247FFF4 calls 824A6140, whose
// 824A617C invokes the WidgetManager logical update. DispatchWidgets in the
// existing 824A6140 hook sees these publications before updating the board.
REX_HOOK_RAW(sub_8247FFC0) {
  static const bool locked = REXCVAR_GET(pvz_replay_frame_locked);
  static const bool record = REXCVAR_GET(pvz_replay_record_steps);
  const auto app = ctx.r3.u32;
  const bool updates = base[app + 0x5A7] == 0;
  if ((locked || record || DeterministicClock().enabled) && updates) {
    const auto step = pvz::replay_step.fetch_add(1);
    if (locked) pvz::ReplayLogicalStep(step);
  }
  __imp__sub_8247FFC0(ctx, base);
  if (locked && updates) {
    PPCContext fractional = ctx;
    fractional.r1.u32 -= 0x100;
    fractional.r3.u32 = app;
    fractional.f1.f64 = 1.0;
    __imp__sub_824752F0(fractional, base);
  }
}
