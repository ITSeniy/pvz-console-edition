// Xbox 584109FF / 0.0.1.4. T17: 824A8B28 ctor and 824A8260/824A96D8 uses.
#pragma once
#include "Widget.h"
#include "DataObjects.h"
namespace pvz::xbox {
struct Dialog {
  Widget widget;
  GuestPtr button_listener_vtable; // D4: secondary interface, callback this points here
  rex::be<uint32_t> field_d8;
  GuestPtr listener;
  GuestPtr background;
  GuestPtr yes_button;
  GuestPtr no_button;
  GuestPtr field_ec;
  DataString header;
  DataString footer;
  DataString body;
  rex::be<int32_t> button_mode;
  GuestPtr header_font;
  GuestPtr body_font;
  rex::be<uint32_t> field_150;
  rex::be<uint32_t> field_154;
  rex::be<uint32_t> button_height;
  rex::be<uint32_t> background_insets[4];
  rex::be<uint32_t> content_insets[4];
  rex::be<uint32_t> header_spacing;
  uint8_t dragging;
  uint8_t unknown_181[3];
  rex::be<uint32_t> drag_x;
  rex::be<uint32_t> drag_y;
  uint8_t modal;
  uint8_t unknown_18d[3];
  rex::be<uint32_t> result;
  rex::be<uint32_t> field_194;
  rex::be<uint32_t> button_spacing;
  rex::be<uint32_t> alpha;
  uint8_t waiting;
};
// Partial views accessed by Dialog; allocation sizes belong to their own classes.
struct DialogButtonMetrics {
  Widget widget;
  DataString label;
  uint8_t unknown_f0[4];
  GuestPtr font;
};
struct DialogWidgetFlags {
  uint8_t unknown_000[0x5C];
  rex::be<uint32_t> field_5c;
};
struct DialogAppState {
  uint8_t unknown_000[0x800];
  uint8_t field_800;
};
static_assert(std::is_standard_layout_v<Dialog>);
static_assert(offsetof(Dialog, button_listener_vtable) == 0xD4);
static_assert(offsetof(Dialog, field_d8) == 0xD8);
static_assert(offsetof(Dialog, listener) == 0xDC);
static_assert(offsetof(Dialog, background) == 0xE0);
static_assert(offsetof(Dialog, yes_button) == 0xE4);
static_assert(offsetof(Dialog, no_button) == 0xE8);
static_assert(offsetof(Dialog, field_ec) == 0xEC);
static_assert(offsetof(Dialog, header) == 0xF0);
static_assert(offsetof(Dialog, footer) == 0x10C);
static_assert(offsetof(Dialog, body) == 0x128);
static_assert(offsetof(Dialog, button_mode) == 0x144);
static_assert(offsetof(Dialog, header_font) == 0x148);
static_assert(offsetof(Dialog, body_font) == 0x14C);
static_assert(offsetof(Dialog, field_150) == 0x150);
static_assert(offsetof(Dialog, field_154) == 0x154);
static_assert(offsetof(Dialog, button_height) == 0x158);
static_assert(offsetof(Dialog, background_insets) == 0x15C);
static_assert(offsetof(Dialog, content_insets) == 0x16C);
static_assert(offsetof(Dialog, header_spacing) == 0x17C);
static_assert(offsetof(Dialog, dragging) == 0x180);
static_assert(offsetof(Dialog, drag_x) == 0x184);
static_assert(offsetof(Dialog, drag_y) == 0x188);
static_assert(offsetof(Dialog, modal) == 0x18C);
static_assert(offsetof(Dialog, result) == 0x190);
static_assert(offsetof(Dialog, field_194) == 0x194);
static_assert(offsetof(Dialog, button_spacing) == 0x198);
static_assert(offsetof(Dialog, alpha) == 0x19C);
static_assert(offsetof(Dialog, waiting) == 0x1A0);
static_assert(sizeof(Dialog) == 0x1A4);
static_assert(offsetof(DialogButtonMetrics, label) == 0xD4);
static_assert(offsetof(DialogButtonMetrics, font) == 0xF4);
static_assert(offsetof(DialogWidgetFlags, field_5c) == 0x5C);
static_assert(offsetof(DialogAppState, field_800) == 0x800);
} // namespace pvz::xbox
