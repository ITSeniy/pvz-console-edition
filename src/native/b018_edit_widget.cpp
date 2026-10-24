// B018. Xbox 584109FF / 0.0.1.4 translations; T17 evidence manifest.
#include "native/replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/EditWidget.h"
#include <array>
#ifndef PVZ_BATCH_SYNTHETIC_TEST
#include "native/guest_call.h"
PVZ_NATIVE_BATCH(b018)
REX_EXTERN(sub_82125760);
REX_EXTERN(sub_821257D8);
REX_EXTERN(sub_82125E30);
REX_EXTERN(sub_82125F60);
REX_EXTERN(sub_821264E0);
REX_EXTERN(sub_821269C0);
REX_EXTERN(sub_82181308);
REX_EXTERN(sub_821C6DC8);
REX_EXTERN(sub_824729F8);
REX_EXTERN(sub_82472BD8);
REX_EXTERN(sub_82472BE8);
REX_EXTERN(sub_82472BF8);
REX_EXTERN(sub_82472C08);
REX_EXTERN(sub_82472C10);
REX_EXTERN(sub_82472FE8);
REX_EXTERN(sub_82472FF0);
REX_EXTERN(sub_82473040);
REX_EXTERN(sub_824730D0);
REX_EXTERN(sub_824730D8);
REX_EXTERN(sub_82473C60);
REX_EXTERN(sub_82473D68);
REX_EXTERN(sub_82473E88);
REX_EXTERN(sub_82474808);
REX_EXTERN(sub_82475EB0);
REX_EXTERN(sub_82475EB8);
REX_EXTERN(sub_8247D790);
REX_EXTERN(sub_82487DD8);
REX_EXTERN(sub_82489318);
REX_EXTERN(sub_82491C48);
REX_EXTERN(sub_82491C78);
REX_EXTERN(sub_82491D80);
REX_EXTERN(sub_82495870);
REX_EXTERN(sub_82496278);
REX_EXTERN(sub_8249A0B8);
REX_EXTERN(sub_8249A0C0);
REX_EXTERN(sub_8249B1A0);
REX_EXTERN(sub_8249B650);
REX_EXTERN(sub_8249B730);
REX_EXTERN(sub_8249C6A8);
REX_EXTERN(sub_8249DDB0);
REX_EXTERN(sub_824AF830);
REX_EXTERN(sub_824AFAC8);
REX_EXTERN(sub_824AFD08);
REX_EXTERN(sub_824B0420);
REX_EXTERN(sub_824B2298);
REX_EXTERN(sub_824BD3B8);
REX_EXTERN(sub_824C41D8);
REX_EXTERN(sub_824DD5E0);
REX_EXTERN(sub_82564B10);
REX_EXTERN(sub_825DDB78);
REX_EXTERN(sub_825F8F50);
REXCVAR_DECLARE(bool, pvz_native_edit_824AFDD0_inject_error);
#endif
namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 0> noOutputs{};
const std::array r3out{RegisterOutput{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}};
const auto NoRanges = [](const PPCContext&) { return std::array<Range, 0>{}; };
void True(PPCContext& c, uint8_t*) {
  c.r3.u64 = 1;
}
void WordCharacter(PPCContext& c, uint8_t*) {
  const uint16_t ch = c.r4.u16;
  c.r3.u64 = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
             (ch >= 0xC0 && ch <= 0xFF) || ch == '_';
}
#ifndef PVZ_BATCH_SYNTHETIC_TEST
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
using pvz::native::Virtual;
auto& E(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::EditWidget*>(b + p);
}
auto& W(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::Widget*>(b + p);
}
auto& Str(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::DataString*>(b + p);
}
auto& Node(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::EditNode*>(b + p);
}
void V(PPCContext& c, uint8_t* b, uint32_t object, uint32_t slot, uint32_t lr,
       std::initializer_list<uint32_t> args) {
  Args(c, args);
  Virtual(c, b, object, slot, lr);
}
uint32_t App(uint8_t* b, uint32_t self) {
  return reinterpret_cast<pvz::xbox::WidgetManagerInput*>(b + uint32_t(W(b, self).manager))->app;
}
void Native_824AF2A0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_824729F8, 0x824AF2B8, {self});
  V(c, b, self, 0x134, 0x824AF2D0, {self, 0});
}
void Native_824AF310(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82472BD8, 0x824AF328, {self});
  if (W(b, self).manager && reinterpret_cast<pvz::xbox::EditApp*>(b + App(b, self))->field_406)
    Call(c, b, sub_82491D80, 0x824AF350, {c.r1.u32 + 0x50, self});
  E(b, self).blink = 1;
  E(b, self).blink_counter = 0;
  V(c, b, self, 0x10, 0x824AF374, {self});
}
void Native_824AF388(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82472BE8, 0x824AF3A0, {self});
  E(b, self).blink = 0;
  V(c, b, self, 0x10, 0x824AF3BC, {self});
}
void Native_824AF3D0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82472BF8, 0x824AF3E8, {self});
  if (!W(b, self).flag_73)
    return;
  if (reinterpret_cast<pvz::xbox::EditApp*>(b + App(b, self))->field_406)
    Call(c, b, sub_82491D80, 0x824AF414, {c.r1.u32 + 0x50, self});
  const uint32_t count = uint32_t(E(b, self).blink_counter) + 1;
  E(b, self).blink_counter = count;
  if (int32_t(count) > int32_t(E(b, self).blink_delay)) {
    V(c, b, self, 0x10, 0x824AF440, {self});
    E(b, self).blink_counter = 0;
    E(b, self).blink = !E(b, self).blink;
  }
}
void Native_824AF4D0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, key = c.r4.u32;
  Frame f(c, b, 0x70);
  if (!(int32_t(key) >= 0x41 && int32_t(key) < 0x5A)) {
    const uint32_t listener = E(b, self).listener;
    V(c, b, listener, 4, 0x824AF518, {listener, uint32_t(W(b, self).id), key});
    if (c.r3.u8)
      V(c, b, self, 0x11C, 0x824AF53C, {self, key, 0});
  }
  Call(c, b, sub_82472C10, 0x824AF548, {self, key});
}
void Native_824AF560(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, ch = c.r4.u32;
  Frame f(c, b, 0x70);
  V(c, b, self, 0x11C, 0x824AF594, {self, 0, ch});
  Call(c, b, sub_82472C08, 0x824AF5A0, {self, ch});
}
void Native_824AF5B8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, clicks = c.r6.u32, button = c.r7.u32;
  Frame f(c, b, 0x80);
  Call(c, b, sub_82473040, 0x824AF5D8, {self, x, y, clicks, button});
  E(b, self).anchor = UINT32_MAX;
  V(c, b, self, 0x130, 0x824AF5FC, {self, x, y});
  E(b, self).cursor = c.r3.u32;
  if (int32_t(button) > 1) {
    E(b, self).double_click = 1;
    V(c, b, self, 0x120, 0x824AF624, {self});
  }
  V(c, b, self, 0x10, 0x824AF638, {self});
  V(c, b, self, 0x134, 0x824AF650, {self, 0});
}
void Native_824AF658(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, clicks = c.r6.u32, button = c.r7.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824730D0, 0x824AF674, {self, x, y, clicks, button});
  if (uint32_t(E(b, self).anchor) == uint32_t(E(b, self).cursor))
    E(b, self).anchor = UINT32_MAX;
  if (E(b, self).double_click) {
    E(b, self).anchor = UINT32_MAX;
    V(c, b, self, 0x130, 0x824AF6B8, {self, x, y});
    E(b, self).cursor = c.r3.u32;
    E(b, self).double_click = 0;
    V(c, b, self, 0x120, 0x824AF6D8, {self});
  }
  V(c, b, self, 0x10, 0x824AF6EC, {self});
}
void Native_824AF6F8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824730D8, 0x824AF714, {self, x, y});
  if (uint32_t(E(b, self).anchor) == UINT32_MAX)
    E(b, self).anchor = uint32_t(E(b, self).cursor);
  V(c, b, self, 0x130, 0x824AF744, {self, x, y});
  E(b, self).cursor = c.r3.u32;
  V(c, b, self, 0x10, 0x824AF75C, {self});
  V(c, b, self, 0x134, 0x824AF774, {self, 0});
}
template <bool Enter> void MouseBoundary(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  if constexpr (Enter)
    Call(c, b, sub_82472FE8, 0x824AF798, {self});
  else
    Call(c, b, sub_82472FF0, 0x824AF7D8, {self});
  Call(c, b, sub_82475EB8, Enter ? 0x824AF7A8 : 0x824AF7E8, {App(b, self), Enter ? 3u : 0u});
}
void Native_824AF800(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, begin = W(b, self).colors_begin;
  if ((int32_t(uint32_t(W(b, self).colors_end) - begin) >> 4) == 0)
    ppc_trap(c, b, 22);
  if (uint32_t(reinterpret_cast<pvz::xbox::EditColor*>(b + begin)->rgba[3]) == 255)
    Call(c, b, sub_82491C48, uint32_t(c.lr), {});
  else
    Call(c, b, sub_82491C78, uint32_t(c.lr), {});
}
// Debug-iterator checks use the list proxy, including after destructor callbacks.
uint32_t CheckedSentinel(PPCContext& c, uint8_t* b, uint32_t proxy, uint32_t node) {
  if (!proxy)
    ppc_trap(c, b, 22);
  const uint32_t owner = reinterpret_cast<pvz::xbox::EditProxy*>(b + proxy)->owner;
  const uint32_t sentinel = reinterpret_cast<pvz::xbox::EditList*>(b + owner)->sentinel;
  if (node == sentinel)
    ppc_trap(c, b, 22);
  return sentinel;
}
void Native_824AF830(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  const uint32_t proxy = E(b, self).fonts.proxy;
  uint32_t node = Node(b, E(b, self).fonts.sentinel).next;
  while (true) {
    if (!proxy || proxy != uint32_t(E(b, self).fonts.proxy))
      ppc_trap(c, b, 22);
    if (node == uint32_t(E(b, self).fonts.sentinel))
      break;
    CheckedSentinel(c, b, proxy, node);
    if (const uint32_t font = Node(b, node).font)
      V(c, b, font, 0, 0x824AF8B0, {font, 1});
    CheckedSentinel(c, b, proxy, node);
    node = Node(b, node).next;
  }
  Call(c, b, sub_824C41D8, 0x824AF8F0, {self + 0x120});
}
void Native_824AF900(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, id = c.r4.u32, listener = c.r5.u32;
  Frame f(c, b, 0x80);
  pvz::guest::Write(b, c.r1.u32 + 0x94, self);
  Call(c, b, sub_82473E88, 0x824AF924, {self});
  W(b, self).vtable = 0x820654B4;
  W(b, self).listener_vtable = 0x8206547C;
  Call(c, b, sub_82125760, 0x824AF94C, {self + 0xD4, 0, 0});
  Call(c, b, sub_82125760, 0x824AF95C, {self + 0xF0, 0, 0});
  Call(c, b, sub_82489318, 0x824AF964, {self + 0x120});
  Call(c, b, sub_82125760, 0x824AF974, {self + 0x164, 0, 0});
  auto& e = E(b, self);
  W(b, self).id = id;
  e.listener = listener;
  e.font = 0;
  e.double_click = 0;
  e.anchor = UINT32_MAX;
  e.scroll = 0;
  e.undo_cursor = 0;
  e.undo_anchor = 0;
  e.undo_scroll = 0;
  e.blink_counter = 0;
  e.cursor = 0;
  e.blink = 0;
  e.show_selection = 0;
  e.max_chars = -1;
  e.max_width = -1;
  e.password_char = 0;
  e.blink_delay = 40;
  Call(c, b, sub_82473D68, 0x824AF9D8, {self, 0x827D2F14, 5});
  c.r3.u64 = self;
}
void Native_824AFAC8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  pvz::guest::Write(b, c.r1.u32 + 0x84, self);
  W(b, self).vtable = 0x820654B4;
  W(b, self).listener_vtable = 0x8206547C;
  if (const uint32_t font = E(b, self).font)
    V(c, b, font, 0, 0x824AFB20, {font, 1});
  Call(c, b, sub_824AF830, 0x824AFB28, {self});
  Call(c, b, sub_82125760, 0x824AFB38, {self + 0x164, 1, 0});
  Call(c, b, sub_824C41D8, 0x824AFB40, {self + 0x120});
  Call(c, b, sub_825F8F50, 0x824AFB48, {uint32_t(E(b, self).fonts.sentinel)});
  const uint32_t proxy = E(b, self).fonts.proxy;
  E(b, self).fonts.sentinel = 0;
  Call(c, b, sub_825F8F50, 0x824AFB58, {proxy});
  Call(c, b, sub_82125760, 0x824AFB68, {self + 0xF0, 1, 0});
  Call(c, b, sub_82125760, 0x824AFB78, {self + 0xD4, 1, 0});
  Call(c, b, sub_82473C60, 0x824AFB80, {self});
}
void Native_824AFC70(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, text = c.r4.u32;
  const uint8_t reset = c.r5.u8;
  Frame f(c, b, 0x70);
  Call(c, b, sub_821257D8, 0x824AFC9C, {self + 0xD4, text, 0, UINT32_MAX});
  E(b, self).anchor = 0;
  E(b, self).cursor = uint32_t(E(b, self).text.length);
  if (reset)
    E(b, self).scroll = 0;
  else
    V(c, b, self, 0x134, 0x824AFCD4, {self, 1});
  V(c, b, self, 0x10, 0x824AFCE8, {self});
}
void Native_824AFD08(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0xB0);
  const uint32_t sp = c.r1.u32;
  const uint16_t ch = E(b, self).password_char;
  if (!ch) {
    c.r3.u64 = self + 0xD4;
    return;
  }
  const uint32_t length = E(b, self).text.length;
  pvz::guest::Write(b, sp + 0x50, length);
  if (uint32_t(E(b, self).masked_text.length) != length) {
    pvz::guest::Write(b, sp + 0x50, length);
    *reinterpret_cast<rex::be<uint16_t>*>(b + sp + 0x50) = ch;
    Call(c, b, sub_82125760, 0x824AFD5C, {sp + 0x60, 0, 0});
    Call(c, b, sub_82474808, 0x824AFD6C, {sp + 0x60, length, ch});
    Call(c, b, sub_821257D8, 0x824AFD80, {self + 0xF0, sp + 0x60, 0, UINT32_MAX});
    Call(c, b, sub_82125760, 0x824AFD90, {sp + 0x60, 1, 0});
  }
  c.r3.u64 = self + 0xF0;
}
void Native_824B2298(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, font = c.r4.u32, selector = c.r5.u32;
  Frame f(c, b, 0x90);
  const uint32_t sp = c.r1.u32, head = E(b, self).fonts.sentinel;
  pvz::guest::Write(b, sp + 0x50, 0);
  pvz::guest::Write(b, sp + 0x54, 0);
  Call(c, b, sub_82487DD8, 0x824B22D4, {self + 0x120, head, uint32_t(Node(b, head).previous), sp + 0x50});
  const uint32_t node = c.r3.u32;
  Call(c, b, sub_824BD3B8, 0x824B22E4, {self + 0x120, 1});
  Node(b, head).previous = node;
  Node(b, Node(b, node).previous).next = node;
  Call(c, b, sub_82564B10, 0x824B22F8, {self + 0x120});
  const uint32_t entry = c.r3.u32;
  pvz::guest::Write(b, entry + 4, selector);
  V(c, b, font, 0x2C, 0x824B2314, {font});
  pvz::guest::Write(b, entry, c.r3.u32);
}
void Native_824B2320(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, font = c.r4.u32, extra = c.r5.u32;
  Frame f(c, b, 0x70);
  if (const uint32_t old = E(b, self).font)
    V(c, b, old, 0, 0x824B2358, {old, 1});
  V(c, b, font, 0x2C, 0x824B236C, {font});
  E(b, self).font = c.r3.u32;
  Call(c, b, sub_824AF830, 0x824B2378, {self});
  if (extra)
    Call(c, b, sub_824B2298, 0x824B2390, {self, extra, UINT32_MAX});
}
void Native_824B23D8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824AFAC8, 0x824B23F8, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x824B2408, {self});
  c.r3.u64 = self;
}
uint16_t Character(PPCContext& c, uint8_t* b, uint32_t text, uint32_t index) {
  if (index > uint32_t(Str(b, text).length))
    ppc_trap(c, b, 22);
  const uint32_t data = uint32_t(Str(b, text).capacity) < 8 ? text + 4 : pvz::guest::Read(b, text + 4);
  return *reinterpret_cast<rex::be<uint16_t>*>(b + data + 2 * index);
}
void Native_824B2150(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824AFD08, 0x824B216C, {self});
  const uint32_t text = c.r3.u32;
  if (int32_t(E(b, self).cursor) >= int32_t(Str(b, text).length))
    return;
  E(b, self).anchor = uint32_t(E(b, self).cursor);
  while (int32_t(E(b, self).anchor) > 0) {
    const uint16_t ch = Character(c, b, text, uint32_t(E(b, self).anchor) - 1);
    V(c, b, self, 0x12C, 0x824B21D4, {self, ch});
    if (!c.r3.u8)
      break;
    E(b, self).anchor = uint32_t(E(b, self).anchor) - 1;
  }
  while (int32_t(E(b, self).cursor) < int32_t(uint32_t(Str(b, text).length) - 1)) {
    const uint16_t ch = Character(c, b, text, uint32_t(E(b, self).cursor) + 1);
    V(c, b, self, 0x12C, 0x824B2240, {self, ch});
    if (!c.r3.u8)
      break;
    E(b, self).cursor = uint32_t(E(b, self).cursor) + 1;
  }
  if (int32_t(E(b, self).cursor) < int32_t(Str(b, text).length))
    E(b, self).cursor = uint32_t(E(b, self).cursor) + 1;
}
void Native_824B1DB0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32;
  Frame f(c, b, 0xD0);
  const uint32_t sp = c.r1.u32;
  Call(c, b, sub_824AFD08, 0x824B1DD0, {self});
  const uint32_t text = c.r3.u32;
  uint32_t result = 0;
  for (uint32_t i = E(b, self).scroll; int32_t(i) < int32_t(Str(b, text).length); ++i) {
    uint32_t start = E(b, self).scroll;
    Call(c, b, sub_82181308, 0x824B1DF8, {sp + 0x70, text, start, i - start});
    start = E(b, self).scroll;
    Call(c, b, sub_82181308, 0x824B1E10, {sp + 0x50, text, start, i - start + 1});
    uint32_t font = E(b, self).font;
    V(c, b, font, 0x1C, 0x824B1E28, {font, sp + 0x70});
    const uint32_t before = c.r3.u32;
    font = E(b, self).font;
    V(c, b, font, 0x1C, 0x824B1E48, {font, sp + 0x50});
    const uint32_t midpoint = uint32_t(int32_t(c.r3.u32 + before) / 2) + 5;
    if (int32_t(x) >= int32_t(midpoint))
      result = i + 1;
    Call(c, b, sub_82125760, 0x824B1E74, {sp + 0x50, 1, 0});
    Call(c, b, sub_82125760, 0x824B1E84, {sp + 0x70, 1, 0});
  }
  c.r3.u64 = result;
}
void Native_824B1EF8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  const uint8_t fast = c.r4.u8;
  Frame f(c, b, 0xF0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0x50, 0);
  while (int32_t(E(b, self).cursor) < int32_t(E(b, self).scroll)) {
    const uint32_t next = uint32_t(E(b, self).scroll) - (fast ? 10 : 1);
    E(b, self).scroll = int32_t(next) < 0 ? 0 : next;
    V(c, b, self, 0x10, 0x824B1F74, {self});
  }
  if (!E(b, self).font)
    return;
  Call(c, b, sub_824AFD08, 0x824B1F98, {self});
  const uint32_t text = c.r3.u32;
  while (true) {
    bool advance = false;
    if (int32_t(uint32_t(W(b, self).width) - 8) > 0) {
      Call(c, b, sub_82181308, 0x824B1FBC, {sp + 0x80, text, 0, uint32_t(E(b, self).cursor)});
      const uint32_t before = c.r3.u32;
      pvz::guest::Write(b, sp + 0x50, 1);
      Call(c, b, sub_82181308, 0x824B1FDC, {sp + 0x60, text, 0, uint32_t(E(b, self).scroll)});
      pvz::guest::Write(b, sp + 0x50, 3);
      const uint32_t font = E(b, self).font;
      V(c, b, font, 0x1C, 0x824B2000, {font, c.r3.u32});
      const uint32_t width = c.r3.u32;
      V(c, b, font, 0x1C, 0x824B201C, {font, before});
      advance = int32_t(c.r3.u32 - width) >= int32_t(uint32_t(W(b, self).width) - 8);
      Call(c, b, sub_82125760, 0x824B2058, {sp + 0x60, 1, 0});
      Call(c, b, sub_82125760, 0x824B2074, {sp + 0x80, 1, 0});
    }
    if (!advance)
      break;
    const uint32_t next = uint32_t(E(b, self).scroll) + (fast ? 10 : 1),
                   last = uint32_t(E(b, self).text.length) - 1;
    E(b, self).scroll = int32_t(next) < int32_t(last) ? next : last;
    V(c, b, self, 0x10, 0x824B20C0, {self});
  }
}
uint32_t EditColor(PPCContext& c, uint8_t* b, uint32_t self, uint32_t sp, uint32_t index) {
  const uint32_t begin = W(b, self).colors_begin;
  const int32_t count = int32_t(uint32_t(W(b, self).colors_end) - begin) >> 4;
  pvz::guest::Write(b, sp + 0x50, self + 0x78);
  if (index ? uint32_t(count) <= index : count == 0)
    ppc_trap(c, b, 22);
  return begin + 16 * index;
}
void Native_824AFDD0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0x190);
  const uint32_t sp = c.r1.u32;
  uint32_t constructed = 0;
  pvz::guest::Write(b, sp + 0x54, 0);
  if (!E(b, self).font) {
    Call(c, b, sub_825DDB78, 0x824AFE04, {0x20});
    const uint32_t memory = c.r3.u32;
    pvz::guest::Write(b, sp + 0x50, memory);
    if (memory) {
      Call(c, b, sub_821264E0, 0x824AFE20, {sp + 0x60, 0x82064DFC});
      constructed = 1;
      pvz::guest::Write(b, sp + 0x54, 1);
      Call(c, b, sub_824DD5E0, 0x824AFE50, {memory, App(b, self), sp + 0x60, 10, 0, 0, 0, 0});
    } else
      c.r3.u64 = 0;
    E(b, self).font = c.r3.u32;
    if (constructed)
      Call(c, b, sub_821269C0, 0x824AFE74, {sp + 0x60, 1, 0});
  }
  Call(c, b, sub_824AFD08, 0x824AFE7C, {self});
  const uint32_t text = c.r3.u32;
  Call(c, b, sub_8249A0C0, 0x824AFEA8, {graphics, EditColor(c, b, self, sp, 0)});
  const uint32_t inject = REXCVAR_GET(pvz_native_edit_824AFDD0_inject_error) ? 1 : 0;
  Call(c, b, sub_8249B650, 0x824AFEC0,
       {graphics, inject, 0, uint32_t(W(b, self).width), uint32_t(W(b, self).height)});
  for (uint32_t pass = 0; pass < 2; ++pass) {
    Call(c, b, sub_8249DDB0, 0x824AFECC, {graphics});
    const uint32_t g = c.r3.u32;
    Call(c, b, sub_8249A0B8, 0x824AFED8, {g, uint32_t(E(b, self).font)});
    uint32_t x = 4, y = 0, width = uint32_t(W(b, self).width) - 8, height = W(b, self).height;
    if (pass == 1) {
      Call(c, b, sub_82181308, 0x824AFEF4, {sp + 0xE0, text, 0, uint32_t(E(b, self).cursor)});
      const uint32_t prefix = c.r3.u32;
      Call(c, b, sub_82181308, 0x824AFF0C, {sp + 0x80, text, 0, uint32_t(E(b, self).scroll)});
      const uint32_t font = E(b, self).font;
      V(c, b, font, 0x1C, 0x824AFF28, {font, c.r3.u32});
      const uint32_t scrollWidth = c.r3.u32;
      V(c, b, font, 0x1C, 0x824AFF44, {font, prefix});
      uint32_t cursorWidth = c.r3.u32 - scrollWidth;
      Call(c, b, sub_82125760, 0x824AFF58, {sp + 0x80, 1, 0});
      Call(c, b, sub_82125760, 0x824AFF68, {sp + 0xE0, 1, 0});
      const uint32_t caret = cursorWidth + 2;
      uint32_t anchorWidth = caret;
      if (uint32_t(E(b, self).anchor) != UINT32_MAX &&
          uint32_t(E(b, self).anchor) != uint32_t(E(b, self).cursor)) {
        Call(c, b, sub_82181308, 0x824AFF98, {sp + 0xC0, text, 0, uint32_t(E(b, self).anchor)});
        const uint32_t anchor = c.r3.u32;
        Call(c, b, sub_82181308, 0x824AFFB0, {sp + 0xA0, text, 0, uint32_t(E(b, self).scroll)});
        const uint32_t anchorFont = E(b, self).font;
        V(c, b, anchorFont, 0x1C, 0x824AFFCC, {anchorFont, c.r3.u32});
        const uint32_t sw = c.r3.u32;
        V(c, b, anchorFont, 0x1C, 0x824AFFE8, {anchorFont, anchor});
        anchorWidth = c.r3.u32 - sw;
        Call(c, b, sub_82125760, 0x824AFFFC, {sp + 0xA0, 1, 0});
        Call(c, b, sub_82125760, 0x824B000C, {sp + 0xC0, 1, 0});
      }
      if (!E(b, self).blink)
        cursorWidth = caret;
      const int32_t maximum = int32_t(uint32_t(W(b, self).width) - 8);
      auto clamp = [&](uint32_t value) -> uint32_t {
        const int32_t positive = int32_t(value) < 0 ? 0 : int32_t(value);
        return positive >= maximum ? uint32_t(maximum) : uint32_t(positive);
      };
      cursorWidth = clamp(cursorWidth);
      anchorWidth = clamp(anchorWidth);
      const uint32_t left = int32_t(cursorWidth) < int32_t(anchorWidth) ? cursorWidth : anchorWidth;
      const uint32_t heightFont = E(b, self).font;
      V(c, b, heightFont, 0x10, 0x824B00B4, {heightFont});
      const uint32_t fontHeight = c.r3.u32;
      V(c, b, heightFont, 0x10, 0x824B00D4, {heightFont});
      y = uint32_t(int32_t(uint32_t(W(b, self).height) - c.r3.u32) / 2);
      x = left + 4;
      const uint32_t delta = anchorWidth - cursorWidth;
      width = int32_t(delta) < 0 ? 0 - delta : delta;
      height = fontHeight;
    }
    Call(c, b, sub_8249C6A8, 0x824B0110, {g, x, y, width, height});
    const bool selection = W(b, self).flag_73 || E(b, self).show_selection;
    if (pass == 1 && selection) {
      Call(c, b, sub_8249A0C0, 0x824B0170, {g, EditColor(c, b, self, sp, 3)});
      Call(c, b, sub_8249B650, 0x824B0188,
           {g, 0, 0, uint32_t(W(b, self).width), uint32_t(W(b, self).height)});
    }
    Call(c, b, sub_8249A0C0, 0x824B01EC, {g, EditColor(c, b, self, sp, pass != 0 && selection ? 4 : 2)});
    Call(c, b, sub_82181308, 0x824B0200, {sp + 0x100, text, uint32_t(E(b, self).scroll), UINT32_MAX});
    const uint32_t visible = c.r3.u32;
    uint32_t font = E(b, self).font;
    V(c, b, font, 0x10, 0x824B0218, {font});
    const uint32_t baseline = uint32_t(int32_t(uint32_t(W(b, self).height) - c.r3.u32) / 2);
    font = E(b, self).font;
    V(c, b, font, 4, 0x824B0240, {font});
    Call(c, b, sub_8249B1A0, 0x824B0254, {g, visible, 4, c.r3.u32 + baseline});
    Call(c, b, sub_82125760, 0x824B0264, {sp + 0x100, 1, 0});
    if (g)
      V(c, b, g, 0, 0x824B0284, {g, 1});
  }
  Call(c, b, sub_8249A0C0, 0x824B02BC, {graphics, EditColor(c, b, self, sp, 1)});
  Call(c, b, sub_8249B730, 0x824B02DC,
       {graphics, 0, 0, uint32_t(W(b, self).width) - 1, uint32_t(W(b, self).height) - 1});
}
// String helpers preserve guest temporary addresses, call order and Xbox LRs.
void Substring(PPCContext& c, uint8_t* b, uint32_t out, uint32_t text, uint32_t start, uint32_t count,
               uint32_t lr) {
  Call(c, b, sub_82181308, lr, {out, text, start, count});
}
void Assign(PPCContext& c, uint8_t* b, uint32_t out, uint32_t text, uint32_t lr) {
  Call(c, b, sub_821257D8, lr, {out, text, 0, UINT32_MAX});
}
void ReleaseString(PPCContext& c, uint8_t* b, uint32_t text, uint32_t lr) {
  Call(c, b, sub_82125760, lr, {text, 1, 0});
}
void Erase(PPCContext& c, uint8_t* b, uint32_t text, uint32_t start, uint32_t end,
           std::array<uint32_t, 3> temp, std::array<uint32_t, 7> lr) {
  Substring(c, b, temp[0], text, end, UINT32_MAX, lr[0]);
  const uint32_t suffix = c.r3.u32;
  Substring(c, b, temp[1], text, 0, start, lr[1]);
  Call(c, b, sub_82125E30, lr[2], {temp[2], c.r3.u32, suffix});
  Assign(c, b, text, c.r3.u32, lr[3]);
  ReleaseString(c, b, temp[2], lr[4]);
  ReleaseString(c, b, temp[1], lr[5]);
  ReleaseString(c, b, temp[0], lr[6]);
}
void Insert(PPCContext& c, uint8_t* b, uint32_t text, uint32_t start, uint32_t end, uint32_t insert,
            std::array<uint32_t, 4> temp, std::array<uint32_t, 9> lr, uint32_t single = 0,
            std::array<uint32_t, 3> singleLR = {}) {
  Substring(c, b, temp[0], text, end, UINT32_MAX, lr[0]);
  const uint32_t suffix = c.r3.u32;
  if (singleLR[0]) {
    Call(c, b, sub_82125760, singleLR[0], {insert, 0, 0});
    Call(c, b, sub_82474808, singleLR[1], {insert, 1, single});
  }
  Substring(c, b, temp[1], text, 0, start, lr[1]);
  Call(c, b, sub_82125E30, lr[2], {temp[2], c.r3.u32, insert});
  Call(c, b, sub_82125E30, lr[3], {temp[3], c.r3.u32, suffix});
  Assign(c, b, text, c.r3.u32, lr[4]);
  ReleaseString(c, b, temp[3], lr[5]);
  ReleaseString(c, b, temp[2], lr[6]);
  ReleaseString(c, b, temp[1], lr[7]);
  if (singleLR[0])
    ReleaseString(c, b, insert, singleLR[2]);
  ReleaseString(c, b, temp[0], lr[8]);
}
void Native_824B0668(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, key = c.r4.u32, ch = c.r5.u32;
  Frame f(c, b, 0x690);
  const uint32_t sp = c.r1.u32;
  auto& modifiers = *reinterpret_cast<pvz::xbox::EditModifiers*>(b + uint32_t(W(b, self).manager));
  const uint8_t shift = modifiers.shift, control = modifiers.control;
  if (key == 0x10 || key == 0x11)
    return;
  bool clearAnchor = !shift, saveUndo = false;
  if (shift && uint32_t(E(b, self).anchor) == UINT32_MAX)
    E(b, self).anchor = uint32_t(E(b, self).cursor);
  const uint32_t text = self + 0xD4;
  Call(c, b, sub_82125F60, 0x824B06DC, {sp + 0xE0, text});
  const uint32_t oldCursor = E(b, self).cursor, oldAnchor = E(b, self).anchor;
  const uint16_t character = uint16_t(ch);
  auto selection = [&] {
    return uint32_t(E(b, self).anchor) != UINT32_MAX &&
           uint32_t(E(b, self).anchor) != uint32_t(E(b, self).cursor);
  };
  auto lo = [&] {
    const uint32_t a = E(b, self).anchor, p = E(b, self).cursor;
    return int32_t(p) < int32_t(a) ? p : a;
  };
  auto hi = [&] {
    const uint32_t a = E(b, self).anchor, p = E(b, self).cursor;
    return int32_t(p) > int32_t(a) ? p : a;
  };
  auto erased = [&] {
    E(b, self).cursor = lo();
    E(b, self).anchor = UINT32_MAX;
    saveUndo = true;
  };
  if (character == 3 || character == 0x18) {
    if (selection()) {
      const bool forward = int32_t(oldCursor) < int32_t(oldAnchor);
      Call(c, b, sub_824AFD08, forward ? 0x824B13CC : 0x824B1428, {self});
      const uint32_t visible = c.r3.u32;
      // Xbox passes the larger endpoint as count here, rather than subtracting.
      Substring(c, b, sp + (forward ? 0x460 : 0x4E0), visible, forward ? oldCursor : oldAnchor,
                forward ? oldAnchor : oldCursor, forward ? 0x824B13E0 : 0x824B143C);
      Call(c, b, sub_82496278, forward ? 0x824B13EC : 0x824B1448, {sp + (forward ? 0x420 : 0x4A0), c.r3.u32});
      Call(c, b, sub_82475EB0, forward ? 0x824B13FC : 0x824B1458, {App(b, self), c.r3.u32});
      Call(c, b, sub_821269C0, forward ? 0x824B140C : 0x824B1468, {sp + (forward ? 0x420 : 0x4A0), 1, 0});
      ReleaseString(c, b, sp + (forward ? 0x460 : 0x4E0), forward ? 0x824B141C : 0x824B1478);
      if (character == 3)
        clearAnchor = false;
      else {
        Erase(c, b, text, lo(), hi(), {sp + 0x5A0, sp + 0x560, sp + 0x520},
              {0x824B14B8, 0x824B14D0, 0x824B14E0, 0x824B14F4, 0x824B1504, 0x824B1514, 0x824B1524});
        erased();
      }
    }
  } else if (character == 0x16) {
    Call(c, b, sub_8247D790, 0x824B0720, {sp + 0x480, App(b, self)});
    Call(c, b, sub_82495870, 0x824B072C, {sp + 0x60, c.r3.u32});
    Call(c, b, sub_821269C0, 0x824B073C, {sp + 0x480, 1, 0});
    if (uint32_t(Str(b, sp + 0x60).length)) {
      Call(c, b, sub_82125760, 0x824B0758, {sp + 0x80, 0, 0});
      for (uint32_t i = 0; i < uint32_t(Str(b, sp + 0x60).length); ++i) {
        uint16_t value = Character(c, b, sp + 0x60, i);
        if (value == 13 || value == 10)
          break;
        const uint32_t font = E(b, self).font;
        V(c, b, font, 0x20, 0x824B07F4, {font, value});
        if (c.r3.u32) {
          value = Character(c, b, sp + 0x60, i);
          const uint32_t listener = E(b, self).listener;
          V(c, b, listener, 8, 0x824B083C, {listener, uint32_t(W(b, self).id), value});
          if (c.r3.u8) {
            value = Character(c, b, sp + 0x60, i);
            Call(c, b, sub_821C6DC8, 0x824B0878, {sp + 0x80, 1, value});
          }
        }
      }
      if (uint32_t(E(b, self).anchor) == UINT32_MAX) {
        const uint32_t p = E(b, self).cursor;
        Insert(c, b, text, p, p, sp + 0x80, {sp + 0x5C0, sp + 0x1C0, sp + 0x3C0, sp + 0x180},
               {0x824B08AC, 0x824B08C4, 0x824B08D4, 0x824B08E4, 0x824B08F8, 0x824B0908, 0x824B0918,
                0x824B0928, 0x824B0938});
      } else {
        Insert(c, b, text, lo(), hi(), sp + 0x80, {sp + 0x500, sp + 0x240, sp + 0x400, sp + 0x200},
               {0x824B0968, 0x824B0980, 0x824B0990, 0x824B09A0, 0x824B09B4, 0x824B09C4, 0x824B09D4,
                0x824B09E4, 0x824B09F4});
        E(b, self).cursor = lo();
        E(b, self).anchor = UINT32_MAX;
      }
      saveUndo = true;
      E(b, self).cursor = uint32_t(E(b, self).cursor) + uint32_t(Str(b, sp + 0x80).length);
      ReleaseString(c, b, sp + 0x80, 0x824B0A34);
    }
    ReleaseString(c, b, sp + 0x60, 0x824B0A44);
  } else if (character == 0x1A) {
    E(b, self).undo_scroll = UINT32_MAX;
    Call(c, b, sub_82125F60, 0x824B0A60, {sp + 0x120, text});
    const uint32_t cursor = E(b, self).cursor, anchor = E(b, self).anchor;
    Assign(c, b, text, self + 0x164, 0x824B0A80);
    E(b, self).cursor = uint32_t(E(b, self).undo_cursor);
    E(b, self).anchor = uint32_t(E(b, self).undo_anchor);
    Assign(c, b, self + 0x164, sp + 0x120, 0x824B0AA4);
    E(b, self).undo_cursor = cursor;
    E(b, self).undo_anchor = anchor;
    clearAnchor = false;
    ReleaseString(c, b, sp + 0x120, 0x824B0AC0);
  } else if (key == 0x25) {
    if (control) {
      while (int32_t(E(b, self).cursor) > 0) {
        V(c, b, self, 0x12C, 0x824B0B28, {self, Character(c, b, text, uint32_t(E(b, self).cursor) - 1)});
        if (c.r3.u8)
          break;
        E(b, self).cursor = uint32_t(E(b, self).cursor) - 1;
      }
      while (int32_t(E(b, self).cursor) > 0) {
        V(c, b, self, 0x12C, 0x824B0B98, {self, Character(c, b, text, uint32_t(E(b, self).cursor) - 1)});
        if (!c.r3.u8)
          break;
        E(b, self).cursor = uint32_t(E(b, self).cursor) - 1;
      }
    } else if (!shift && oldAnchor != UINT32_MAX)
      E(b, self).cursor = lo();
    else
      E(b, self).cursor = oldCursor - 1;
  } else if (key == 0x27) {
    if (control) {
      while (int32_t(E(b, self).cursor) < int32_t(uint32_t(E(b, self).text.length) - 1)) {
        V(c, b, self, 0x12C, 0x824B0C54, {self, Character(c, b, text, uint32_t(E(b, self).cursor) + 1)});
        if (!c.r3.u8)
          break;
        E(b, self).cursor = uint32_t(E(b, self).cursor) + 1;
      }
      while (int32_t(E(b, self).cursor) < int32_t(uint32_t(E(b, self).text.length) - 1)) {
        V(c, b, self, 0x12C, 0x824B0CCC, {self, Character(c, b, text, uint32_t(E(b, self).cursor) + 1)});
        if (c.r3.u8)
          break;
        E(b, self).cursor = uint32_t(E(b, self).cursor) + 1;
      }
    }
    if (!shift && uint32_t(E(b, self).anchor) != UINT32_MAX)
      E(b, self).cursor = hi();
    else
      E(b, self).cursor = uint32_t(E(b, self).cursor) + 1;
  } else if (key == 8) {
    if (uint32_t(E(b, self).text.length)) {
      if (selection()) {
        Erase(c, b, text, lo(), hi(), {sp + 0x2C0, sp + 0x440, sp + 0x280},
              {0x824B0D78, 0x824B0D90, 0x824B0DA0, 0x824B0DB4, 0x824B0DC4, 0x824B0DD4, 0x824B0DE4});
        erased();
      } else {
        if (int32_t(oldCursor) > 0)
          Erase(c, b, text, uint32_t(E(b, self).cursor) - 1, oldCursor, {sp + 0x140, sp + 0x300, sp + 0x580},
                {0x824B0E24, 0x824B0E40, 0x824B0E50, 0x824B0E64, 0x824B0E74, 0x824B0E84, 0x824B0E94});
        else {
          Substring(c, b, sp + 0x340, text, oldCursor, UINT32_MAX, 0x824B0EAC);
          Assign(c, b, text, c.r3.u32, 0x824B0EC0);
          ReleaseString(c, b, sp + 0x340, 0x824B0ED0);
        }
        const uint32_t p = uint32_t(E(b, self).cursor) - 1;
        E(b, self).anchor = UINT32_MAX;
        E(b, self).cursor = p;
        saveUndo = p != uint32_t(E(b, self).undo_scroll);
        E(b, self).undo_scroll = p - 1;
      }
    }
  } else if (key == 0x2E) {
    const uint32_t length = E(b, self).text.length;
    pvz::guest::Write(b, sp + 0x50, length);
    if (length) {
      if (selection()) {
        Erase(c, b, text, lo(), hi(), {sp + 0x4C0, sp + 0x380, sp + 0x540},
              {0x824B0F4C, 0x824B0F64, 0x824B0F74, 0x824B0F88, 0x824B0F98, 0x824B0FA8, 0x824B0FB8});
        erased();
      } else {
        pvz::guest::Write(b, sp + 0x50, length);
        if (int32_t(oldCursor) < int32_t(length))
          Erase(c, b, text, uint32_t(E(b, self).cursor), oldCursor + 1, {sp + 0x1E0, sp + 0x1A0, sp + 0x160},
                {0x824B0FFC, 0x824B1014, 0x824B1024, 0x824B1038, 0x824B1048, 0x824B1058, 0x824B1068});
        const uint32_t p = E(b, self).cursor;
        saveUndo = p != uint32_t(E(b, self).undo_scroll);
        E(b, self).undo_scroll = p;
      }
    }
  } else if (key == 0x24)
    E(b, self).cursor = 0;
  else if (key == 0x23)
    E(b, self).cursor = uint32_t(E(b, self).text.length);
  else if (key == 13) {
    const uint32_t listener = E(b, self).listener;
    V(c, b, listener, 0, 0x824B10CC, {listener, uint32_t(W(b, self).id), text});
  } else {
    Call(c, b, sub_82125760, 0x824B10E0, {sp + 0xC0, 0, 0});
    Call(c, b, sub_82474808, 0x824B10F0, {sp + 0xC0, 1, ch});
    const uint32_t global = pvz::guest::Read(b, 0x828923E8);
    const uint32_t maximum =
        reinterpret_cast<pvz::xbox::EditGlobalApp*>(b + global)->extended_characters ? 255 : 127;
    bool accepted = character >= 32 && character <= maximum;
    if (accepted) {
      const uint32_t font = E(b, self).font;
      V(c, b, font, 0x1C, 0x824B1134, {font, sp + 0xC0});
      accepted = c.r3.s32 > 0;
    }
    if (accepted) {
      const uint32_t listener = E(b, self).listener;
      V(c, b, listener, 8, 0x824B1158, {listener, uint32_t(W(b, self).id), ch});
      accepted = c.r3.u8 != 0;
    }
    if (accepted) {
      if (selection()) {
        Insert(c, b, text, lo(), hi(), sp + 0xA0, {sp + 0x2E0, sp + 0x2A0, sp + 0x260, sp + 0x220},
               {0x824B11A0, 0x824B11D8, 0x824B11E8, 0x824B11F8, 0x824B120C, 0x824B121C, 0x824B122C,
                0x824B123C, 0x824B125C},
               ch, {0x824B11B4, 0x824B11C4, 0x824B124C});
        erased();
      } else {
        const uint32_t p = E(b, self).cursor;
        Insert(c, b, text, p, p, sp + 0x100, {sp + 0x3E0, sp + 0x3A0, sp + 0x360, sp + 0x320},
               {0x824B1294, 0x824B12CC, 0x824B12DC, 0x824B12EC, 0x824B1300, 0x824B1310, 0x824B1320,
                0x824B1330, 0x824B1350},
               ch, {0x824B12A8, 0x824B12B8, 0x824B1340});
        const uint32_t now = E(b, self).cursor;
        saveUndo = now != uint32_t(E(b, self).undo_scroll) + 1;
        E(b, self).undo_scroll = now;
        E(b, self).anchor = UINT32_MAX;
      }
      E(b, self).cursor = uint32_t(E(b, self).cursor) + 1;
      V(c, b, self, 0x134, 0x824B1394, {self, 0});
    } else
      clearAnchor = false;
    ReleaseString(c, b, sp + 0xC0, 0x824B13AC);
  }
  const int32_t maxChars = E(b, self).max_chars;
  if (maxChars != -1 && int32_t(E(b, self).text.length) > maxChars) {
    Substring(c, b, sp + 0x5E0, text, 0, uint32_t(maxChars), 0x824B156C);
    Assign(c, b, text, c.r3.u32, 0x824B1580);
    ReleaseString(c, b, sp + 0x5E0, 0x824B1590);
  }
  Call(c, b, sub_824B0420, 0x824B1598, {self});
  const uint32_t cursor = E(b, self).cursor;
  if (int32_t(cursor) < 0)
    E(b, self).cursor = 0;
  else {
    const uint32_t length = E(b, self).text.length;
    pvz::guest::Write(b, sp + 0x50, length);
    if (int32_t(cursor) > int32_t(length))
      E(b, self).cursor = length;
  }
  if (oldCursor != uint32_t(E(b, self).cursor)) {
    E(b, self).blink_counter = 0;
    E(b, self).blink = 1;
  }
  V(c, b, self, 0x134, 0x824B15F0, {self, 1});
  if (clearAnchor || uint32_t(E(b, self).anchor) == uint32_t(E(b, self).cursor))
    E(b, self).anchor = UINT32_MAX;
  const uint32_t listener = E(b, self).listener;
  V(c, b, listener, 0xC, 0x824B1628, {listener, uint32_t(W(b, self).id), text});
  if (!c.r3.u8) {
    Assign(c, b, text, sp + 0xE0, 0x824B1644);
    E(b, self).cursor = oldCursor;
    E(b, self).anchor = oldAnchor;
  } else if (saveUndo) {
    Assign(c, b, self + 0x164, sp + 0xE0, 0x824B166C);
    E(b, self).undo_cursor = oldCursor;
    E(b, self).undo_anchor = oldAnchor;
  }
  V(c, b, self, 0x10, 0x824B1688, {self});
  ReleaseString(c, b, sp + 0xE0, 0x824B1698);
}
#endif
} // namespace
#ifndef PVZ_BATCH_SYNTHETIC_TEST
#define LEAF(A, P, N, O, R, I)                                                                               \
  PVZ_NATIVE_BATCH_REPLACEMENT(b018, P, edit_##A, sub_##A, N, pvz::native::Safety::DeclaredOutputsOnly, O, R)
#define SNAP(A, P, N)                                                                                        \
  PVZ_NATIVE_BATCH_REPLACEMENT(b018, P, edit_##A, sub_##A, N, pvz::native::Safety::SnapshotsOnly, noOutputs, \
                               NoRanges)
#endif
LEAF(824AF298, true, True, r3out, NoRanges, Identity)
LEAF(824AF470, true, WordCharacter, r3out, NoRanges, WordCharacter)
SNAP(824AF2A0, false, Native_824AF2A0)
SNAP(824AF310, false, Native_824AF310)
SNAP(824AF388, false, Native_824AF388)
SNAP(824AF3D0, false, Native_824AF3D0)
SNAP(824AF4D0, false, Native_824AF4D0)
SNAP(824AF560, false, Native_824AF560)
SNAP(824AF5B8, false, Native_824AF5B8)
SNAP(824AF658, false, Native_824AF658)
SNAP(824AF6F8, false, Native_824AF6F8)
SNAP(824AF780, false, MouseBoundary<true>)
SNAP(824AF7C0, false, MouseBoundary<false>)
SNAP(824AF800, false, Native_824AF800)
SNAP(824AF830, false, Native_824AF830)
SNAP(824AF900, false, Native_824AF900)
SNAP(824AFAC8, false, Native_824AFAC8)
SNAP(824AFC70, false, Native_824AFC70)
SNAP(824AFD08, false, Native_824AFD08)
SNAP(824B2298, false, Native_824B2298)
SNAP(824B2320, false, Native_824B2320)
SNAP(824B23D8, false, Native_824B23D8)
SNAP(824B2150, false, Native_824B2150)
SNAP(824B1DB0, false, Native_824B1DB0)
SNAP(824B1EF8, false, Native_824B1EF8)
SNAP(824AFDD0, false, Native_824AFDD0)
SNAP(824B0668, false, Native_824B0668)
#undef LEAF
#undef SNAP
