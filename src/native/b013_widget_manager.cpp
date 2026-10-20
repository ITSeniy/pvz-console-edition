// B013. Xbox WidgetManager. Callback/list effects execute once (SnapshotsOnly).
#include "native/replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/WidgetContainer.h"
#include "xbox/WidgetManager.h"
#include <array>

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#include "native/guest_call.h"
PVZ_NATIVE_BATCH(b013)
REX_EXTERN(sub_82489398);
REX_EXTERN(sub_82491E90);
REX_EXTERN(sub_824920D8);
REX_EXTERN(sub_824921D0);
REX_EXTERN(sub_82492F28);
REX_EXTERN(sub_82492FD8);
REX_EXTERN(sub_82493620);
REX_EXTERN(sub_82493F70);
REX_EXTERN(sub_82494040);
REX_EXTERN(sub_824A5BA0);
REX_EXTERN(sub_824A5D20);
REX_EXTERN(sub_824A5F48);
REX_EXTERN(sub_824A67E8);
REX_EXTERN(sub_824A6D00);
REX_EXTERN(sub_825557B0);
REX_EXTERN(sub_82564B78);
REX_EXTERN(sub_825F8F50);
#endif

namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 0> noOutputs{};
const std::array<RegisterOutput, 1> r3out{{{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}}};
const auto NoRanges = [](const PPCContext&) { return std::array<Range, 0>{}; };
const auto FlagsRange = [](const PPCContext& c) {
  return std::array{Range{c.r4.u32, offsetof(pvz::xbox::WidgetUpdateContext, is_modal) + 1}};
};

uint32_t CurrentWidgetFlags(uint8_t* base, uint32_t self) {
  using Layout = pvz::xbox::WidgetManagerFlags;
  const uint8_t current_mode = base[self + offsetof(Layout, current_mode)];
  const uint32_t current = pvz::guest::Read(base, self + offsetof(Layout, current_flags));
  if (current_mode)
    return current;
  return (current | pvz::guest::Read(base, self + offsetof(Layout, flags_add))) &
         ~pvz::guest::Read(base, self + offsetof(Layout, flags_remove));
}
void WidgetManagerCurrentFlags(PPCContext& c, uint8_t* base) {
  c.r3.u64 = CurrentWidgetFlags(base, c.r3.u32);
}
void WidgetManagerGetFlags(PPCContext& c, uint8_t* base) {
  using Layout = pvz::xbox::WidgetManagerFlags;
  using Output = pvz::xbox::WidgetUpdateContext;
  const uint32_t self = c.r3.u32;
  // Preserve the Xbox store/load order when the output aliases manager fields.
  base[c.r4.u32 + offsetof(Output, is_modal)] =
      pvz::guest::Read(base, self + offsetof(Layout, modal_widget)) == 0 ? 1 : 0;
  const uint32_t flags = CurrentWidgetFlags(base, self);
  pvz::guest::Write(base, c.r4.u32, flags);
  const uint32_t changed = (pvz::guest::Read(base, self + offsetof(Layout, modal_flags_add)) | flags) &
                           ~pvz::guest::Read(base, self + offsetof(Layout, modal_flags_remove));
  pvz::guest::Write(base, c.r4.u32 + offsetof(Output, flags), changed);
}

#ifdef PVZ_BATCH_SYNTHETIC_TEST
const pvz::native::SyntheticInputs<PPCContext> CurrentFlagsInputs{
    "self=0x1000; focus byte and all four flag masks cover boundaries and 1024 seeded cases", 2 * 9 * 9 * 9,
    1024, 42, [](size_t i, PPCContext& c, uint8_t* base, uint64_t& seed) {
      constexpr uint32_t words[]{0,          1,          2,          0x80000000, 0xFFFFFFFF,
                                 0x7FFFFFFF, 0x55555555, 0xAAAAAAAA, 0x10000000};
      c.r3.u64 = 0x1000;
      base[0x10A0] = uint8_t(i % 2);
      size_t n = i / 2;
      for (uint32_t off : {0x1228u, 0x10B8u, 0x10BCu}) {
        const uint32_t v = i < 2 * 9 * 9 * 9 ? words[n % 9] : uint32_t(Random(seed));
        if (i < 2 * 9 * 9 * 9)
          n /= 9;
        pvz::guest::Write(base, 0x1000 + (off - 0x1000), v);
      }
    }};
