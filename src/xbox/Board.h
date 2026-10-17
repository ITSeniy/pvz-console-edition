// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"
#include "DataArray.h"

namespace pvz::xbox {
struct Board {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x3C];
  rex::be<int32_t> x; // +0x40
  rex::be<int32_t> y; // +0x44
  rex::be<int32_t> width; // +0x48
  rex::be<int32_t> height; // +0x4C
  uint8_t pad_050[0xA4];
  GuestPtr app; // +0xF4
  DataArray zombies; // +0xF8
  DataArray plants; // +0x114
  DataArray projectiles; // +0x130
  DataArray coins; // +0x14C
  DataArray lawn_mowers; // +0x168
  DataArray grid_items; // +0x184
  uint8_t pad_1A0[0x4];
  GuestPtr seed_banks[2]; // +0x1A4
  uint8_t pad_1AC[0x4];
  GuestPtr controls[2]; // +0x1B0
  GuestPtr cursors[2]; // +0x1B8
  uint8_t pad_1C0[0x10];
  GuestPtr cut_scene; // +0x1D0
  GuestPtr challenge; // +0x1D4
  uint8_t pad_1D8[0x1];
  uint8_t paused; // +0x1D9
  uint8_t pad_1DA[0x5406];
  rex::be<int32_t> background; // +0x55E0
  uint8_t pad_55E4[0x10];
  rex::be<int32_t> sun[2]; // +0x55F4
  uint8_t pad_55FC[0x8];
  rex::be<int32_t> counter_5604; // +0x5604
  uint8_t pad_5608[0x4];
  rex::be<int32_t> draw_count; // +0x560C
  uint8_t pad_5610[0x10];
  rex::be<int32_t> tutorial_state; // +0x5620
  uint8_t pad_5624[0x6D];
  uint8_t shovel_enabled; // +0x5691
  uint8_t pad_5692[0x1];
  uint8_t hammer_enabled; // +0x5693
  rex::be<int32_t> coin_bank; // +0x5694
  uint8_t pad_5698[0x8];
  rex::be<int32_t> state_56a0; // +0x56A0
  uint8_t pad_56A4[0x150];
  rex::be<int32_t> counter_57f4; // +0x57F4
  uint8_t pad_57F8[0x44];
  rex::be<int32_t> shovels_used; // +0x583C
  uint8_t pad_5840[0x1C];
  uint8_t join_hint_blink; // +0x585C
  uint8_t pad_585D[0x7];
  uint8_t join_hint_storage[28]; // +0x5864
  uint8_t pad_5880[0x4];
};
static_assert(std::is_standard_layout_v<Board>);
static_assert(offsetof(Board, vtable) == 0x0);
static_assert(offsetof(Board, x) == 0x40);
static_assert(offsetof(Board, y) == 0x44);
static_assert(offsetof(Board, width) == 0x48);
static_assert(offsetof(Board, height) == 0x4C);
static_assert(offsetof(Board, app) == 0xF4);
static_assert(offsetof(Board, zombies) == 0xF8);
static_assert(offsetof(Board, plants) == 0x114);
static_assert(offsetof(Board, projectiles) == 0x130);
static_assert(offsetof(Board, coins) == 0x14C);
static_assert(offsetof(Board, lawn_mowers) == 0x168);
static_assert(offsetof(Board, grid_items) == 0x184);
static_assert(offsetof(Board, seed_banks) == 0x1A4);
static_assert(offsetof(Board, controls) == 0x1B0);
static_assert(offsetof(Board, cursors) == 0x1B8);
static_assert(offsetof(Board, cut_scene) == 0x1D0);
static_assert(offsetof(Board, challenge) == 0x1D4);
static_assert(offsetof(Board, paused) == 0x1D9);
static_assert(offsetof(Board, background) == 0x55E0);
static_assert(offsetof(Board, sun) == 0x55F4);
static_assert(offsetof(Board, counter_5604) == 0x5604);
static_assert(offsetof(Board, draw_count) == 0x560C);
static_assert(offsetof(Board, tutorial_state) == 0x5620);
static_assert(offsetof(Board, shovel_enabled) == 0x5691);
static_assert(offsetof(Board, hammer_enabled) == 0x5693);
static_assert(offsetof(Board, coin_bank) == 0x5694);
static_assert(offsetof(Board, state_56a0) == 0x56A0);
static_assert(offsetof(Board, counter_57f4) == 0x57F4);
static_assert(offsetof(Board, shovels_used) == 0x583C);
static_assert(offsetof(Board, join_hint_blink) == 0x585C);
static_assert(offsetof(Board, join_hint_storage) == 0x5864);
static_assert(sizeof(Board) == 0x5884);
} // namespace pvz::xbox
