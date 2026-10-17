// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct SeedPacket {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x18];
  GuestPtr app; // +0x1C
  GuestPtr board; // +0x20
  rex::be<int32_t> x; // +0x24
  rex::be<int32_t> y; // +0x28
  rex::be<int32_t> width; // +0x2C
  rex::be<int32_t> height; // +0x30
  uint8_t pad_034[0xC];
  rex::be<int32_t> counter_40; // +0x40
  rex::be<int32_t> counter_44; // +0x44
  rex::be<int32_t> packet_index; // +0x48
  rex::be<int32_t> offset_x; // +0x4C
  rex::be<int32_t> seed_type; // +0x50
  rex::be<int32_t> imitater_type; // +0x54
  rex::be<int32_t> counter_58; // +0x58
  rex::be<int32_t> field_5c; // +0x5C
  rex::be<float> float_60; // +0x60
  uint8_t active; // +0x64
  uint8_t flag_65; // +0x65
  uint8_t pad_066[0x2];
  rex::be<int32_t> times_used; // +0x68
  GuestPtr seed_bank; // +0x6C
  rex::be<float> selection_animation; // +0x70
  rex::be<float> float_74; // +0x74
  rex::be<float> float_78; // +0x78
};
static_assert(std::is_standard_layout_v<SeedPacket>);
static_assert(offsetof(SeedPacket, vtable) == 0x0);
static_assert(offsetof(SeedPacket, app) == 0x1C);
static_assert(offsetof(SeedPacket, board) == 0x20);
static_assert(offsetof(SeedPacket, x) == 0x24);
static_assert(offsetof(SeedPacket, y) == 0x28);
static_assert(offsetof(SeedPacket, width) == 0x2C);
static_assert(offsetof(SeedPacket, height) == 0x30);
static_assert(offsetof(SeedPacket, counter_40) == 0x40);
static_assert(offsetof(SeedPacket, counter_44) == 0x44);
static_assert(offsetof(SeedPacket, packet_index) == 0x48);
static_assert(offsetof(SeedPacket, offset_x) == 0x4C);
static_assert(offsetof(SeedPacket, seed_type) == 0x50);
static_assert(offsetof(SeedPacket, imitater_type) == 0x54);
static_assert(offsetof(SeedPacket, counter_58) == 0x58);
static_assert(offsetof(SeedPacket, field_5c) == 0x5C);
static_assert(offsetof(SeedPacket, float_60) == 0x60);
static_assert(offsetof(SeedPacket, active) == 0x64);
static_assert(offsetof(SeedPacket, flag_65) == 0x65);
static_assert(offsetof(SeedPacket, times_used) == 0x68);
static_assert(offsetof(SeedPacket, seed_bank) == 0x6C);
static_assert(offsetof(SeedPacket, selection_animation) == 0x70);
static_assert(offsetof(SeedPacket, float_74) == 0x74);
static_assert(offsetof(SeedPacket, float_78) == 0x78);
static_assert(sizeof(SeedPacket) == 0x7C);
} // namespace pvz::xbox
