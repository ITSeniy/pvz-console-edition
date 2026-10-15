#include "game_settings.h"
#include "display_sync.h"
#include "hitch_profiler.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <algorithm>
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
  Clock::time_point next{}, report = Clock::now(), previous{};
  unsigned frames = 0;
  double longest = 0;
#ifdef _WIN32
  HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  ~FrameClock() { if (timer) CloseHandle(timer); }
#endif
  void Wait() {
    const auto interval = std::chrono::nanoseconds(1000000000 / rex::cvar::Query<int32_t>("pvz_frame_limit"));
    auto now = Clock::now();
    // With VSync the display refresh is the clock; the timer is the fallback.
    if (pvz::WaitForDisplayRefresh()) next = Clock::now();
    else if (next == Clock::time_point{} || now > next + interval * 2) next = now;
    else if (now < next) {
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
    // A stalled game thread shows up as one long present interval; name it
    // in the log so freezes can be matched with what the game was doing.
    const auto presented = Clock::now();
    if (previous != Clock::time_point{}) {
      const double frame = std::chrono::duration<double, std::milli>(presented - previous).count();
      longest = std::max(longest, frame);
      if (frame >= 100) REXLOG_INFO("PvZ frame hitch: {:.0f} ms", frame);
    }
    previous = presented;
    const double elapsed = std::chrono::duration<double>(presented - report).count();
    if (elapsed >= 10) {
      if (rex::cvar::Query<bool>("pvz_input_trace"))
        REXLOG_INFO("PvZ frame pacing: {} presents / {:.3f}s = {:.2f} FPS (timer limit {}), longest {:.0f} ms", frames, elapsed,
            frames / elapsed, rex::cvar::Query<int32_t>("pvz_frame_limit"), longest);
      frames = 0; longest = 0; report = presented;
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
  // The finished frame goes out at once; the wait then holds the next update
  // until the refresh, so input is read late and a frame never sits ready.
  pvz::NoteFrameReady();
  pvz::NotePresent();
  pvz::NoteGuestFrame();
  __imp__sub_822B2138(ctx, base);
  static thread_local FrameClock clock; clock.Wait();
}

