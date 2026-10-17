// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
// Known prefix only. Its C++ sizeof is not an allocation size for the Xbox class.
struct BaseGamepadControls {
  GuestPtr vtable; // +0x0
  GuestPtr listener_vtable; // +0x4
  uint8_t pad_008[0x18];
  GuestPtr app; // +0x20
  uint8_t pad_024[0x20];
  GuestPtr board; // +0x44
  uint8_t flag_48; // +0x48
  uint8_t pad_049[0x7];
  rex::be<float> vector_50[2]; // +0x50
  rex::be<float> cursor_x; // +0x58
  rex::be<float> cursor_y; // +0x5C
  rex::be<float> vector_60[2]; // +0x60
  rex::be<int32_t> state; // +0x68
  uint8_t pad_06C[0x4];
  rex::be<float> state_elapsed; // +0x70
  rex::be<float> target_x; // +0x74
  rex::be<float> target_y; // +0x78
  rex::be<float> delta_x; // +0x7C
  rex::be<float> delta_y; // +0x80
  rex::be<float> vector_84[2]; // +0x84
  rex::be<float> vector_8c[2]; // +0x8C
  rex::be<float> vector_94[2]; // +0x94
  rex::be<int32_t> player_index; // +0x9C
  rex::be<int32_t> user_index; // +0xA0
  uint8_t pad_0A4[0x8];
  rex::be<uint32_t> field_ac; // +0xAC
};
static_assert(std::is_standard_layout_v<BaseGamepadControls>);
static_assert(offsetof(BaseGamepadControls, vtable) == 0x0);
static_assert(offsetof(BaseGamepadControls, listener_vtable) == 0x4);
static_assert(offsetof(BaseGamepadControls, app) == 0x20);
static_assert(offsetof(BaseGamepadControls, board) == 0x44);
static_assert(offsetof(BaseGamepadControls, flag_48) == 0x48);
static_assert(offsetof(BaseGamepadControls, vector_50) == 0x50);
static_assert(offsetof(BaseGamepadControls, cursor_x) == 0x58);
static_assert(offsetof(BaseGamepadControls, cursor_y) == 0x5C);
static_assert(offsetof(BaseGamepadControls, vector_60) == 0x60);
static_assert(offsetof(BaseGamepadControls, state) == 0x68);
static_assert(offsetof(BaseGamepadControls, state_elapsed) == 0x70);
static_assert(offsetof(BaseGamepadControls, target_x) == 0x74);
static_assert(offsetof(BaseGamepadControls, target_y) == 0x78);
static_assert(offsetof(BaseGamepadControls, delta_x) == 0x7C);
static_assert(offsetof(BaseGamepadControls, delta_y) == 0x80);
static_assert(offsetof(BaseGamepadControls, vector_84) == 0x84);
static_assert(offsetof(BaseGamepadControls, vector_8c) == 0x8C);
static_assert(offsetof(BaseGamepadControls, vector_94) == 0x94);
static_assert(offsetof(BaseGamepadControls, player_index) == 0x9C);
static_assert(offsetof(BaseGamepadControls, user_index) == 0xA0);
static_assert(offsetof(BaseGamepadControls, field_ac) == 0xAC);
} // namespace pvz::xbox
