// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"
#include "SeedPacket.h"

namespace pvz::xbox {
struct SeedBank {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x18];
  GuestPtr app; // +0x1C
  GuestPtr board; // +0x20
  rex::be<int32_t> x; // +0x24
  rex::be<int32_t> y; // +0x28
  rex::be<int32_t> width; // +0x2C
  rex::be<int32_t> height; // +0x30
  uint8_t pad_034[0xC];
  rex::be<int32_t> packet_count; // +0x40
  rex::be<uint32_t> field_44; // +0x44
  rex::be<int32_t> field_48; // +0x48
  uint8_t player_index; // +0x4C
  uint8_t pad_04D[0x3];
  SeedPacket packets[10]; // +0x50
};
static_assert(std::is_standard_layout_v<SeedBank>);
static_assert(offsetof(SeedBank, vtable) == 0x0);
static_assert(offsetof(SeedBank, app) == 0x1C);
static_assert(offsetof(SeedBank, board) == 0x20);
static_assert(offsetof(SeedBank, x) == 0x24);
static_assert(offsetof(SeedBank, y) == 0x28);
static_assert(offsetof(SeedBank, width) == 0x2C);
static_assert(offsetof(SeedBank, height) == 0x30);
static_assert(offsetof(SeedBank, packet_count) == 0x40);
static_assert(offsetof(SeedBank, field_44) == 0x44);
static_assert(offsetof(SeedBank, field_48) == 0x48);
static_assert(offsetof(SeedBank, player_index) == 0x4C);
static_assert(offsetof(SeedBank, packets) == 0x50);
static_assert(sizeof(SeedBank) == 0x528);
} // namespace pvz::xbox
