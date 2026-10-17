// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct Plant {
  uint8_t pad_000[0x1C];
  GuestPtr app; // +0x1C
  GuestPtr board; // +0x20
  rex::be<int32_t> x; // +0x24
  rex::be<int32_t> y; // +0x28
  rex::be<int32_t> width; // +0x2C
  rex::be<int32_t> height; // +0x30
  uint8_t pad_034[0x4];
  rex::be<int32_t> row; // +0x38
  uint8_t pad_03C[0x4];
  rex::be<int32_t> seed_type; // +0x40
  rex::be<int32_t> column; // +0x44
  uint8_t pad_048[0x10];
  rex::be<int32_t> state; // +0x58
  rex::be<int32_t> health; // +0x5C
  rex::be<int32_t> max_health; // +0x60
  uint8_t pad_064[0xF0];
  rex::be<int32_t> imitater_type; // +0x154
  uint8_t pad_158[0x5];
  uint8_t dead; // +0x15D
  uint8_t pad_15E[0x3];
  uint8_t additive_highlight; // +0x161
  uint8_t pad_162[0x2];
  rex::be<int32_t> player_index; // +0x164
};
static_assert(std::is_standard_layout_v<Plant>);
static_assert(offsetof(Plant, app) == 0x1C);
static_assert(offsetof(Plant, board) == 0x20);
static_assert(offsetof(Plant, x) == 0x24);
static_assert(offsetof(Plant, y) == 0x28);
static_assert(offsetof(Plant, width) == 0x2C);
static_assert(offsetof(Plant, height) == 0x30);
static_assert(offsetof(Plant, row) == 0x38);
static_assert(offsetof(Plant, seed_type) == 0x40);
static_assert(offsetof(Plant, column) == 0x44);
static_assert(offsetof(Plant, state) == 0x58);
static_assert(offsetof(Plant, health) == 0x5C);
static_assert(offsetof(Plant, max_health) == 0x60);
static_assert(offsetof(Plant, imitater_type) == 0x154);
static_assert(offsetof(Plant, dead) == 0x15D);
static_assert(offsetof(Plant, additive_highlight) == 0x161);
static_assert(offsetof(Plant, player_index) == 0x164);
static_assert(sizeof(Plant) == 0x168);
} // namespace pvz::xbox
