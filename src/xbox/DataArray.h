// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct DataArray {
  GuestPtr block; // +0x0
  rex::be<uint32_t> max_used; // +0x4
  rex::be<uint32_t> capacity; // +0x8
  rex::be<uint32_t> free_head; // +0xC
  rex::be<uint32_t> size; // +0x10
  rex::be<uint32_t> next_key; // +0x14
  GuestPtr name; // +0x18
};
static_assert(std::is_standard_layout_v<DataArray>);
static_assert(offsetof(DataArray, block) == 0x0);
static_assert(offsetof(DataArray, max_used) == 0x4);
static_assert(offsetof(DataArray, capacity) == 0x8);
static_assert(offsetof(DataArray, free_head) == 0xC);
static_assert(offsetof(DataArray, size) == 0x10);
static_assert(offsetof(DataArray, next_key) == 0x14);
static_assert(offsetof(DataArray, name) == 0x18);
static_assert(sizeof(DataArray) == 0x1C);
} // namespace pvz::xbox
