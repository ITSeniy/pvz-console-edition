// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct Zombie {
  uint8_t pad_000[0x1C];
  GuestPtr app; // +0x1C
  GuestPtr board; // +0x20
  rex::be<int32_t> rect_x; // +0x24
  rex::be<int32_t> rect_y; // +0x28
  rex::be<int32_t> width; // +0x2C
  rex::be<int32_t> height; // +0x30
  uint8_t pad_034[0x4];
  rex::be<int32_t> row; // +0x38
  uint8_t pad_03C[0x4];
  rex::be<int32_t> zombie_type; // +0x40
  rex::be<int32_t> phase; // +0x44
  rex::be<float> x; // +0x48
  rex::be<float> y; // +0x4C
  uint8_t pad_050[0x94];
  rex::be<int32_t> body_health; // +0xE4
  rex::be<int32_t> body_max_health; // +0xE8
  uint8_t pad_0EC[0x1C];
  uint8_t dead; // +0x108
  uint8_t pad_109[0x77];
};
static_assert(std::is_standard_layout_v<Zombie>);
static_assert(offsetof(Zombie, app) == 0x1C);
static_assert(offsetof(Zombie, board) == 0x20);
static_assert(offsetof(Zombie, rect_x) == 0x24);
static_assert(offsetof(Zombie, rect_y) == 0x28);
static_assert(offsetof(Zombie, width) == 0x2C);
static_assert(offsetof(Zombie, height) == 0x30);
static_assert(offsetof(Zombie, row) == 0x38);
static_assert(offsetof(Zombie, zombie_type) == 0x40);
static_assert(offsetof(Zombie, phase) == 0x44);
static_assert(offsetof(Zombie, x) == 0x48);
static_assert(offsetof(Zombie, y) == 0x4C);
static_assert(offsetof(Zombie, body_health) == 0xE4);
static_assert(offsetof(Zombie, body_max_health) == 0xE8);
static_assert(offsetof(Zombie, dead) == 0x108);
static_assert(sizeof(Zombie) == 0x180);
} // namespace pvz::xbox
