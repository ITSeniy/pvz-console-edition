// Xbox prefix, not a host STL container. Evidence: T15, xbox-layouts.csv.
#pragma once
#include "Types.h"
namespace pvz::xbox {
struct WidgetContainer {
  GuestPtr vtable;
  GuestPtr list_proxy; // +4, checked iterator owner
  uint8_t pad_008[0x10];
  GuestPtr children_head; // +18, circular list sentinel
  rex::be<uint32_t> children_count; // +1C
  GuestPtr manager; // +20
  GuestPtr parent; // +24
  uint8_t iterator_changed; // +28
  uint8_t pad_029[3];
  GuestPtr iterator_owner; // +2C
  GuestPtr iterator_node; // +30
  rex::be<uint32_t> last_manager_update_count; // +34
  rex::be<uint32_t> update_count; // +38
  uint8_t dirty; // +3C
  uint8_t pad_03d[3];
  rex::be<int32_t> x,y,width,height; // +40..4C
  uint8_t flag_50; // +50, dirty-full branch when nonzero
  uint8_t flag_51; // +51
  uint8_t pad_052[2];
  rex::be<uint32_t> flags_add; // +54
  rex::be<uint32_t> flags_remove; // +58
  rex::be<uint32_t> field_5c,field_60,field_64;
};
struct WidgetUpdateContext {
  rex::be<uint32_t> modal_flags; // +0
  rex::be<uint32_t> flags; // +4
  uint8_t is_modal; // +8
};
struct WidgetListNode {GuestPtr next,previous,widget;};
static_assert(offsetof(WidgetContainer,children_head)==0x18);
static_assert(offsetof(WidgetContainer,iterator_owner)==0x2C);
static_assert(offsetof(WidgetContainer,update_count)==0x38);
static_assert(offsetof(WidgetContainer,dirty)==0x3C);
static_assert(offsetof(WidgetContainer,flags_add)==0x54);
static_assert(offsetof(WidgetContainer,field_64)==0x64);
static_assert(sizeof(WidgetContainer)==0x68);
static_assert(sizeof(WidgetListNode)==12);
} // namespace pvz::xbox
