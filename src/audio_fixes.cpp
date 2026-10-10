#include "mouse_bridge.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <bit>

REXCVAR_DEFINE_BOOL(pvz_test_drop_menu_music, false, "Diagnostics", "Replay-only: simulate a lost menu music channel before changing SFX");
namespace {
uint32_t Read(uint8_t* base, uint32_t at) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + at)); }
}
REX_EXTERN(__imp__sub_82199968);
REX_EXTERN(sub_82477730); // SetMusicVolume; does not restart an active song
REX_EXTERN(sub_821C7E10); // Music::PlayMusic(tune), restores module/instrument settings
REX_EXTERN(sub_82558978); // FModMusicInterface::IsPlaying
REX_EXTERN(__imp__sub_82558598); // StopMusic (diagnostic fault only)
REX_HOOK_RAW(sub_82199968) {
  const auto app = Read(base, ctx.r3.u32 + 8), id = ctx.r4.u32;
  const bool menu = app && Read(base, app + 0x9D0) == 1;
  const auto music = app ? Read(base, app + 0x410) : 0;
  const auto song = app ? Read(base, app + 0xA10) : 0;
  const bool theme = song && Read(base, song + 0x1C) == app && Read(base, song + 0x24) == 7 && !base[song + 0x60];
  const auto value = ctx.f1.f64;
  if (rex::cvar::Query<bool>("pvz_input_trace")) REXLOG_INFO("PvZ audio slider {} value {}", id, value);
  // Exercise recovery without changing a user's save or stopping a real song
  // in normal launches. This flag is accepted only during a diagnostic replay.
  static bool dropped = false;
  if (menu && id == 1 && theme && !dropped &&
      REXCVAR_GET(pvz_test_drop_menu_music) && !rex::cvar::Query<std::string>("pvz_input_replay").empty()) {
    PPCContext work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = music; work.r4.u32 = 1;
    __imp__sub_82558598(work, base); dropped = true;
    REXLOG_INFO("PvZ audio diagnostic dropped menu channel");
  }
  __imp__sub_82199968(ctx, base);
  if (!menu || !theme || !music || (id != 0 && id != 1) || Read(base, app + 0x4C8)) return;
  const auto volume = std::bit_cast<double>((uint64_t(Read(base, app + 0x118)) << 32) | Read(base, app + 0x11C));
  if (volume <= 0) return;
  PPCContext work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = music; work.r4.u32 = 1;
  sub_82558978(work, base);
  const bool playing = work.r3.u32 != 0;
  if (!playing && theme) {
    // MakeSureMusicIsPlaying checks the tune ID, not the FMOD channel. The
    // full native play path also restores module/instrument volume settings.
    work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = song; work.r4.u32 = 7;
    sub_821C7E10(work, base);
    REXLOG_INFO("PvZ audio restored lost menu music channel");
  }
  if (id == 1) {
    work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = app; work.f1.f64 = volume;
    sub_82477730(work, base);
  }
  if (rex::cvar::Query<bool>("pvz_input_trace")) {
    work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = music; work.r4.u32 = 1;
    sub_82558978(work, base);
    REXLOG_INFO("PvZ audio menu music playing {} volume {}", work.r3.u32 != 0, volume);
  }
}

