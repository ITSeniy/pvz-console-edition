// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
// Known prefix only. Derived widgets and WidgetManager have additional fields.
struct Widget {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x14];
  GuestPtr children_head; // +0x18
  uint8_t pad_01C[0x4];
  GuestPtr manager; // +0x20
  GuestPtr parent; // +0x24
  uint8_t flag_28; // +0x28
  uint8_t pad_029[0xB];
  rex::be<int32_t> update_count; // +0x34
  rex::be<int32_t> last_update_count; // +0x38
  uint8_t pad_03C[0x4];
  rex::be<int32_t> x; // +0x40
  rex::be<int32_t> y; // +0x44
  rex::be<int32_t> width; // +0x48
  rex::be<int32_t> height; // +0x4C
  uint8_t dirty; // +0x50
  uint8_t flag_51; // +0x51
  uint8_t pad_052[0x2];
  rex::be<int32_t> rect_54[2]; // +0x54
  uint8_t pad_05C[0xC];
  GuestPtr listener_vtable; // +0x68
  rex::be<int32_t> id; // +0x6C
  uint8_t visible; // +0x70
  uint8_t flag_71; // +0x71
  uint8_t disabled; // +0x72
  uint8_t flag_73; // +0x73
  uint8_t is_down; // +0x74
  uint8_t is_over; // +0x75
  uint8_t flag_76; // +0x76
  uint8_t pad_077[0x2B];
  uint8_t gamepad_focus; // +0xA2
  uint8_t pad_0A3[0x1];
  rex::be<uint32_t> field_a4; // +0xA4
  GuestPtr field_a8; // +0xA8
  GuestPtr field_ac; // +0xAC
  GuestPtr field_b0; // +0xB0
  GuestPtr navigation_1; // +0xB4
  GuestPtr field_b8; // +0xB8
  GuestPtr field_bc; // +0xBC
  rex::be<int32_t> user_index; // +0xC0
};
static_assert(std::is_standard_layout_v<Widget>);
static_assert(offsetof(Widget, vtable) == 0x0);
static_assert(offsetof(Widget, children_head) == 0x18);
static_assert(offsetof(Widget, manager) == 0x20);
static_assert(offsetof(Widget, parent) == 0x24);
static_assert(offsetof(Widget, flag_28) == 0x28);
static_assert(offsetof(Widget, update_count) == 0x34);
static_assert(offsetof(Widget, last_update_count) == 0x38);
static_assert(offsetof(Widget, x) == 0x40);
static_assert(offsetof(Widget, y) == 0x44);
static_assert(offsetof(Widget, width) == 0x48);
static_assert(offsetof(Widget, height) == 0x4C);
static_assert(offsetof(Widget, dirty) == 0x50);
static_assert(offsetof(Widget, flag_51) == 0x51);
static_assert(offsetof(Widget, rect_54) == 0x54);
static_assert(offsetof(Widget, listener_vtable) == 0x68);
static_assert(offsetof(Widget, id) == 0x6C);
static_assert(offsetof(Widget, visible) == 0x70);
static_assert(offsetof(Widget, flag_71) == 0x71);
static_assert(offsetof(Widget, disabled) == 0x72);
static_assert(offsetof(Widget, flag_73) == 0x73);
static_assert(offsetof(Widget, is_down) == 0x74);
static_assert(offsetof(Widget, is_over) == 0x75);
static_assert(offsetof(Widget, flag_76) == 0x76);
static_assert(offsetof(Widget, gamepad_focus) == 0xA2);
static_assert(offsetof(Widget, field_a4) == 0xA4);
static_assert(offsetof(Widget, field_a8) == 0xA8);
static_assert(offsetof(Widget, field_ac) == 0xAC);
static_assert(offsetof(Widget, field_b0) == 0xB0);
static_assert(offsetof(Widget, navigation_1) == 0xB4);
static_assert(offsetof(Widget, field_b8) == 0xB8);
static_assert(offsetof(Widget, field_bc) == 0xBC);
static_assert(offsetof(Widget, user_index) == 0xC0);
} // namespace pvz::xbox
