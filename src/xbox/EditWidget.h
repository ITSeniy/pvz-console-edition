// Xbox EditWidget fields, with instruction witnesses in xbox-layouts.csv (T17).
#pragma once
#include "Widget.h"
#include "DataObjects.h"
namespace pvz::xbox {
struct EditList {
  GuestPtr proxy;
  uint8_t unknown_004[0x10];
  GuestPtr sentinel;
  uint8_t unknown_018[4];
};
struct EditNode {
  GuestPtr next;
  GuestPtr previous;
  GuestPtr font;
};
struct EditProxy {
  GuestPtr owner;
};
struct EditWidget {
  Widget widget;
  DataString text;
  DataString masked_text;
  GuestPtr font;
  uint8_t unknown_110[0x10];
  EditList fonts;
  GuestPtr listener;
  uint8_t blink;
  uint8_t show_selection;
  uint8_t double_click;
  uint8_t unknown_143;
  rex::be<uint32_t> cursor;
  rex::be<uint32_t> anchor;
  rex::be<uint32_t> blink_counter;
  rex::be<int32_t> blink_delay;
  rex::be<uint32_t> scroll;
  rex::be<int32_t> max_chars;
  rex::be<int32_t> max_width;
  rex::be<uint16_t> password_char;
  uint8_t unknown_162[2];
  DataString undo_text;
  rex::be<uint32_t> undo_cursor;
  rex::be<uint32_t> undo_anchor;
  rex::be<uint32_t> undo_scroll;
};
struct EditApp {
  uint8_t unknown_000[0x406];
  uint8_t field_406;
};
struct EditColor {
  rex::be<uint32_t> rgba[4];
};
struct EditModifiers {
  uint8_t unknown_000[0x134];
  uint8_t shift;
  uint8_t control;
};
struct EditGlobalApp {
  uint8_t unknown_000[0x13D];
  uint8_t extended_characters;
};
static_assert(sizeof(EditList) == 0x1C);
static_assert(offsetof(EditList, proxy) == 0);
static_assert(offsetof(EditList, sentinel) == 0x14);
static_assert(offsetof(EditNode, next) == 0);
static_assert(offsetof(EditNode, previous) == 4);
static_assert(offsetof(EditNode, font) == 8);
static_assert(offsetof(EditProxy, owner) == 0);
static_assert(offsetof(EditWidget, text) == 0xD4);
static_assert(offsetof(EditWidget, masked_text) == 0xF0);
static_assert(offsetof(EditWidget, font) == 0x10C);
static_assert(offsetof(EditWidget, fonts) == 0x120);
static_assert(offsetof(EditWidget, listener) == 0x13C);
static_assert(offsetof(EditWidget, blink) == 0x140);
static_assert(offsetof(EditWidget, show_selection) == 0x141);
static_assert(offsetof(EditWidget, double_click) == 0x142);
static_assert(offsetof(EditWidget, cursor) == 0x144);
static_assert(offsetof(EditWidget, anchor) == 0x148);
static_assert(offsetof(EditWidget, blink_counter) == 0x14C);
static_assert(offsetof(EditWidget, blink_delay) == 0x150);
static_assert(offsetof(EditWidget, scroll) == 0x154);
static_assert(offsetof(EditWidget, max_chars) == 0x158);
static_assert(offsetof(EditWidget, max_width) == 0x15C);
static_assert(offsetof(EditWidget, password_char) == 0x160);
static_assert(offsetof(EditWidget, undo_text) == 0x164);
static_assert(offsetof(EditWidget, undo_cursor) == 0x180);
static_assert(offsetof(EditWidget, undo_anchor) == 0x184);
static_assert(offsetof(EditWidget, undo_scroll) == 0x188);
static_assert(sizeof(EditWidget) == 0x18C);
static_assert(offsetof(EditApp, field_406) == 0x406);
static_assert(offsetof(EditColor, rgba) == 0);
static_assert(offsetof(EditModifiers, shift) == 0x134);
static_assert(offsetof(EditModifiers, control) == 0x135);
static_assert(offsetof(EditGlobalApp, extended_characters) == 0x13D);
} // namespace pvz::xbox