const pvz::native::SyntheticInputs<PPCContext> ManagerGetFlagsInputs{
    "self=0x1000; distinct and eight aliased outputs; full-word modal pointer 0/1; five mask boundary "
    "product plus alias boundaries and 2048 seeded cases",
    4 * 9 * 9 * 9 * 9 * 9 + 9 * 4 * 9, 2048, 42, [](size_t i, PPCContext& c, uint8_t* base, uint64_t& seed) {
      constexpr uint32_t words[]{0,          1,          2,          0x80000000, 0xFFFFFFFF,
                                 0x7FFFFFFF, 0x55555555, 0xAAAAAAAA, 0x10000000};
      c.r3.u64 = 0x1000;
      constexpr uint32_t outputs[]{0x2000, 0x1000, 0x1098, 0x109C, 0x10B0, 0x10B8, 0x10C0, 0x10C4, 0x1220};
      constexpr size_t word_product = 9 * 9 * 9 * 9 * 9;
      constexpr size_t alias_boundaries = std::size(outputs) * 4 * std::size(words);
      const bool exhaustive = i < 4 * word_product;
      const bool bounded = i < 4 * word_product + alias_boundaries;
      const size_t alias_case = exhaustive ? 0 : i - 4 * word_product;
      c.r4.u64 =
          exhaustive ? outputs[0] : outputs[(bounded ? alias_case : Random(seed)) % std::size(outputs)];
      base[0x10A0] = uint8_t(exhaustive ? (i / word_product) % 2
                             : bounded  ? (alias_case / std::size(outputs)) % 2
                                        : Random(seed));
      pvz::guest::Write(base, 0x10B4,
                        uint32_t(exhaustive ? (i / (2 * word_product)) % 2
                                 : bounded  ? (alias_case / (2 * std::size(outputs))) % 2
                                            : Random(seed)));
      uint32_t offsets[]{0x1228, 0x10B8, 0x10BC, 0x10C0, 0x10C4};
      size_t n = exhaustive ? i : alias_case / (4 * std::size(outputs));
      for (uint32_t off : offsets) {
        uint32_t v;
        if (exhaustive) {
          v = words[n % 9];
          n /= 9;
        } else if (bounded) {
          v = words[n++ % std::size(words)];
        } else {
          v = uint32_t(Random(seed));
        }
        pvz::guest::Write(base, 0x1000 + (off - 0x1000), v);
      }
    }};
#endif

