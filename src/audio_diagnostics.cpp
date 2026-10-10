#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/audio/audio_system.h>
#include <bit>
#include <cmath>

REXCVAR_DEFINE_BOOL(pvz_audio_trace, false, "Diagnostics", "Measure PCM output energy once per second");
// Keep the SDK's submission behavior; inspect the frame before handing it on.
REX_HOOK_RAW(__imp__XAudioSubmitRenderDriverFrame) {
  const auto driver = ctx.r3.u32, samples = ctx.r4.u32;
  if (REXCVAR_GET(pvz_audio_trace)) {
    static double energy = 0; static uint32_t frames = 0;
    const auto* data = rex::system::kernel_memory()->TranslateVirtual<const uint32_t*>(samples);
    for (int i = 0; i < 1536; ++i) {
      const float value = std::bit_cast<float>(__builtin_bswap32(data[i]));
      if (std::isfinite(value)) energy += double(value) * value;
    }
    if (++frames == 188) { REXLOG_INFO("PvZ audio output RMS {}", std::sqrt(energy / (188 * 1536))); frames = 0; energy = 0; }
  }
  auto* audio = static_cast<rex::audio::AudioSystem*>(rex::system::kernel_state()->emulator()->audio_system());
  audio->SubmitFrame(driver & 0xFFFF, samples); ctx.r3.u64 = 0;
}
