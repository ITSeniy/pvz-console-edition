// Xbox BigInt prefix from 825838F0 and 82584188; see xbox-layouts.csv.
#pragma once
#include "Types.h"
#include <cstddef>

namespace pvz::xbox {
struct BigInt {
  GuestPtr vtable;
  uint8_t negative;
  uint8_t pad_005[3];
  GuestPtr limbs;
  rex::be<uint32_t> count;
  rex::be<uint32_t> capacity;
};
static_assert(offsetof(BigInt, negative) == 4);
static_assert(offsetof(BigInt, limbs) == 8);
static_assert(offsetof(BigInt, count) == 0xC);
static_assert(offsetof(BigInt, capacity) == 0x10);
static_assert(sizeof(BigInt) == 0x14);
} // namespace pvz::xbox
