// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct LawnApp {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x104];
  rex::be<int32_t> width; // +0x108
  rex::be<int32_t> height; // +0x10C
  rex::be<int32_t> color_depth; // +0x110
  uint8_t pad_114[0x4];
  rex::be<double> music_volume; // +0x118
  uint8_t pad_120[0x24C];
  GuestPtr widget_manager; // +0x36C
  uint8_t pad_370[0xA0];
  GuestPtr music_interface; // +0x410
  uint8_t pad_414[0xB4];
  rex::be<uint32_t> field_4c8; // +0x4C8
  uint8_t pad_4CC[0xDA];
  uint8_t flag_5a6; // +0x5A6
  uint8_t pad_5A7[0x259];
  uint8_t flag_800; // +0x800
  uint8_t pad_801[0xFF];
  GuestPtr board; // +0x900
  uint8_t pad_904[0x4];
  GuestPtr main_menu; // +0x908
  uint8_t pad_90C[0x28];
  GuestPtr challenge_screen; // +0x934
  GuestPtr foley; // +0x938
  uint8_t pad_93C[0x90];
  rex::be<int32_t> game_mode; // +0x9CC
  rex::be<int32_t> game_scene; // +0x9D0
  uint8_t pad_9D4[0x18];
  GuestPtr field_9ec; // +0x9EC
  GuestPtr field_9f0; // +0x9F0
  GuestPtr effect_system; // +0x9F4
  uint8_t pad_9F8[0x4];
  GuestPtr profile; // +0x9FC
  uint8_t pad_A00[0x10];
  rex::be<int32_t> song_id; // +0xA10
  uint8_t pad_A14[0x4];
  rex::be<int32_t> crazy_dave_state; // +0xA18
  uint8_t pad_A1C[0x8];
  rex::be<int32_t> crazy_dave_message; // +0xA24
  uint8_t pad_A28[0x1C];
  rex::be<uint32_t> seed; // +0xA44
  uint8_t pad_A48[0xC];
  GuestPtr save_status; // +0xA54
  uint8_t pad_A58[0x48];
  uint8_t cheat_flags[7]; // +0xAA0
  uint8_t pad_AA7[0x7];
  uint8_t save_pending; // +0xAAE
  uint8_t pad_AAF[0x5];
  rex::be<int32_t> secondary_user; // +0xAB4
  uint8_t pad_AB8[0x5C];
  rex::be<int32_t> field_b14; // +0xB14
};
static_assert(std::is_standard_layout_v<LawnApp>);
static_assert(offsetof(LawnApp, vtable) == 0x0);
static_assert(offsetof(LawnApp, width) == 0x108);
static_assert(offsetof(LawnApp, height) == 0x10C);
static_assert(offsetof(LawnApp, color_depth) == 0x110);
static_assert(offsetof(LawnApp, music_volume) == 0x118);
static_assert(offsetof(LawnApp, widget_manager) == 0x36C);
static_assert(offsetof(LawnApp, music_interface) == 0x410);
static_assert(offsetof(LawnApp, field_4c8) == 0x4C8);
static_assert(offsetof(LawnApp, flag_5a6) == 0x5A6);
static_assert(offsetof(LawnApp, flag_800) == 0x800);
static_assert(offsetof(LawnApp, board) == 0x900);
static_assert(offsetof(LawnApp, main_menu) == 0x908);
static_assert(offsetof(LawnApp, challenge_screen) == 0x934);
static_assert(offsetof(LawnApp, foley) == 0x938);
static_assert(offsetof(LawnApp, game_mode) == 0x9CC);
static_assert(offsetof(LawnApp, game_scene) == 0x9D0);
static_assert(offsetof(LawnApp, field_9ec) == 0x9EC);
static_assert(offsetof(LawnApp, field_9f0) == 0x9F0);
static_assert(offsetof(LawnApp, effect_system) == 0x9F4);
static_assert(offsetof(LawnApp, profile) == 0x9FC);
static_assert(offsetof(LawnApp, song_id) == 0xA10);
static_assert(offsetof(LawnApp, crazy_dave_state) == 0xA18);
static_assert(offsetof(LawnApp, crazy_dave_message) == 0xA24);
static_assert(offsetof(LawnApp, seed) == 0xA44);
static_assert(offsetof(LawnApp, save_status) == 0xA54);
static_assert(offsetof(LawnApp, cheat_flags) == 0xAA0);
static_assert(offsetof(LawnApp, save_pending) == 0xAAE);
static_assert(offsetof(LawnApp, secondary_user) == 0xAB4);
static_assert(offsetof(LawnApp, field_b14) == 0xB14);
static_assert(sizeof(LawnApp) == 0xB18);
} // namespace pvz::xbox
