#include "game_settings.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <atomic>
#include <chrono>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

REX_EXTERN(__imp__sub_822DB228); // WMV player creation
REX_EXTERN(__imp__sub_822DAC58); // WMV render/status
REX_EXTERN(__imp__sub_822B2138); // D3DDevice swap, the sole VdSwap caller
namespace {
std::atomic_bool skip_requested{false}, intro_active{false};
struct FrameClock {
  using Clock = std::chrono::steady_clock;
  Clock::time_point next{}, report = Clock::now();
  unsigned frames = 0;
#ifdef _WIN32
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  ~FrameClock() { if (timer) CloseHandle(timer); }
#endif
  void Wait() {
    const auto interval = std::chrono::nanoseconds(1000000000 / rex::cvar::Query<int32_t>("pvz_frame_limit"));
    auto now = Clock::now();
    if (next == Clock::time_point{} || now > next + interval * 2) next = now;
    if (now < next) {
#ifdef _WIN32
      LARGE_INTEGER due{};
      due.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(next - now).count() / 100;
      if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, INFINITE);
      else
#endif
        std::this_thread::sleep_until(next);
    }
    next += interval;
    ++frames;
    const double elapsed = std::chrono::duration<double>(Clock::now() - report).count();
    if (elapsed >= 10) {
      if (rex::cvar::Query<bool>("pvz_input_trace"))
        REXLOG_INFO("PvZ frame pacing: {} presents / {:.3f}s = {:.2f} FPS (limit {})", frames, elapsed, frames / elapsed,
            rex::cvar::Query<int32_t>("pvz_frame_limit"));
      frames = 0; report = Clock::now();
    }
  }
};
}
bool pvz::RequestIntroSkip() {
  if (!intro_active.load()) return false;
  skip_requested.store(true); return true;
}
REX_HOOK_RAW(sub_822DB228) {
  // Keep player/device initialization; skip through the end-of-movie path.
  if (ctx.lr == 0x8219FE2C) {
    skip_requested.store(false); intro_active.store(true);
  }
  __imp__sub_822DB228(ctx, base);
}
REX_HOOK_RAW(sub_822DAC58) {
  if (ctx.lr == 0x8219FEF0 && (rex::cvar::Query<bool>("pvz_skip_intro") || skip_requested.exchange(false))) {
    intro_active.store(false);
    ctx.r3.u64 = 0x16660026; // The native end-of-movie status, with normal cleanup.
    REXLOG_INFO("PvZ startup intro skipped ({})", rex::cvar::Query<bool>("pvz_skip_intro") ? "setting" : "click"); return;
  }
  __imp__sub_822DAC58(ctx, base);
  if (ctx.lr == 0x8219FEF0 && ctx.r3.u32 == 0x16660026) intro_active.store(false);
}
REX_HOOK_RAW(sub_822B2138) {
  // Pace front-buffer submissions, regardless of the native D3D wrapper.
  static thread_local FrameClock clock; clock.Wait();
  __imp__sub_822B2138(ctx, base);
}

