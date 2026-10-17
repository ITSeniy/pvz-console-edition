// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct GamepadControls {
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
  uint8_t pad_060[0x8];
  rex::be<int32_t> state; // +0x68
  uint8_t pad_06C[0x4];
  rex::be<float> state_elapsed; // +0x70
  rex::be<float> target_x; // +0x74
  rex::be<float> target_y; // +0x78
  rex::be<float> delta_x; // +0x7C
  rex::be<float> delta_y; // +0x80
  uint8_t pad_084[0x18];
  rex::be<int32_t> player_index; // +0x9C
  rex::be<int32_t> user_index; // +0xA0
  uint8_t pad_0A4[0x18];
  rex::be<int32_t> selected_packet; // +0xBC
  rex::be<int32_t> held_seed; // +0xC0
  uint8_t zombie_side; // +0xC4
  uint8_t flag_c5; // +0xC5
  uint8_t pad_0C6[0x2];
  rex::be<int32_t> field_c8; // +0xC8
  rex::be<int32_t> counter_cc; // +0xCC
  uint8_t flag_d0; // +0xD0
  uint8_t pad_0D1[0x13];
  rex::be<int32_t> counter_e4; // +0xE4
  uint8_t pad_0E8[0x8];
  GuestPtr field_f0; // +0xF0
  rex::be<uint32_t> butter_target_id; // +0xF4
};
static_assert(std::is_standard_layout_v<GamepadControls>);
static_assert(offsetof(GamepadControls, vtable) == 0x0);
static_assert(offsetof(GamepadControls, listener_vtable) == 0x4);
static_assert(offsetof(GamepadControls, app) == 0x20);
static_assert(offsetof(GamepadControls, board) == 0x44);
static_assert(offsetof(GamepadControls, flag_48) == 0x48);
static_assert(offsetof(GamepadControls, vector_50) == 0x50);
static_assert(offsetof(GamepadControls, cursor_x) == 0x58);
static_assert(offsetof(GamepadControls, cursor_y) == 0x5C);
static_assert(offsetof(GamepadControls, state) == 0x68);
static_assert(offsetof(GamepadControls, state_elapsed) == 0x70);
static_assert(offsetof(GamepadControls, target_x) == 0x74);
static_assert(offsetof(GamepadControls, target_y) == 0x78);
static_assert(offsetof(GamepadControls, delta_x) == 0x7C);
static_assert(offsetof(GamepadControls, delta_y) == 0x80);
static_assert(offsetof(GamepadControls, player_index) == 0x9C);
static_assert(offsetof(GamepadControls, user_index) == 0xA0);
static_assert(offsetof(GamepadControls, selected_packet) == 0xBC);
static_assert(offsetof(GamepadControls, held_seed) == 0xC0);
static_assert(offsetof(GamepadControls, zombie_side) == 0xC4);
static_assert(offsetof(GamepadControls, flag_c5) == 0xC5);
static_assert(offsetof(GamepadControls, field_c8) == 0xC8);
static_assert(offsetof(GamepadControls, counter_cc) == 0xCC);
static_assert(offsetof(GamepadControls, flag_d0) == 0xD0);
static_assert(offsetof(GamepadControls, counter_e4) == 0xE4);
static_assert(offsetof(GamepadControls, field_f0) == 0xF0);
static_assert(offsetof(GamepadControls, butter_target_id) == 0xF4);
static_assert(sizeof(GamepadControls) == 0xF8);
} // namespace pvz::xbox
