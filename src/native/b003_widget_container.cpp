// B003. Xbox WidgetContainer; behavior comes from complete Xbox bodies.
#include "replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/Widget.h"
#include "xbox/WidgetContainer.h"
#ifndef PVZ_T15_SYNTHETIC_TEST
#include "guest_call.h"
#endif
#ifndef PVZ_T15_SYNTHETIC_TEST
PVZ_NATIVE_BATCH(b003)
#endif
REX_EXTERN(sub_821DF2A0);
REX_EXTERN(sub_824A68F0);
REX_EXTERN(sub_82489160);
REX_EXTERN(sub_824C41D8);
REX_EXTERN(sub_825F8F50);
REX_EXTERN(sub_82492FD8);
REX_EXTERN(sub_821B1448);
namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 1> r3out{{{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}}};
const std::array<RegisterOutput, 0> noOutputs{};
auto NoRanges(const PPCContext&) {
  return std::array<Range, 0>{};
}
void Identity(PPCContext&, uint8_t*) {}
auto& W(uint8_t* b, uint32_t at) {
  return *reinterpret_cast<pvz::xbox::WidgetContainer*>(b + at);
}
auto& Widget(uint8_t* b, uint32_t at) {
  return *reinterpret_cast<pvz::xbox::Widget*>(b + at);
}
auto& Node(uint8_t* b, uint32_t at) {
  return *reinterpret_cast<pvz::xbox::WidgetListNode*>(b + at);
}
void SetField64(PPCContext& c, uint8_t* b) {
  W(b, c.r3.u32).field_64 = c.r4.u32;
}
auto Field64Range(const PPCContext& c) {
  return std::array{Range{c.r3.u32 + 0x64, 4}};
}
void UpdateCount(PPCContext& c, uint8_t* b) {
  auto& w = W(b, c.r3.u32);
  w.update_count = uint32_t(w.update_count) + 1;
}
auto CountRange(const PPCContext& c) {
  return std::array{Range{c.r3.u32 + 0x38, 4}};
}
auto RectRange(const PPCContext& c) {
  return std::array{Range{c.r3.u32, 16}};
}
void Rect(PPCContext& c, uint8_t* b) {
  const auto& w = W(b, c.r4.u32);
  const std::array<uint32_t, 4> v{uint32_t(w.x), uint32_t(w.y), uint32_t(w.width), uint32_t(w.height)};
  for (uint32_t i = 0; i < 4; ++i)
    pvz::guest::Write(b, c.r3.u32 + 4 * i, v[i]);
}
#ifndef PVZ_T15_SYNTHETIC_TEST
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
using pvz::native::Virtual;
// Checked Xbox list iterators use the proxy's first word to locate the list.
// Keep checks before dereference/advance, including after mutating callbacks.
void Owner(PPCContext& c, uint8_t* b, uint32_t owner, uint32_t self) {
  if (!owner || owner != uint32_t(W(b, self).list_proxy))
    ppc_trap(c, b, 22);
}
void Element(PPCContext& c, uint8_t* b, uint32_t owner, uint32_t node) {
  if (!owner)
    ppc_trap(c, b, 22);
  if (node == pvz::guest::Read(b, pvz::guest::Read(b, owner) + 0x14))
    ppc_trap(c, b, 22);
}
template <bool Full> void Dirty(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, parent = W(b, self).parent;
  Args(c, {self, self});
  if (parent) {
    Args(c, {parent, self});
    Virtual(c, b, parent, Full ? 20 : 12, uint32_t(c.lr));
  } else
    W(b, self).dirty = 1;
}
void Intersects(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, other = c.r4.u32;
  Frame f(c, b, 0x90);
  Args(c, {c.r1.u32 + 0x50, other});
  Virtual(c, b, other, 4, 0x82491D44);
  const uint32_t r = c.r3.u32;
  Args(c, {c.r1.u32 + 0x60, self});
  Virtual(c, b, self, 4, 0x82491D60);
  c.r4.u64 = r;
  c.lr = 0x82491D68;
  sub_821DF2A0(c, b);
}
template <bool Remove> void ManagerChanged(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, manager = c.r4.u32;
  Frame f(c, b, Remove ? 0x90 : 0x80);
  const uint32_t owner = W(b, self).list_proxy;
  uint32_t node = Node(b, W(b, self).children_head).next;
  for (;;) {
    Owner(c, b, owner, self);
    if (node == uint32_t(W(b, self).children_head))
      break;
    Element(c, b, owner, node);
    const uint32_t child = Node(b, node).widget;
    if constexpr (Remove) {
      Call(c, b, sub_824A68F0, 0x82492444, {manager, child});
      Args(c, {child, manager});
      Virtual(c, b, child, 32, 0x8249245C);
      W(b, child).manager = 0;
    } else {
      W(b, child).manager = manager;
      Args(c, {child, manager});
      Virtual(c, b, child, 28, 0x82492374);
    }
    Element(c, b, owner, node);
    node = Node(b, node).next;
    if constexpr (!Remove) {
      Args(c, {self});
      Virtual(c, b, self, 16, 0x824923AC);
    }
  }
  if constexpr (Remove) {
    if (pvz::guest::Read(b, manager + 0x80) == self)
      pvz::guest::Write(b, manager + 0x80, 0);
  }
}
void DirtyChild(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, child = c.r4.u32;
  Frame f(c, b, 0x90);
  if (W(b, child).dirty)
    return;
  Virtual(c, b, self, 16, 0x824924E0);
  W(b, child).dirty = 1;
  if (uint32_t(W(b, self).parent))
    return;
  if (W(b, child).flag_50) {
    Args(c, {self, child});
    Virtual(c, b, self, 20, 0x82492518);
    return;
  }
  const uint32_t owner = W(b, self).list_proxy;
  uint32_t node = Node(b, W(b, self).children_head).next;
  bool after = false;
  for (;;) {
    Owner(c, b, owner, self);
    if (node == uint32_t(W(b, self).children_head))
      return;
    Element(c, b, owner, node);
    const uint32_t next = Node(b, node).widget;
    if (next == child)
      after = true;
    else if (after && Widget(b, next).visible) {
      Args(c, {next, child});
      Virtual(c, b, next, 8, 0x824925C0);
      if (c.r3.u8) {
        Args(c, {self, next});
        Virtual(c, b, self, 12, 0x824925E0);
      }
    }
    Element(c, b, owner, node);
    node = Node(b, node).next;
  }
}
void UpdateF(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, context = c.r4.u32;
  const uint64_t dt = c.f1.u64;
  Frame f(c, b, 0xC0);
  auto& w = W(b, self);
  auto& flags = *reinterpret_cast<pvz::xbox::WidgetUpdateContext*>(b + context);
  const uint32_t a = flags.modal_flags, d = flags.flags;
  flags.modal_flags = (a | uint32_t(w.flags_add)) & ~uint32_t(w.flags_remove);
  flags.flags = (d | uint32_t(w.flags_add)) & ~uint32_t(w.flags_remove);
  if ((flags.is_modal ? uint32_t(flags.modal_flags) : uint32_t(flags.flags)) & 1) {
    Args(c, {self});
    c.f1.u64 = dt;
    Virtual(c, b, self, 44, 0x824928F8);
  }
  w.iterator_owner = w.list_proxy;
  w.iterator_node = Node(b, w.children_head).next;
  for (;;) {
    const uint32_t owner = w.iterator_owner, node = w.iterator_node;
    Owner(c, b, owner, self);
    if (node == uint32_t(w.children_head))
      break;
    w.iterator_changed = 0;
    Element(c, b, owner, node);
    const uint32_t child = Node(b, node).widget;
    if (child == pvz::guest::Read(b, uint32_t(w.manager) + 0xB4))
      flags.is_modal = 1;
    Args(c, {child, context});
    c.f1.u64 = dt;
    Virtual(c, b, child, 48, 0x824929B0);
    if (!w.iterator_changed) {
      Element(c, b, w.iterator_owner, w.iterator_node);
      w.iterator_node = Node(b, w.iterator_node).next;
    }
  }
  w.iterator_changed = 1;
  flags.modal_flags = a;
  flags.flags = d;
}
void NotifyAll(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Virtual(c, b, self, 76, 0x82492E60);
  if (uint32_t(W(b, self).children_count))
    pvz::guest::Write(b, 0x828924D0, pvz::guest::Read(b, 0x828924D0) + 1);
  const uint32_t owner = W(b, self).list_proxy;
  uint32_t node = Node(b, W(b, self).children_head).next;
  for (;;) {
    Owner(c, b, owner, self);
    if (node == uint32_t(W(b, self).children_head))
      return;
    Element(c, b, owner, node);
    const uint32_t child = Node(b, node).widget;
    Args(c, {child});
    Virtual(c, b, child, 72, 0x82492EE4);
    Element(c, b, owner, node);
    node = Node(b, node).next;
  }
}
void Construct(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  auto& w = W(b, self);
  w.vtable = 0x82062ACC;
  Call(c, b, sub_82489160, 0x82492F50, {self + 4});
  w.iterator_owner = 0;
  w.iterator_node = 0;
  w.flags_add = 0;
  w.flags_remove = 0;
  w.x = 0;
  w.y = 0;
  w.width = 0;
  w.height = 0;
  w.parent = 0;
  w.manager = 0;
  w.iterator_changed = 0;
  w.last_manager_update_count = 0;
  w.update_count = 0;
  w.dirty = 0;
  w.flag_50 = 0;
  w.flag_51 = 1;
  w.field_5c = 0;
  w.field_60 = 0;
  w.field_64 = 0;
  w.iterator_owner = w.list_proxy;
  w.iterator_node = w.children_head;
  c.r3.u64 = self;
}
void Destruct(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  W(b, self).vtable = 0x82062ACC;
  Call(c, b, sub_824C41D8, 0x82493000, {self + 4});
  Call(c, b, sub_825F8F50, 0x82493008, {W(b, self).children_head});
  const uint32_t proxy = W(b, self).list_proxy;
  W(b, self).children_head = 0;
  Call(c, b, sub_825F8F50, 0x82493018, {proxy});
}
void Delete(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82492FD8, 0x824940E0, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x824940F0, {self});
  c.r3.u64 = self;
}
void DirtyChildFull(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, child = c.r4.u32;
  Frame f(c, b, 0xE0);
  pvz::guest::Write(b, c.r1.u32 + 0xFC, child);
  Virtual(c, b, self, 24, 0x82493210);
  W(b, child).dirty = 1;
  if (uint32_t(W(b, self).parent))
    return;
  const uint32_t owner = W(b, self).list_proxy, head = W(b, self).children_head;
  uint32_t found = Node(b, head).next;
  while (found != head && uint32_t(Node(b, found).widget) != child)
    found = Node(b, found).next;
  Owner(c, b, owner, self);
  if (found == head)
    return;
  uint32_t node = found;
  while (node != uint32_t(Node(b, W(b, self).children_head).next)) {
    Element(c, b, owner, node);
    node = Node(b, node).previous;
    Element(c, b, owner, node);
    const uint32_t previous = Node(b, node).widget;
    if (Widget(b, previous).visible) {
      bool covered = false;
      if (!Widget(b, previous).flag_76 && !W(b, previous).flag_50) {
        const uint32_t a = c.r1.u32 + 0x50, r = c.r1.u32 + 0x60, out = c.r1.u32 + 0x70;
        for (uint32_t i = 0; i < 4; ++i)
          pvz::guest::Write(b, a + i * 4, pvz::guest::Read(b, child + 0x40 + i * 4));
        pvz::guest::Write(b, r, 0);
        pvz::guest::Write(b, r + 4, 0);
        pvz::guest::Write(b, r + 8, uint32_t(W(b, self).width));
        pvz::guest::Write(b, r + 12, uint32_t(W(b, self).height));
        Call(c, b, sub_821B1448, 0x82493368, {out, a, r});
        const uint32_t x = pvz::guest::Read(b, out), y = pvz::guest::Read(b, out + 4);
        Args(c, {previous, x, y});
        Virtual(c, b, previous, 276, 0x8249338C);
        if (c.r3.u8) {
          Args(c, {previous, x + pvz::guest::Read(b, out + 8) - 1, y + pvz::guest::Read(b, out + 12) - 1});
          Virtual(c, b, previous, 276, 0x824933C0);
          covered = c.r3.u8 != 0;
        }
      }
      if (covered) {
        Args(c, {previous});
        Virtual(c, b, previous, 16, 0x8249347C);
        break;
      }
      Args(c, {previous, child});
      Virtual(c, b, previous, 8, 0x824933E0);
      if (c.r3.u8) {
        Args(c, {self, previous});
        Virtual(c, b, self, 12, 0x82493400);
      }
    }
    Owner(c, b, owner, self);
  }
  node = found;
  for (;;) {
    Owner(c, b, owner, self);
    if (node == uint32_t(W(b, self).children_head))
      return;
    Element(c, b, owner, node);
    const uint32_t next = Node(b, node).widget;
    if (Widget(b, next).visible) {
      Args(c, {next, child});
      Virtual(c, b, next, 8, 0x824934F4);
      if (c.r3.u8) {
        Args(c, {self, next});
        Virtual(c, b, self, 12, 0x82493514);
      }
    }
    Element(c, b, owner, node);
    node = Node(b, node).next;
  }
}
#endif
} // namespace
#ifndef PVZ_T15_SYNTHETIC_TEST
#define LEAF(A, P, B, O, R, I)                                                                               \
  PVZ_NATIVE_BATCH_REPLACEMENT(b003, P, container_##A, sub_##A, B, pvz::native::Safety::DeclaredOutputsOnly, \
                               O, R)