#ifndef PVZ_BATCH_SYNTHETIC_TEST
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
using pvz::native::Virtual;
uint32_t Read(uint8_t* b, uint32_t at) {
  return pvz::guest::Read(b, at);
}
void Write(uint8_t* b, uint32_t at, uint32_t v) {
  pvz::guest::Write(b, at, v);
}
uint64_t Read64(uint8_t* b, uint32_t at) {
  return uint64_t(Read(b, at)) << 32 | Read(b, at + 4);
}
void Pair(uint8_t* b, uint32_t at, uint32_t owner, uint32_t node) {
  Write(b, at, owner);
  Write(b, at + 4, node);
}
auto& Container(uint8_t* b, uint32_t self) {
  return *reinterpret_cast<pvz::xbox::WidgetContainer*>(b + self);
}
auto& Manager(uint8_t* b, uint32_t self) {
  return *reinterpret_cast<pvz::xbox::WidgetManagerFlags*>(b + self);
}
void IteratorOwner(PPCContext& c, uint8_t* b, uint32_t owner, uint32_t list) {
  if (!owner || owner != Read(b, list))
    ppc_trap(c, b, 22);
}
void IteratorElement(PPCContext& c, uint8_t* b, uint32_t owner, uint32_t node) {
  if (!owner)
    ppc_trap(c, b, 22);
  if (node == Read(b, Read(b, owner) + 0x14))
    ppc_trap(c, b, 22);
}
void Native_82491E90(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, inputFlags = c.r6.u32, found = c.r7.u32,
                 outX = c.r8.u32, outY = c.r9.u32;
  Frame f(c, b, 0xE0);
  const uint32_t sp = c.r1.u32;
  auto& w = Container(b, self);
  Pair(b, sp + 0x58, w.list_proxy, w.children_head);
  const uint32_t owner = Read(b, sp + 0x58),
                 flags = (inputFlags | uint32_t(w.flags_add)) & ~uint32_t(w.flags_remove);
  uint32_t node = Read(b, sp + 0x5C);
  bool afterModal = false;
  for (;;) {
    const uint32_t first = Read(b, uint32_t(w.children_head));
    IteratorOwner(c, b, owner, self + 4);
    if (node == first) {
      c.r3.u64 = 0;
      b[found] = 0;
      return;
    }
    Call(c, b, sub_82493F70, 0x82491F20, {sp + 0x58});
    const uint32_t child = Read(b, c.r3.u32);
    const auto& childContainer = Container(b, child);
    uint32_t childFlags =
        (uint32_t(childContainer.flags_add) | flags) & ~uint32_t(childContainer.flags_remove);
    if (afterModal) {
      const uint32_t manager = w.manager;
      childFlags = (Read(b, manager + 0xC0) | childFlags) & ~Read(b, manager + 0xC4);
    }
    if ((childFlags & 0x10) && b[child + 0x70]) {
      Call(c, b, sub_82491E90, 0x82491F88,
           {child, x - uint32_t(childContainer.x), y - uint32_t(childContainer.y), childFlags, sp + 0x50,
            outX, outY});
      if (c.r3.u32 || b[sp + 0x50]) {
        b[found] = 1;
        return;
      }
      if (b[child + 0x71]) {
        Args(c, {sp + 0x60, child});
        Virtual(c, b, child, 0x118, 0x82491FC0);
        const uint32_t rect = c.r3.u32, rx = Read(b, rect), ry = Read(b, rect + 4);
        const bool in = int32_t(x) >= int32_t(rx) && int32_t(x) < int32_t(rx + Read(b, rect + 8)) &&
                        int32_t(y) >= int32_t(ry) && int32_t(y) < int32_t(ry + Read(b, rect + 12));
        if (in) {
          b[found] = 1;
          Args(c, {child, x - uint32_t(childContainer.x), y - uint32_t(childContainer.y)});
          Virtual(c, b, child, 0xF4, 0x82492030);
          if (c.r3.u8) {
            if (outX)
              Write(b, outX, x - uint32_t(childContainer.x));
            if (outY)
              Write(b, outY, y - uint32_t(childContainer.y));
            c.r3.u64 = child;
            return;
          }
        }
      }
    }
    afterModal = afterModal || (Read(b, uint32_t(w.manager) + 0xB4) == child);
    if (!owner)
      ppc_trap(c, b, 22);
    node = Read(b, node + 4);
    Write(b, sp + 0x5C, node);
    if (node == Read(b, Read(b, owner) + 0x14))
      ppc_trap(c, b, 22);
  }
}
void Native_824920D8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, a = c.r4.u32, d = c.r5.u32, found = c.r6.u32;
  Frame f(c, b, 0x90);
  const uint32_t owner = Read(b, self + 4);
  uint32_t node = Read(b, Read(b, self + 0x18));
  for (;;) {
    const uint32_t head = Read(b, self + 0x18);
    IteratorOwner(c, b, owner, self + 4);
    if (node == head) {
      c.r3.u64 = 0;
      return;
    }
    IteratorElement(c, b, owner, node);
    const uint32_t child = Read(b, node + 8);
    if (child == a) {
      c.r3.u64 = 1;
      b[found] = 1;
      return;
    }
    if (child == d) {
      c.r3.u64 = 0;
      b[found] = 1;
      return;
    }
    Call(c, b, sub_824920D8, 0x82492168, {child, a, d, found});
    if (b[found])
      return;
    IteratorElement(c, b, owner, node);
    node = Read(b, node);
  }
}
void Native_824921D0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, a = c.r4.u32, d = c.r5.u32;
  Frame f(c, b, 0x60);
  b[c.r1.u32 + 0x50] = 0;
  Call(c, b, sub_824920D8, 0x824921EC, {self, a, d, c.r1.u32 + 0x50});
}
void Native_824939B0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, child = c.r4.u32;
  Frame f(c, b, 0x80);
  const uint32_t sp = c.r1.u32;
  Write(b, sp + 0x9C, child);
  const uint32_t head = Read(b, self + 0x18), proxy = Read(b, self + 4);
  Pair(b, sp + 0x50, proxy, head);
  Pair(b, sp + 0x58, proxy, Read(b, head));
  c.r3.u64 = sp + 0x50;
  c.r4.u64 = Read64(b, sp + 0x58);
  c.r5.u64 = Read64(b, sp + 0x50);
  c.r6.u64 = sp + 0x9C;
  Call(c, b, sub_82494040, 0x82493A04, {});
  const uint32_t owner = Read(b, sp + 0x50), node = Read(b, sp + 0x54);
  IteratorOwner(c, b, owner, self + 4);
  if (node == Read(b, self + 0x18))
    return;
  IteratorOwner(c, b, owner, self + 0x2C);
  if (node == Read(b, self + 0x30)) {
    const uint32_t currentOwner = Read(b, self + 0x2C), currentNode = Read(b, self + 0x30);
    IteratorElement(c, b, currentOwner, currentNode);
    Write(b, self + 0x30, Read(b, currentNode));
    b[self + 0x28] = 1;
  }
  c.r3.u64 = sp + 0x58;
  c.r4.u64 = self + 4;
  c.r5.u64 = Read64(b, sp + 0x50);
  Call(c, b, sub_82489398, 0x82493A9C, {});
  Pair(b, sp + 0x58, Read(b, self + 4), Read(b, Read(b, self + 0x18)));
  Call(c, b, sub_82493620, 0x82493AC0, {self, sp + 0x58, child});
  Args(c, {child});
  Virtual(c, b, child, 0x54, 0x82493AD4);
}
template <bool EnabledOnly, uint32_t Lr> void FindWidget(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, outX = c.r6.u32, outY = c.r7.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82491E90, Lr, {self, x, y, CurrentWidgetFlags(b, self), c.r1.u32 + 0x50, outX, outY});
  if constexpr (EnabledOnly)
    if (c.r3.u32 && b[c.r3.u32 + 0x72])
      c.r3.u64 = 0;
}
void Native_824A5AB8(PPCContext& c, uint8_t* b) {
  FindWidget<false, 0x824A5AF8>(c, b);
}
void Native_824A5B08(PPCContext& c, uint8_t* b) {
  FindWidget<true, 0x824A5B48>(c, b);
}
void Native_824A5D88(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, modal = c.r4.u32, masks = c.r5.u32;
  Frame f(c, b, 0x70);
  auto& m = Manager(b, self);
  m.modal_widget = modal;
  m.modal_flags_add = Read(b, masks);
  m.modal_flags_remove = Read(b, masks + 4);
  uint32_t target = m.mouse_over;
  if (target && (uint32_t(m.modal_flags_remove) & 0x10)) {
    Call(c, b, sub_824921D0, 0x824A5DDC, {self, target, modal});
    if (c.r3.u8) {
      target = m.mouse_over;
      m.mouse_over = 0;
      Call(c, b, sub_824A5D20, 0x824A5DF4, {self, target});
    }
  }
  target = m.mouse_down;
  if (target && (uint32_t(m.modal_flags_remove) & 0x10)) {
    Call(c, b, sub_824921D0, 0x824A5E18, {self, target, uint32_t(m.modal_widget)});
    if (c.r3.u8) {
      target = m.mouse_down;
      const uint32_t buttons = m.pressed_buttons;
      m.pressed_buttons = 0;
      m.mouse_down = 0;
      Call(c, b, sub_824A5BA0, 0x824A5E38, {self, target, buttons});
    }
  }
  target = m.focus;
  if (target && (uint32_t(m.modal_flags_remove) & 0x20)) {
    Call(c, b, sub_824921D0, 0x824A5E5C, {self, target, uint32_t(m.modal_widget)});
    if (c.r3.u8) {
      target = m.focus;
      m.focus = 0;
      Args(c, {target});
      Virtual(c, b, target, 0x98, 0x824A5E7C);
    }
  }
  target = m.gamepad_focus;
  if (target && (uint32_t(m.modal_flags_remove) & 0x20)) {
    Call(c, b, sub_824921D0, 0x824A5EA0, {self, target, uint32_t(m.modal_widget)});
    if (c.r3.u8) {
      target = m.gamepad_focus;
      m.gamepad_focus = 0;
      Args(c, {target});
      Virtual(c, b, target, 0xAC, 0x824A5EC0);
    }
  }
}
void Native_824A5FE8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, target = c.r4.u32;
  Frame f(c, b, 0x70);
  auto& m = Manager(b, self);
  const uint32_t previous = m.focus;
  if (target == previous) {
    c.r3.u64 = target;
    return;
  }
  uint32_t receiver = m.app;
  if (receiver) {
    Args(c, {receiver, target, previous});
    Virtual(c, b, receiver, 0x194, 0x824A6038);
  }
  receiver = m.focus;
  if (receiver) {
    Args(c, {receiver});
    Virtual(c, b, receiver, 0x98, 0x824A6054);
  }
  if (target && uint32_t(Container(b, target).manager) == self) {
    m.focus = target;
    if (m.current_mode) {
      Args(c, {target});
      Virtual(c, b, target, 0x94, 0x824A608C);
      return;
    }
  } else
    m.focus = 0;
  c.r3.u64 = 0;
}
void Native_824A67E8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, key = c.r4.u32;
  Frame f(c, b, 0x60);
  auto& m = Manager(b, self);
  m.last_key_update = m.container.update_count;
  if (int32_t(key) >= 0 && int32_t(key) < 255)
    m.key_states[key] = 0;
  if (!(key == 9 && m.key_states[17])) {
    const uint32_t focus = m.focus;
    if (focus) {
      Args(c, {focus, key});
      Virtual(c, b, focus, 0xA4, 0x824A6848);
    }
  }
  c.r3.u64 = 1;
}
void Native_824A68F0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, widget = c.r4.u32;
  Frame f(c, b, 0x70);
  auto& m = Manager(b, self);
  if (uint32_t(m.mouse_over) == widget) {
    const uint32_t old = m.mouse_over;
    m.mouse_over = 0;
    Call(c, b, sub_824A5D20, 0x824A691C, {self, old});
  }
  if (uint32_t(m.mouse_down) == widget) {
    const uint32_t old = m.mouse_down;
    m.mouse_down = 0;
    Call(c, b, sub_824A5BA0, 0x824A6938, {self, old, uint32_t(m.pressed_buttons)});
    m.pressed_buttons = 0;
  }
  if (uint32_t(m.focus) == widget) {
    const uint32_t old = m.focus;
    m.focus = 0;
    Args(c, {old});
    Virtual(c, b, old, 0x98, 0x824A695C);
  }
  if (uint32_t(m.gamepad_focus) == widget)
    Call(c, b, sub_824A5F48, 0x824A6974, {self, 0});
  if (uint32_t(m.modal_widget) == widget)
    m.modal_widget = 0;
}
void Native_824A69E0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x80);
  auto& m = Manager(b, self);
  if (!m.current_mode)
    return;
  m.update_flags = 0;
  for (uint32_t key = 0; key < 255; ++key)
    if (m.key_states[key])
      Call(c, b, sub_824A67E8, 0x824A6A24, {self, key});
  const uint32_t focus = m.focus;
  m.current_mode = 0;
  c.r3.u64 = focus;
  if (focus)
    Virtual(c, b, focus, 0x98, 0x824A6A50);
}
void Native_824A6D00(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Write(b, self, 0x8206404C);
  Call(c, b, sub_82564B78, 0x824A6D28, {self + 0xD0});
  Call(c, b, sub_82564B78, 0x824A6D30, {self + 0x84});
  Call(c, b, sub_82492FD8, 0x824A6D38, {self});
}
void Native_824A6D58(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, app = c.r4.u32;
  Frame f(c, b, 0x80);
  const uint32_t sp = c.r1.u32;
  Write(b, sp + 0x94, self);
  Call(c, b, sub_82492F28, 0x824A6D78, {self, app});
  Write(b, self, 0x8206404C);
  Call(c, b, sub_825557B0, 0x824A6D8C, {self + 0x84});
  for (uint32_t offset : {0xB8u, 0xBCu, 0xC0u, 0xC4u, 0xC8u, 0xCCu})
    Write(b, self + offset, 0);
  Write(b, sp + 0x50, self + 0xB8);
  Write(b, sp + 0x50, self + 0xC0);
  Write(b, sp + 0x50, self + 0xC8);
  Call(c, b, sub_825557B0, 0x824A6DC8, {self + 0xD0});
  for (uint32_t offset : {0xE8u, 0xECu, 0xF0u, 0xF4u, 0xF8u, 0xFCu, 0x100u, 0x104u})
    Write(b, self + offset, 0);
  Write(b, sp + 0x50, self + 0xE8);
  Write(b, self + 0x70, app);
  Write(b, self + 0x9C, 0x7FFFFFFF);
  Write(b, self + 0x20, self);
  b[self + 0x108] = 0;
  for (uint32_t offset : {0x68u, 0x74u, 0x80u, 0xA4u, 0xA8u, 0xACu, 0xB0u, 0xB4u})
    Write(b, self + offset, 0);
  b[self + 0x7C] = 0;
  Write(b, self + 0xCC, 0x30);
  Write(b, self + 0x48, 0);
  Write(b, self + 0x4C, 0);
  Write(b, sp + 0x50, self + 0xF8);
  b[self + 0xA0] = 1;
  for (uint32_t offset : {0x38u, 0x224u, 0x114u, 0x118u, 0x120u})
    Write(b, self + offset, 0);
  Write(b, self + 0x228, 0x35);
  for (uint32_t i = 0; i < 255; ++i)
    b[self + 0x124 + i] = 0;
  c.r3.u64 = self;
}
void Native_824A7CB0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824A6D00, 0x824A7CD0, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x824A7CE0, {self});
  c.r3.u64 = self;
}
#endif
} // namespace

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#define LEAF(A, P, NATIVE, OUTPUTS, RANGES, INPUTS)                                                          \
  PVZ_NATIVE_BATCH_REPLACEMENT(b013, P, manager_##A, sub_##A, NATIVE,                                        \
                               pvz::native::Safety::DeclaredOutputsOnly, OUTPUTS, RANGES)
