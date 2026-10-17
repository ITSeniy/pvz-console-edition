// Xbox 360 584109FF / 0.0.1.4. Evidence: docs/native-port/data/xbox-layouts.csv.
// Declarative guest bytes only: no host pointers, methods, ownership or vptr.
#pragma once
#include "Types.h"

namespace pvz::xbox {
struct CursorObject {
  GuestPtr vtable; // +0x0
  uint8_t pad_004[0x18];
  GuestPtr app; // +0x1C
  GuestPtr board; // +0x20
  rex::be<int32_t> x; // +0x24
  rex::be<int32_t> y; // +0x28
  rex::be<int32_t> width; // +0x2C
  rex::be<int32_t> height; // +0x30
  uint8_t pad_034[0xC];
  rex::be<int32_t> field_40; // +0x40
  rex::be<int32_t> seed_type; // +0x44
  rex::be<int32_t> imitater_type; // +0x48
  rex::be<int32_t> cursor_type; // +0x4C
  rex::be<int32_t> field_50; // +0x50
  rex::be<int32_t> field_54; // +0x54
  rex::be<int32_t> field_58; // +0x58
  rex::be<int32_t> field_5c; // +0x5C
  uint8_t pad_060[0x4];
  rex::be<uint32_t> reanimation_id; // +0x64
};
static_assert(std::is_standard_layout_v<CursorObject>);
static_assert(offsetof(CursorObject, vtable) == 0x0);
static_assert(offsetof(CursorObject, app) == 0x1C);
static_assert(offsetof(CursorObject, board) == 0x20);
static_assert(offsetof(CursorObject, x) == 0x24);
static_assert(offsetof(CursorObject, y) == 0x28);
static_assert(offsetof(CursorObject, width) == 0x2C);
static_assert(offsetof(CursorObject, height) == 0x30);
static_assert(offsetof(CursorObject, field_40) == 0x40);
static_assert(offsetof(CursorObject, seed_type) == 0x44);
static_assert(offsetof(CursorObject, imitater_type) == 0x48);
static_assert(offsetof(CursorObject, cursor_type) == 0x4C);
static_assert(offsetof(CursorObject, field_50) == 0x50);
static_assert(offsetof(CursorObject, field_54) == 0x54);
static_assert(offsetof(CursorObject, field_58) == 0x58);
static_assert(offsetof(CursorObject, field_5c) == 0x5C);
static_assert(offsetof(CursorObject, reanimation_id) == 0x64);
static_assert(sizeof(CursorObject) == 0x68);
} // namespace pvz::xbox