#define SNAP(A, P, B)                                                                                        \
  PVZ_NATIVE_BATCH_REPLACEMENT(b003, P, container_##A, sub_##A, B, pvz::native::Safety::SnapshotsOnly,       \
                               noOutputs, NoRanges)
#endif
// 82492618 and 82492A48 retain their existing port hooks.
LEAF(82120000, true, SetField64, noOutputs, Field64Range, Field64)
LEAF(82491CB8, true, Identity, r3out, NoRanges, Identity)
LEAF(82491CC0, true, Identity, r3out, NoRanges, Identity)
LEAF(82491CC8, true, Identity, r3out, NoRanges, Identity)
LEAF(82491CD0, true, Identity, r3out, NoRanges, Identity)
LEAF(82491CD8, true, Identity, r3out, NoRanges, Identity)
LEAF(82491CE0, true, Identity, r3out, NoRanges, Identity)
LEAF(82491CA8, true, UpdateCount, noOutputs, CountRange, Count)
LEAF(82491CF0, true, Rect, r3out, RectRange, Rect)
SNAP(82491C48, true, Dirty<false>)
SNAP(82491C78, true, Dirty<true>)
SNAP(82491D18, true, Intersects)
SNAP(824922F0, true, ManagerChanged<false>)
SNAP(824923C8, true, ManagerChanged<true>)
SNAP(824924B0, true, DirtyChild)
SNAP(82492850, false, UpdateF)
SNAP(82492E40, false, NotifyAll)
SNAP(82492F28, true, Construct)
SNAP(82492FD8, true, Destruct)
SNAP(824940C0, false, Delete)
SNAP(824931E8, true, DirtyChildFull)
#undef LEAF
#undef SNAP
