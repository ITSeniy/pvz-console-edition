// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
#pragma once
#include "Widget.h"

namespace pvz::xbox {
// Xbox field offsets, not host object layout. The vtable-bearing ButtonListener
// subobject follows Widget at +0xD4.
struct ScrollbarWidget {
  Widget widget;
  GuestPtr button_listener_vtable; // +0xD4
  GuestPtr up_button; // +0xD8
  GuestPtr down_button; // +0xDC
  uint8_t invis_if_no_scroll; // +0xE0
  uint8_t pad_0E1[7];
  rex::be<double> value; // +0xE8
  rex::be<double> max_value; // +0xF0
  rex::be<double> page_size; // +0xF8
  uint8_t horizontal; // +0x100
  uint8_t pad_101[3];
  rex::be<int32_t> field_104; // +0x104
  uint8_t pressed_on_thumb; // +0x108
  uint8_t pad_109[3];
  rex::be<int32_t> thumb_position; // +0x10C
  rex::be<int32_t> mouse_down_x; // +0x110
  rex::be<int32_t> mouse_down_y; // +0x114
  rex::be<int32_t> update_mode; // +0x118
  rex::be<int32_t> update_acc; // +0x11C
  rex::be<int32_t> button_acc; // +0x120
  rex::be<int32_t> last_mouse_x; // +0x124
  rex::be<int32_t> last_mouse_y; // +0x128
  GuestPtr scroll_listener; // +0x12C
  rex::be<uint32_t> field_130;
  rex::be<uint32_t> field_134;
  rex::be<uint32_t> field_138;
};

// Scrollbar child fields touched by Xbox helpers. Other bytes are omitted.
struct ScrollbuttonWidget {
  uint8_t pad_000[0x48];
  rex::be<int32_t> width; // +0x48
  uint8_t pad_04C[0xAC];
  rex::be<uint32_t> field_F8; // +0xF8
  uint8_t pad_0FC[4];
  rex::be<uint32_t> field_100; // +0x100
  rex::be<uint32_t> field_104; // +0x104
  uint8_t pad_108[0x68];
  uint8_t horizontal; // +0x170
};
static_assert(offsetof(ScrollbarWidget, button_listener_vtable) == 0xD4);
static_assert(offsetof(ScrollbarWidget, up_button) == 0xD8);
static_assert(offsetof(ScrollbarWidget, down_button) == 0xDC);
static_assert(offsetof(ScrollbarWidget, invis_if_no_scroll) == 0xE0);
static_assert(offsetof(ScrollbarWidget, value) == 0xE8);
static_assert(offsetof(ScrollbarWidget, max_value) == 0xF0);
static_assert(offsetof(ScrollbarWidget, page_size) == 0xF8);
static_assert(offsetof(ScrollbarWidget, horizontal) == 0x100);
static_assert(offsetof(ScrollbarWidget, field_104) == 0x104);
static_assert(offsetof(ScrollbarWidget, pressed_on_thumb) == 0x108);
static_assert(offsetof(ScrollbarWidget, thumb_position) == 0x10C);
static_assert(offsetof(ScrollbarWidget, mouse_down_x) == 0x110);
static_assert(offsetof(ScrollbarWidget, mouse_down_y) == 0x114);
static_assert(offsetof(ScrollbarWidget, update_mode) == 0x118);
static_assert(offsetof(ScrollbarWidget, update_acc) == 0x11C);
static_assert(offsetof(ScrollbarWidget, button_acc) == 0x120);
static_assert(offsetof(ScrollbarWidget, last_mouse_x) == 0x124);
static_assert(offsetof(ScrollbarWidget, last_mouse_y) == 0x128);
static_assert(offsetof(ScrollbarWidget, scroll_listener) == 0x12C);
static_assert(offsetof(ScrollbarWidget, field_130) == 0x130);
static_assert(offsetof(ScrollbarWidget, field_134) == 0x134);
static_assert(offsetof(ScrollbarWidget, field_138) == 0x138);
static_assert(offsetof(ScrollbuttonWidget, width) == 0x48);
static_assert(offsetof(ScrollbuttonWidget, field_F8) == 0xF8);
static_assert(offsetof(ScrollbuttonWidget, field_100) == 0x100);
static_assert(offsetof(ScrollbuttonWidget, field_104) == 0x104);
static_assert(offsetof(ScrollbuttonWidget, horizontal) == 0x170);
} // namespace pvz::xbox
