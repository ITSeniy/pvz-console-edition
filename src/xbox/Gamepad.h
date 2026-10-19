// Xbox Sexy::Gamepad prefix verified in 825808E8, 82580A38..82580CD4.
#pragma once
#include "Types.h"
namespace pvz::xbox {
struct Gamepad {
  GuestPtr vtable; // +0
  rex::be<uint32_t> user_index; // +4
  rex::be<uint32_t> field_08;
  uint8_t connected; // +C
  uint8_t pad_00d[3];
  rex::be<int32_t> axis_x; // +10
  rex::be<int32_t> axis_y; // +14
  uint8_t pad_018[0x124];
  rex::be<uint32_t> button_map[20]; // +13C..188, 82580924..825809B8
  rex::be<float> vibration_left; // +18C
  rex::be<float> vibration_right; // +190
  rex::be<float> vibration_duration; // +194
  rex::be<float> vibration_remaining; // +198
};
static_assert(offsetof(Gamepad,connected)==0xC);
static_assert(offsetof(Gamepad,axis_x)==0x10);
static_assert(offsetof(Gamepad,axis_y)==0x14);
static_assert(offsetof(Gamepad,vibration_left)==0x18C);
static_assert(offsetof(Gamepad,button_map)==0x13C);
static_assert(offsetof(Gamepad,vibration_remaining)==0x198);
} // namespace pvz::xbox
