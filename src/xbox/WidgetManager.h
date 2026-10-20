// Partial Xbox flag state established by 824A5A88 and 824A60F0.
#pragma once
#include "Types.h"
#include "WidgetContainer.h"
#include "DataObjects.h"
#include <cstddef>

namespace pvz::xbox {
struct WidgetManagerFlags {
  WidgetContainer container;
  GuestPtr field_68;
  uint8_t pad_06C[4];
  GuestPtr app;
  GuestPtr field_74;
  uint8_t pad_078[4];
  uint8_t flag_7C;
  uint8_t pad_07D[3];
  GuestPtr field_80;
  DataVector vector_84;
  rex::be<int32_t> field_9C;
  uint8_t current_mode;
  uint8_t pad_0A1[3];
  GuestPtr focus;
  GuestPtr gamepad_focus;
  GuestPtr mouse_down;
  GuestPtr mouse_over;
  GuestPtr modal_widget;
  rex::be<uint32_t> flags_add;
  rex::be<uint32_t> flags_remove;
  rex::be<uint32_t> modal_flags_add;
  rex::be<uint32_t> modal_flags_remove;
  rex::be<uint32_t> field_C8;
  rex::be<uint32_t> field_CC;
  DataVector vector_D0;
  rex::be<int32_t> rect_E8[4];
  rex::be<int32_t> rect_F8[4];
  uint8_t flag_108;
  uint8_t pad_109[0xB];
  rex::be<uint32_t> pressed_buttons;
  rex::be<uint32_t> update_flags;
  rex::be<uint32_t> last_key_update;
  rex::be<uint32_t> field_120;
  uint8_t key_states[255];
  uint8_t pad_223;
  rex::be<uint32_t> field_224;
  rex::be<uint32_t> current_flags;
};
static_assert(offsetof(WidgetManagerFlags, current_mode) == 0xA0);
static_assert(offsetof(WidgetManagerFlags, modal_widget) == 0xB4);
static_assert(offsetof(WidgetManagerFlags, flags_add) == 0xB8);
static_assert(offsetof(WidgetManagerFlags, flags_remove) == 0xBC);
static_assert(offsetof(WidgetManagerFlags, modal_flags_add) == 0xC0);
static_assert(offsetof(WidgetManagerFlags, modal_flags_remove) == 0xC4);
static_assert(offsetof(WidgetManagerFlags, current_flags) == 0x228);
static_assert(offsetof(WidgetManagerFlags, app) == 0x70);
static_assert(offsetof(WidgetManagerFlags, vector_84) == 0x84);
static_assert(offsetof(WidgetManagerFlags, field_9C) == 0x9C);
static_assert(offsetof(WidgetManagerFlags, focus) == 0xA4);
static_assert(offsetof(WidgetManagerFlags, gamepad_focus) == 0xA8);
static_assert(offsetof(WidgetManagerFlags, mouse_down) == 0xAC);
static_assert(offsetof(WidgetManagerFlags, mouse_over) == 0xB0);
static_assert(offsetof(WidgetManagerFlags, vector_D0) == 0xD0);
static_assert(offsetof(WidgetManagerFlags, rect_E8) == 0xE8);
static_assert(offsetof(WidgetManagerFlags, rect_F8) == 0xF8);
static_assert(offsetof(WidgetManagerFlags, flag_108) == 0x108);
static_assert(offsetof(WidgetManagerFlags, pressed_buttons) == 0x114);
static_assert(offsetof(WidgetManagerFlags, update_flags) == 0x118);
static_assert(offsetof(WidgetManagerFlags, last_key_update) == 0x11C);
static_assert(offsetof(WidgetManagerFlags, field_120) == 0x120);
static_assert(offsetof(WidgetManagerFlags, key_states) == 0x124);
static_assert(offsetof(WidgetManagerFlags, field_224) == 0x224);
static_assert(sizeof(WidgetManagerFlags) == 0x22C);
} // namespace pvz::xbox
