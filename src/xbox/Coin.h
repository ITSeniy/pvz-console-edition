// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct Coin {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x18];
  GuestPtr app; // +0x1C
  GuestPtr board; // +0x20
  uint8_t pad_024[0x8];
  rex::be<int32_t> width; // +0x2C
  rex::be<int32_t> height; // +0x30
  uint8_t pad_034[0xC];
  rex::be<float> x; // +0x40
  rex::be<float> y; // +0x44
  rex::be<float> start_x; // +0x48
  rex::be<float> start_y; // +0x4C
  rex::be<float> velocity_x; // +0x50
  rex::be<float> velocity_y; // +0x54
  rex::be<float> scale; // +0x58
  uint8_t dead; // +0x5C
  uint8_t pad_05D[0x17];
  uint8_t flag_74; // +0x74
  uint8_t pad_075[0xB];
  rex::be<int32_t> coin_type; // +0x80
  rex::be<int32_t> motion; // +0x84
  uint8_t pad_088[0x10];
  rex::be<int32_t> seed_type; // +0x98
  uint8_t pad_09C[0x1C];
  rex::be<int32_t> collecting_player; // +0xB8
  rex::be<float> collection_elapsed; // +0xBC
  uint8_t pad_0C0[0x4];
};
static_assert(std::is_standard_layout_v<Coin>);
static_assert(offsetof(Coin, vtable) == 0x0);
static_assert(offsetof(Coin, app) == 0x1C);
static_assert(offsetof(Coin, board) == 0x20);
static_assert(offsetof(Coin, width) == 0x2C);
static_assert(offsetof(Coin, height) == 0x30);
static_assert(offsetof(Coin, x) == 0x40);
static_assert(offsetof(Coin, y) == 0x44);
static_assert(offsetof(Coin, start_x) == 0x48);
static_assert(offsetof(Coin, start_y) == 0x4C);
static_assert(offsetof(Coin, velocity_x) == 0x50);
static_assert(offsetof(Coin, velocity_y) == 0x54);
static_assert(offsetof(Coin, scale) == 0x58);
static_assert(offsetof(Coin, dead) == 0x5C);
static_assert(offsetof(Coin, flag_74) == 0x74);
static_assert(offsetof(Coin, coin_type) == 0x80);
static_assert(offsetof(Coin, motion) == 0x84);
static_assert(offsetof(Coin, seed_type) == 0x98);
static_assert(offsetof(Coin, collecting_player) == 0xB8);
static_assert(offsetof(Coin, collection_elapsed) == 0xBC);
static_assert(sizeof(Coin) == 0xC4);
} // namespace pvz::xbox