#define SNAP(A, P, NATIVE)                                                                                   \
  PVZ_NATIVE_BATCH_REPLACEMENT(b013, P, manager_##A, sub_##A, NATIVE, pvz::native::Safety::SnapshotsOnly,    \
                               noOutputs, NoRanges)
#endif

LEAF(824A5A88, true, WidgetManagerCurrentFlags, r3out, NoRanges, CurrentFlags)
LEAF(824A60F0, true, WidgetManagerGetFlags, noOutputs, FlagsRange, ManagerGetFlags)

SNAP(82491E90, true, Native_82491E90)
SNAP(824920D8, true, Native_824920D8)
SNAP(824921D0, true, Native_824921D0)
SNAP(824939B0, true, Native_824939B0)
SNAP(824A5AB8, false, Native_824A5AB8)
SNAP(824A5B08, true, Native_824A5B08)
SNAP(824A5D88, true, Native_824A5D88)
SNAP(824A5FE8, true, Native_824A5FE8)
SNAP(824A67E8, false, Native_824A67E8)
SNAP(824A68F0, true, Native_824A68F0)
SNAP(824A69E0, false, Native_824A69E0)
SNAP(824A6D00, false, Native_824A6D00)
SNAP(824A6D58, true, Native_824A6D58)
SNAP(824A7CB0, false, Native_824A7CB0)

#undef LEAF
#undef SNAP
