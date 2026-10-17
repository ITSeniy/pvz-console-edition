// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct Challenge {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x18];
  GuestPtr app; // +0x1C
  GuestPtr board; // +0x20
  uint8_t pad_024[0x4];
  uint8_t has_selected; // +0x28
  uint8_t pad_029[0x3];
  rex::be<int32_t> selected_x; // +0x2C
  rex::be<int32_t> selected_y; // +0x30
  uint8_t pad_034[0x36];
  uint8_t heavy_upgrade_0; // +0x6A
  uint8_t heavy_upgrade_1; // +0x6B
  uint8_t heavy_upgrade_2; // +0x6C
  uint8_t pad_06D[0x7];
  rex::be<int32_t> state; // +0x74
  uint8_t pad_078[0xC];
  rex::be<int32_t> match_count; // +0x84
  uint8_t pad_088[0x44];
  rex::be<int32_t> selected_column; // +0xCC
  rex::be<int32_t> selected_row; // +0xD0
  uint8_t pad_0D4[0xC];
  uint8_t flag_e0; // +0xE0
  uint8_t pad_0E1[0x37];
  rex::be<float> heavy_position; // +0x118
  rex::be<float> heavy_y; // +0x11C
  rex::be<float> heavy_angle; // +0x120
  uint8_t pad_124[0x8];
  rex::be<int32_t> counter_12c; // +0x12C
  rex::be<int32_t> counter_130; // +0x130
  uint8_t pad_134[0x18];
};
static_assert(std::is_standard_layout_v<Challenge>);
static_assert(offsetof(Challenge, vtable) == 0x0);
static_assert(offsetof(Challenge, app) == 0x1C);
static_assert(offsetof(Challenge, board) == 0x20);
static_assert(offsetof(Challenge, has_selected) == 0x28);
static_assert(offsetof(Challenge, selected_x) == 0x2C);
static_assert(offsetof(Challenge, selected_y) == 0x30);
static_assert(offsetof(Challenge, heavy_upgrade_0) == 0x6A);
static_assert(offsetof(Challenge, heavy_upgrade_1) == 0x6B);
static_assert(offsetof(Challenge, heavy_upgrade_2) == 0x6C);
static_assert(offsetof(Challenge, state) == 0x74);
static_assert(offsetof(Challenge, match_count) == 0x84);
static_assert(offsetof(Challenge, selected_column) == 0xCC);
static_assert(offsetof(Challenge, selected_row) == 0xD0);
static_assert(offsetof(Challenge, flag_e0) == 0xE0);
static_assert(offsetof(Challenge, heavy_position) == 0x118);
static_assert(offsetof(Challenge, heavy_y) == 0x11C);
static_assert(offsetof(Challenge, heavy_angle) == 0x120);
static_assert(offsetof(Challenge, counter_12c) == 0x12C);
static_assert(offsetof(Challenge, counter_130) == 0x130);
static_assert(sizeof(Challenge) == 0x14C);
} // namespace pvz::xbox
