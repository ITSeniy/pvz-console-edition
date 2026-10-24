// B008. Complete Xbox 584109FF / 0.0.1.4 translations; T17 evidence manifest.
#include "native/replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/Dialog.h"
#include <array>
#ifndef PVZ_BATCH_SYNTHETIC_TEST
#include "native/guest_call.h"
PVZ_NATIVE_BATCH(b008)
REX_EXTERN(sub_82125760);
REX_EXTERN(sub_821257D8);
REX_EXTERN(sub_821264E0);
REX_EXTERN(sub_821269C0);
REX_EXTERN(sub_82472958);
REX_EXTERN(sub_824729F8);
REX_EXTERN(sub_82472BD8);
REX_EXTERN(sub_82473040);
REX_EXTERN(sub_824730D0);
REX_EXTERN(sub_82473C60);
REX_EXTERN(sub_82473D68);
REX_EXTERN(sub_82473E88);
REX_EXTERN(sub_82473FF8);
REX_EXTERN(sub_82475EB8);
REX_EXTERN(sub_824918B8);
REX_EXTERN(sub_82491928);
REX_EXTERN(sub_824919C8);
REX_EXTERN(sub_82491A90);
REX_EXTERN(sub_824922F0);
REX_EXTERN(sub_824923C8);
REX_EXTERN(sub_824930D0);
REX_EXTERN(sub_82493C50);
REX_EXTERN(sub_82493E10);
REX_EXTERN(sub_8249A0B8);
REX_EXTERN(sub_8249A0C0);
REX_EXTERN(sub_8249A100);
REX_EXTERN(sub_8249B650);
REX_EXTERN(sub_8249B730);
REX_EXTERN(sub_8249DBE0);
REX_EXTERN(sub_8249DD50);
REX_EXTERN(sub_8249E3C0);
REX_EXTERN(sub_824A5F48);
REX_EXTERN(sub_824A7F28);
REX_EXTERN(sub_824A7FE8);
REX_EXTERN(sub_824A8060);
REX_EXTERN(sub_824A80D0);
REX_EXTERN(sub_824A8140);
REX_EXTERN(sub_824A81A0);
REX_EXTERN(sub_824A8200);
REX_EXTERN(sub_824A8260);
REX_EXTERN(sub_824A8588);
REX_EXTERN(sub_824A85F0);
REX_EXTERN(sub_824A8730);
REX_EXTERN(sub_824A87A0);
REX_EXTERN(sub_824A87A8);
REX_EXTERN(sub_824A8850);
REX_EXTERN(sub_824A8888);
REX_EXTERN(sub_824A88C0);
REX_EXTERN(sub_824A88C8);
REX_EXTERN(sub_824A88D0);
REX_EXTERN(sub_824A8900);
REX_EXTERN(sub_824A8908);
REX_EXTERN(sub_824A8960);
REX_EXTERN(sub_824A8B28);
REX_EXTERN(sub_824A92D8);
REX_EXTERN(sub_824A94E0);
REX_EXTERN(sub_824A96D8);
REX_EXTERN(sub_824A9B50);
REX_EXTERN(sub_824DCF20);
REX_EXTERN(sub_824DD5E0);
REX_EXTERN(sub_8254C4A0);
REX_EXTERN(sub_8254C4B8);
REX_EXTERN(sub_82581B18);
REX_EXTERN(sub_825DDB78);
REX_EXTERN(sub_825F8F50);
REXCVAR_DECLARE(bool, pvz_native_dialog_824A96D8_inject_error);
#endif

namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 0> noOutputs{};
const std::array r3out{RegisterOutput{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}};
const auto NoRanges = [](const PPCContext&) { return std::array<Range, 0>{}; };
auto& D(uint8_t* base, uint32_t address) {
  return *reinterpret_cast<pvz::xbox::Dialog*>(base + address);
}
void Modal(PPCContext& c, uint8_t* b) {
  c.r3.u64 = D(b, c.r3.u32).modal;
}
void Identity(PPCContext&, uint8_t*) {}
#ifndef PVZ_BATCH_SYNTHETIC_TEST
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
using pvz::native::Virtual;
// The target object can differ from r3 for guest methods with a hidden result pointer.
void V(PPCContext& c, uint8_t* b, uint32_t object, uint32_t slot, uint32_t lr,
       std::initializer_list<uint32_t> args) {
  Args(c, args);
  Virtual(c, b, object, slot, lr);
}
auto& W(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::Widget*>(b + p);
}
auto& Button(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::DialogButtonMetrics*>(b + p);
}
uint32_t App(uint8_t* b) {
  return pvz::guest::Read(b, 0x828923E8);
}
void Native_824A7F28(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, index = c.r4.u32, color = c.r5.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82473FF8, 0x824A7F44, {self, index, color});
  if (index != 3 && index != 4)
    return;
  if (const uint32_t yes = D(b, self).yes_button)
    V(c, b, yes, 0x64, index == 3 ? 0x824A7F74 : 0x824A7FB8, {yes, index - 3, color});
  if (const uint32_t no = D(b, self).no_button)
    V(c, b, no, 0x64, 0x824A7FE0, {no, index - 3, color});
}
void Native_824A7FE8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, value = c.r4.u32;
  Frame f(c, b, 0x70);
  if (const uint32_t yes = D(b, self).yes_button)
    V(c, b, yes, 0x124, 0x824A8024, {yes, value});
  if (const uint32_t no = D(b, self).no_button)
    V(c, b, no, 0x124, 0x824A8048, {no, value});
}
template <bool Header> void SetFont(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, source = c.r4.u32;
  Frame f(c, b, 0x70);
  const uint32_t old = Header ? uint32_t(D(b, self).header_font) : uint32_t(D(b, self).body_font);
  if (old)
    V(c, b, old, 0, Header ? 0x824A809C : 0x824A810C, {old, 1});
  V(c, b, source, 0x2C, Header ? 0x824A80B0 : 0x824A8120, {source});
  if constexpr (Header)
    D(b, self).header_font = c.r3.u32;
  else
    D(b, self).body_font = c.r3.u32;
}
template <bool Add> void ManagerChanged(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, manager = c.r4.u32;
  Frame f(c, b, 0x70);
  if constexpr (Add)
    Call(c, b, sub_824923C8, 0x824A81C0, {self, manager});
  else
    Call(c, b, sub_824922F0, 0x824A8160, {self, manager});
  if (const uint32_t yes = D(b, self).yes_button) {
    if constexpr (Add)
      Call(c, b, sub_824930D0, 0x824A81D4, {manager, yes});
    else
      Call(c, b, sub_82493E10, 0x824A8174, {manager, yes});
  }
  if (const uint32_t no = D(b, self).no_button) {
    if constexpr (Add)
      Call(c, b, sub_824930D0, 0x824A81E8, {manager, no});
    else
      Call(c, b, sub_82493E10, 0x824A8188, {manager, no});
  }
}
void Native_824A8200(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82472958, 0x824A8218, {self});
  if (const uint32_t yes = D(b, self).yes_button)
    Call(c, b, sub_82493C50, 0x824A8230, {uint32_t(W(b, self).manager), yes, self});
  if (const uint32_t no = D(b, self).no_button)
    Call(c, b, sub_82493C50, 0x824A8248, {uint32_t(W(b, self).manager), no, self});
}
uint32_t LabelWidth(PPCContext& c, uint8_t* b, uint32_t self, bool NoFont, uint32_t lr) {
  const uint32_t yes = D(b, self).yes_button, button = NoFont ? uint32_t(D(b, self).no_button) : yes;
  const uint32_t font = Button(b, button).font;
  // Xbox deliberately measures the yes-label through the no-button's font too.
  V(c, b, font, 0x1C, lr, {font, yes + 0xD4});
  return c.r3.u32 + 40;
}
void Native_824A8260(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x90);
  Call(c, b, sub_824729F8, 0x824A8274, {self});
  if (!uint32_t(D(b, self).yes_button))
    return;
  auto& d = D(b, self);
  uint32_t width = uint32_t(W(b, self).width) - 2 * uint32_t(d.button_spacing) - uint32_t(d.field_194) -
                   uint32_t(d.content_insets[2]) - uint32_t(d.content_insets[0]) -
                   uint32_t(d.background_insets[2]) - uint32_t(d.background_insets[0]);
  if (int32_t(width) >= 200)
    width = 200;
  const bool pair = uint32_t(d.no_button) != 0;
  if (int32_t(width) <= int32_t(LabelWidth(c, b, self, false, pair ? 0x824A82F0 : 0x824A84B8)))
    width = LabelWidth(c, b, self, false, pair ? 0x824A8318 : 0x824A84E0);
  if (pair && int32_t(width) <= int32_t(LabelWidth(c, b, self, true, 0x824A833C)))
    width = LabelWidth(c, b, self, true, 0x824A8368);
  const uint32_t available = uint32_t(W(b, self).width) - 2 * uint32_t(d.button_spacing) -
                             uint32_t(d.background_insets[0]) - uint32_t(d.content_insets[0]) -
                             uint32_t(d.content_insets[2]) - uint32_t(d.background_insets[2]);
  const uint32_t x = uint32_t(W(b, self).x) + uint32_t(int32_t(available) / 2) -
                     uint32_t(int32_t(width) / 2) + uint32_t(d.background_insets[0]) +
                     uint32_t(d.content_insets[0]) + uint32_t(d.button_spacing);
  const uint32_t height = d.button_height;
  const uint32_t y = pair ? uint32_t(W(b, self).y) + uint32_t(W(b, self).height) +
                                uint32_t(d.content_insets[3]) + 2 * height -
                                uint32_t(d.background_insets[3]) - uint32_t(d.button_spacing)
                          : uint32_t(W(b, self).y) + uint32_t(d.content_insets[3]) -
                                uint32_t(W(b, self).height) - uint32_t(d.background_insets[3]) - height;
  const uint32_t yes = d.yes_button;
  V(c, b, yes, 0x7C, pair ? 0x824A8418 : 0x824A857C, {yes, x, y, width, height});
  if (pair) {
    const uint32_t no = d.no_button;
    V(c, b, no, 0x7C, 0x824A857C,
      {no, x, uint32_t(W(b, uint32_t(d.yes_button)).y) + height + uint32_t(d.button_spacing), width, height});
  }
}
template <bool Down> void MouseButton(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, clicks = c.r6.u32, button = c.r7.u32;
  Frame f(c, b, 0x80);
  if ((Down && button == 1) || (!Down && D(b, self).dragging)) {
    const uint32_t manager = W(b, self).manager;
    const uint32_t app = pvz::guest::Read(b, manager + 0x70);
    Call(c, b, sub_82475EB8, Down ? 0x824A85C0 : 0x824A876C, {app, Down ? 2u : 0u});
    D(b, self).dragging = Down;
    if constexpr (Down) {
      D(b, self).drag_x = x;
      D(b, self).drag_y = y;
    }
  }
  if constexpr (Down)
    Call(c, b, sub_82473040, 0x824A85E8, {self, x, y, clicks, button});
  else
    Call(c, b, sub_824730D0, 0x824A878C, {self, x, y, clicks, button});
}
void Native_824A87A8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  const bool kill = c.r4.u8 != 0;
  Frame f(c, b, 0x70);
  D(b, self).waiting = 1;
  for (;;) {
    const uint32_t app = App(b);
    V(c, b, app, 0x184, 0x824A87E0, {app, 0});
    if (!c.r3.u8 || !uint32_t(W(b, self).manager) || uint32_t(D(b, self).result) != 0x7FFFFFFF)
      break;
    if (reinterpret_cast<pvz::xbox::DialogAppState*>(b + App(b))->field_800)
      break;
  }
  D(b, self).waiting = 0;
  if (kill) {
    const uint32_t app = App(b);
    V(c, b, app, 0x140, 0x824A8840, {app, uint32_t(W(b, self).id)});
  }
  c.r3.u64 = uint32_t(D(b, self).result);
}
template <bool Depress> void Listener(PPCContext& c, uint8_t* b) {
  const uint32_t interface = c.r3.u32, id = c.r4.u32, self = interface - 0xD4;
  if (id != 1000 && id != 1001)
    return;
  if constexpr (Depress)
    D(b, self).result = id;
  const uint32_t target = D(b, self).listener;
  V(c, b, target, Depress ? 4 : 0, uint32_t(c.lr), {target, uint32_t(W(b, self).id), id});
}
void Native_824A88D0(PPCContext& c, uint8_t* b) {
  if (!(c.r6.u32 & 1) && c.r4.s32 == 7) {
    const uint32_t listener = c.r3.u32 + 0xD4;
    V(c, b, listener, 8, uint32_t(c.lr), {listener, 1001});
  }
}
void Native_824A8908(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82472BD8, 0x824A8920, {self});
  uint32_t button = D(b, self).yes_button;
  if (!button)
    button = D(b, self).no_button;
  if (button)
    Call(c, b, sub_824A5F48, 0x824A8940, {uint32_t(W(b, self).manager), button});
}
void Native_824A8960(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  pvz::guest::Write(b, c.r1.u32 + 0x84, self);
  W(b, self).vtable = 0x820644BC;
  W(b, self).listener_vtable = 0x82064484;
  D(b, self).button_listener_vtable = 0x82064464;
  for (uint32_t i = 0; i < 4; ++i) {
    const uint32_t p = i == 0   ? uint32_t(D(b, self).yes_button)
                       : i == 1 ? uint32_t(D(b, self).no_button)
                       : i == 2 ? uint32_t(D(b, self).header_font)
                                : uint32_t(D(b, self).body_font);
    constexpr uint32_t lr[]{0x824A89C4, 0x824A89E4, 0x824A8A04, 0x824A8A24};
    if (p)
      V(c, b, p, 0, lr[i], {p, 1});
  }
  Call(c, b, sub_82125760, 0x824A8A34, {self + 0x128, 1, 0});
  Call(c, b, sub_82125760, 0x824A8A44, {self + 0x10C, 1, 0});
  Call(c, b, sub_82125760, 0x824A8A54, {self + 0xF0, 1, 0});
  Call(c, b, sub_82473C60, 0x824A8A5C, {self});
}
void Native_824A8B28(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, background = c.r4.u32, image = c.r5.u32, id = c.r6.u32, modal = c.r7.u8,
                 header = c.r8.u32, body = c.r9.u32, footer = c.r10.u32;
  Frame f(c, b, 0x210);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0x224, self);
  b[sp + 0x50] = uint8_t(modal);
  Call(c, b, sub_82473E88, 0x824A8B64, {self});
  D(b, self).button_listener_vtable = 0x82060CD8;
  W(b, self).vtable = 0x820644BC;
  W(b, self).listener_vtable = 0x82064484;
  D(b, self).button_listener_vtable = 0x82064464;
  const uint32_t listener = self + 0xD4;
  pvz::guest::Write(b, sp + 0x54, self + 0xF0);
  pvz::guest::Write(b, sp + 0x54, listener);
  Call(c, b, sub_82125760, 0x824A8BB4, {self + 0xF0, 0, 0});
  pvz::guest::Write(b, sp + 0x54, self + 0x10C);
  Call(c, b, sub_82125760, 0x824A8BCC, {self + 0x10C, 0, 0});
  pvz::guest::Write(b, sp + 0x54, self + 0x128);
  Call(c, b, sub_82125760, 0x824A8BE4, {self + 0x128, 0, 0});
  Call(c, b, sub_8254C4A0, 0x824A8BEC, {self + 0x15C});
  Call(c, b, sub_8254C4A0, 0x824A8BF4, {self + 0x16C});
  constexpr uint32_t keys[]{0x820641BC, 0x820641C0, 0x820641C4, 0x820641C8};
  constexpr uint32_t globals[]{0x828A33C0, 0x828A3388, 0x828A336C, 0x828A33A4};
  constexpr uint32_t narrow[]{0xC0, 0xA0, 0x60, 0x80}, wide[]{0xE0, 0x100, 0x120, 0x140};
  constexpr uint32_t a[]{0x824A8C08, 0x824A8C60, 0x824A8CB4, 0x824A8D08};
  constexpr uint32_t d[]{0x824A8C14, 0x824A8C6C, 0x824A8CC0, 0x824A8D14};
  constexpr uint32_t e[]{0x824A8C34, 0x824A8C88, 0x824A8CDC, 0x824A8D30};
  constexpr uint32_t g[]{0x824A8C44, 0x824A8C98, 0x824A8CEC, 0x824A8D40};
  constexpr uint32_t h[]{0x824A8C54, 0x824A8CA8, 0x824A8CFC, 0x824A8D50};
  for (uint32_t i = 0; i < 4; ++i) {
    Call(c, b, sub_821264E0, a[i], {sp + narrow[i], keys[i]});
    Call(c, b, sub_82581B18, d[i], {sp + wide[i], sp + narrow[i]});
    Call(c, b, sub_821257D8, e[i], {globals[i], c.r3.u32, 0, 0xFFFFFFFF});
    Call(c, b, sub_82125760, g[i], {sp + wide[i], 1, 0});
    Call(c, b, sub_821269C0, h[i], {sp + narrow[i], 1, 0});
  }
  auto& dlg = D(b, self);
  dlg.background = background;
  W(b, self).id = id;
  dlg.field_d8 = 0;
  dlg.result = 0x7FFFFFFF;
  dlg.modal = b[sp + 0x50];
  Call(c, b, sub_8254C4B8, 0x824A8D8C, {sp + 0x160, 24, 24, 24, 24});
  for (uint32_t i = 0; i < 4; ++i)
    dlg.content_insets[i] = pvz::guest::Read(b, c.r3.u32 + 4 * i);
  dlg.field_150 = 0;
  dlg.field_154 = 0;
  dlg.header_spacing = 10;
  dlg.button_spacing = 0;
  dlg.field_194 = 8;
  const uint32_t app = App(b);
  dlg.listener = app ? app + 4 : 0;
  dlg.alpha = 255;
  Call(c, b, sub_821257D8, 0x824A8E00, {self + 0xF0, header, 0, 0xFFFFFFFF});
  Call(c, b, sub_821257D8, 0x824A8E14, {self + 0x10C, footer, 0, 0xFFFFFFFF});
  const uint32_t mode = pvz::guest::Read(b, sp + 0x264);
  dlg.button_mode = mode;
  if (mode == 1 || mode == 2 || mode == 3) {
    Call(c, b, sub_825DDB78, mode == 3 ? 0x824A8E3C : 0x824A8EB4, {0x188});
    pvz::guest::Write(b, sp + 0x54, c.r3.u32);
    if (c.r3.u32)
      Call(c, b, sub_824DCF20, mode == 3 ? 0x824A8E58 : 0x824A8ED0, {c.r3.u32, image, 1000, listener});
    dlg.yes_button = c.r3.u32;
    if (mode == 3) {
      Call(c, b, sub_821257D8, 0x824A8E7C, {uint32_t(dlg.yes_button) + 0xD4, self + 0x10C, 0, 0xFFFFFFFF});
      dlg.no_button = 0;
      const uint32_t yes = dlg.yes_button;
      V(c, b, yes, 0xB4, 0x824A8E98, {yes, self});
    } else {
      Call(c, b, sub_825DDB78, 0x824A8EE8, {0x188});
      pvz::guest::Write(b, sp + 0x54, c.r3.u32);
      if (c.r3.u32)
        Call(c, b, sub_824DCF20, 0x824A8F04, {c.r3.u32, image, 1001, listener});
      dlg.no_button = c.r3.u32;
      const uint32_t yes = dlg.yes_button;
      V(c, b, yes, 0xB4, 0x824A8F28, {yes, self});
      const uint32_t no = dlg.no_button;
      V(c, b, no, 0xB4, 0x824A8F40, {no, self});
      V(c, b, uint32_t(dlg.yes_button), 0xB0, 0x824A8F64,
        {uint32_t(dlg.yes_button), 0, 0, 0, uint32_t(dlg.no_button)});
      V(c, b, uint32_t(dlg.no_button), 0xB0, 0x824A8F88,
        {uint32_t(dlg.no_button), 0, 0, uint32_t(dlg.yes_button), 0});
      const bool yesNo = int32_t(dlg.button_mode) == 1;
      Call(c, b, sub_821257D8, yesNo ? 0x824A8FAC : 0x824A8FBC,
           {uint32_t(dlg.yes_button) + 0xD4, globals[yesNo ? 0 : 2], 0, 0xFFFFFFFF});
      Call(c, b, sub_821257D8, 0x824A8FD4,
           {uint32_t(dlg.no_button) + 0xD4, globals[yesNo ? 1 : 3], 0, 0xFFFFFFFF});
    }
  } else {
    dlg.yes_button = 0;
    dlg.no_button = 0;
    dlg.field_ec = 0;
  }
  Call(c, b, sub_821257D8, 0x824A8FE8, {self + 0x128, body, 0, 0xFFFFFFFF});
  dlg.button_height = image ? pvz::guest::Read(b, image + 0x28) : 24;
  W(b, self).flag_76 = 1;
  W(b, self).dirty = 1;
  dlg.header_font = 0;
  dlg.body_font = 0;
  dlg.dragging = 0;
  reinterpret_cast<pvz::xbox::DialogWidgetFlags*>(b + self)->field_5c = 1;
  if (!image)
    for (uint32_t i = 0; i < 6; ++i)
      pvz::guest::Write(b, 0x827D2E48 + 0x24 + i * 4, 0);
  Call(c, b, sub_82473D68, 0x824A9058, {self, 0x827D2E48, 7});
  dlg.waiting = 0;
  c.r3.u64 = self;
}
void Native_824A92D8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0xE0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0x50, 0);
  for (uint32_t i = 0; i < 2; ++i) {
    if (i == 0 ? uint32_t(D(b, self).header_font) : uint32_t(D(b, self).body_font))
      continue;
    const uint32_t text = sp + (i == 0 ? 0x60 : 0x80);
    Call(c, b, sub_825DDB78, i == 0 ? 0x824A9314 : 0x824A9394, {0x20});
    const uint32_t font = c.r3.u32;
    pvz::guest::Write(b, sp + 0x54, font);
    if (font) {
      Call(c, b, sub_821264E0, i == 0 ? 0x824A932C : 0x824A93AC, {text, i == 0 ? 0x820641D0u : 0x820641E4u});
      pvz::guest::Write(b, sp + 0x50, i == 0 ? 1 : 2);
      Call(c, b, sub_824DD5E0, i == 0 ? 0x824A9358 : 0x824A93D8,
           {font, App(b), text, i == 0 ? 14u : 12u, 0, 0, 0, 0});
    }
    if (i == 0)
      D(b, self).header_font = c.r3.u32;
    else
      D(b, self).body_font = c.r3.u32;
    if (font)
      Call(c, b, sub_821269C0, i == 0 ? 0x824A9380 : 0x824A93FC, {text, 1, 0});
  }
}
void Native_824A94E0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, width = c.r4.u32;
  Frame f(c, b, 0x120);
  Call(c, b, sub_824A92D8, 0x824A94FC, {self});
  auto& d = D(b, self);
  uint32_t height = uint32_t(d.content_insets[3]) + uint32_t(d.content_insets[1]) +
                    uint32_t(d.background_insets[3]) + uint32_t(d.background_insets[1]);
  bool text = false;
  if (uint32_t(d.header.length)) {
    V(c, b, uint32_t(d.header_font), 8, 0x824A953C, {uint32_t(d.header_font)});
    height -= c.r3.u32;
    V(c, b, uint32_t(d.header_font), 0x10, 0x824A9558, {uint32_t(d.header_font)});
    height += c.r3.u32;
    text = true;
  }
  if (uint32_t(d.body.length)) {
    if (text)
      height += uint32_t(d.header_spacing);
    const uint32_t graphics = c.r1.u32 + 0x50;
    Call(c, b, sub_8249DBE0, 0x824A9588, {graphics, 0});
    Call(c, b, sub_8249A0B8, 0x824A9594, {graphics, uint32_t(d.body_font)});
    V(c, b, uint32_t(d.body_font), 0x18, 0x824A95AC, {uint32_t(d.body_font)});
    const uint32_t bodyWidth = width - uint32_t(d.content_insets[2]) - uint32_t(d.content_insets[0]) -
                               uint32_t(d.background_insets[2]) - uint32_t(d.background_insets[0]) - 4;
    V(c, b, self, 0x108, 0x824A95F0,
      {self, graphics, bodyWidth, self + 0x128, c.r3.u32 + uint32_t(d.field_154)});
    height += c.r3.u32;
    text = true;
    Call(c, b, sub_8249DD50, 0x824A9600, {graphics});
  }
  if (uint32_t(d.footer.length) && int32_t(d.button_mode) != 3) {
    if (text)
      height += 8;
    V(c, b, uint32_t(d.header_font), 0x18, 0x824A9638, {uint32_t(d.header_font)});
    height += c.r3.u32;
    text = true;
  }
  if (uint32_t(d.yes_button) && W(b, uint32_t(d.yes_button)).visible) {
    if (text)
      height += 8;
    height += uint32_t(d.button_height) + 8;
  }
  if (uint32_t(d.no_button) && W(b, uint32_t(d.no_button)).visible) {
    if (text)
      height += 8;
    height += uint32_t(d.button_height) + 8;
  }
  c.r3.u64 = height;
}
void Native_824A9B50(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824A8960, 0x824A9B70, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x824A9B80, {self});
  c.r3.u64 = self;
}
void Native_824A96D8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0xE0);
  const uint32_t sp = c.r1.u32;
  Call(c, b, sub_824A92D8, 0x824A96F0, {self});
  auto& d = D(b, self);
  const bool inject = REXCVAR_GET(pvz_native_dialog_824A96D8_inject_error);
  pvz::guest::Write(b, sp + 0x50,
                    uint32_t(d.background_insets[0]) + uint32_t(inject && uint32_t(d.background)));
  pvz::guest::Write(b, sp + 0x54, uint32_t(d.background_insets[1]));
  pvz::guest::Write(b, sp + 0x58,
                    uint32_t(W(b, self).width) - uint32_t(d.background_insets[0]) -
                        uint32_t(d.background_insets[2]));
  pvz::guest::Write(b, sp + 0x5C,
                    uint32_t(W(b, self).height) - uint32_t(d.background_insets[3]) -
                        uint32_t(d.background_insets[1]));
  Call(c, b, sub_8249A100, uint32_t(d.background) ? 0x824A9740 : 0x824A9774, {graphics, 1});
  if (uint32_t(d.background)) {
    Call(c, b, sub_824918B8, 0x824A9750, {sp + 0x70, 0xFFFFFFFF, uint32_t(d.alpha)});
    Call(c, b, sub_8249A0C0, 0x824A975C, {graphics, c.r3.u32});
    Call(c, b, sub_8249E3C0, 0x824A976C, {graphics, sp + 0x50, uint32_t(d.background)});
  } else {
    const uint32_t alpha = d.alpha;
    Call(c, b, sub_824919C8, 0x824A9790, {sp + 0x70, 0x827D2E90});
    V(c, b, self, 0x68, 0x824A97AC, {sp + 0x80, self, 6, c.r3.u32});
    Call(c, b, sub_82491A90, 0x824A97B0, {});
    Call(c, b, sub_824918B8, 0x824A97C0, {sp + 0x90, c.r3.u32, alpha});
    Call(c, b, sub_8249A0C0, 0x824A97CC, {graphics, c.r3.u32});
    Call(c, b, sub_8249B730, 0x824A97EC,
         {graphics, 12 + uint32_t(inject), 12, uint32_t(W(b, self).width) - 25,
          uint32_t(W(b, self).height) - 25});
    const uint32_t alpha2 = d.alpha;
    Call(c, b, sub_824919C8, 0x824A9800, {sp + 0x90, 0x827D2E84});
    V(c, b, self, 0x68, 0x824A981C, {sp + 0x80, self, 5, c.r3.u32});
    Call(c, b, sub_82491A90, 0x824A9820, {});
    Call(c, b, sub_824918B8, 0x824A9830, {sp + 0x70, c.r3.u32, alpha2});
    Call(c, b, sub_8249A0C0, 0x824A983C, {graphics, c.r3.u32});
    Call(c, b, sub_8249B650, 0x824A985C,
         {graphics, 13, 13, uint32_t(W(b, self).width) - 26, uint32_t(W(b, self).height) - 26});
    Call(c, b, sub_82491928, 0x824A9880,
         {sp + 0x90, 0, 0, 0, uint32_t(int32_t(uint32_t(d.alpha) << 7) / 255)});
    Call(c, b, sub_8249A0C0, 0x824A988C, {graphics, c.r3.u32});
    Call(c, b, sub_8249B650, 0x824A98AC,
         {graphics, uint32_t(W(b, self).width) - 12, 24, 12, uint32_t(W(b, self).height) - 36});
    Call(c, b, sub_8249B650, 0x824A98CC,
         {graphics, 24, uint32_t(W(b, self).height) - 12, uint32_t(W(b, self).width) - 24, 12});
  }
  uint32_t y = uint32_t(d.content_insets[1]) + uint32_t(d.background_insets[1]);
  if (uint32_t(d.header.length)) {
    V(c, b, uint32_t(d.header_font), 8, 0x824A98F8, {uint32_t(d.header_font)});
    y -= c.r3.u32;
    V(c, b, uint32_t(d.header_font), 4, 0x824A9914, {uint32_t(d.header_font)});
    y += c.r3.u32;
    Call(c, b, sub_8249A0B8, 0x824A9924, {graphics, uint32_t(d.header_font)});
    const uint32_t colors = W(b, self).colors_begin;
    if ((int32_t(uint32_t(W(b, self).colors_end) - colors) >> 4) == 0)
      ppc_trap(c, b, 22);
    const uint32_t alpha = d.alpha;
    Call(c, b, sub_82491A90, 0x824A9944, {colors});
    Call(c, b, sub_824918B8, 0x824A9954, {sp + 0x90, c.r3.u32, alpha});
    Call(c, b, sub_8249A0C0, 0x824A9960, {graphics, c.r3.u32});
    V(c, b, self, 0xFC, 0x824A9984, {sp + 0x90, self, graphics, y, self + 0xF0});
    V(c, b, uint32_t(d.header_font), 4, 0x824A9998, {uint32_t(d.header_font)});
    y -= c.r3.u32;
    V(c, b, uint32_t(d.header_font), 0x10, 0x824A99B4, {uint32_t(d.header_font)});
    y += c.r3.u32 + uint32_t(d.header_spacing);
  }
  Call(c, b, sub_8249A0B8, 0x824A99CC, {graphics, uint32_t(d.body_font)});
  uint32_t colors = W(b, self).colors_begin;
  if (uint32_t(int32_t(uint32_t(W(b, self).colors_end) - colors) >> 4) <= 1)
    ppc_trap(c, b, 22);
  const uint32_t alpha = d.alpha;
  Call(c, b, sub_82491A90, 0x824A99F8, {colors + 16});
  Call(c, b, sub_824918B8, 0x824A9A08, {sp + 0x90, c.r3.u32, alpha});
  Call(c, b, sub_8249A0C0, 0x824A9A14, {graphics, c.r3.u32});
  pvz::guest::Write(b, sp + 0x60, uint32_t(d.background_insets[0]) + uint32_t(d.content_insets[0]) + 2);
  pvz::guest::Write(b, sp + 0x64, y);
  pvz::guest::Write(b, sp + 0x68,
                    uint32_t(W(b, self).width) - uint32_t(d.content_insets[2]) -
                        uint32_t(d.background_insets[0]) - uint32_t(d.content_insets[0]) -
                        uint32_t(d.background_insets[2]) - 4);
  pvz::guest::Write(b, sp + 0x6C, 0);
  const uint32_t alignment = d.field_150;
  V(c, b, uint32_t(d.body_font), 0x18, 0x824A9A74, {uint32_t(d.body_font)});
  V(c, b, self, 0x104, 0x824A9A9C,
    {self, graphics, sp + 0x60, self + 0x128, c.r3.u32 + uint32_t(d.field_154), alignment});
  y += c.r3.u32;
  if (uint32_t(d.footer.length) && int32_t(d.button_mode) != 3) {
    V(c, b, uint32_t(d.header_font), 0x18, 0x824A9ACC, {uint32_t(d.header_font)});
    y += c.r3.u32 + 8;
    Call(c, b, sub_8249A0B8, 0x824A9AE0, {graphics, uint32_t(d.header_font)});
    colors = W(b, self).colors_begin;
    if (uint32_t(int32_t(uint32_t(W(b, self).colors_end) - colors) >> 4) <= 2)
      ppc_trap(c, b, 22);
    const uint32_t alpha3 = d.alpha;
    Call(c, b, sub_82491A90, 0x824A9B08, {colors + 32});
    Call(c, b, sub_824918B8, 0x824A9B18, {sp + 0x90, c.r3.u32, alpha3});
    Call(c, b, sub_8249A0C0, 0x824A9B24, {graphics, c.r3.u32});
    V(c, b, self, 0xFC, 0x824A9B48, {sp + 0x90, self, graphics, y, self + 0x10C});
  }
}
#endif
} // namespace

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#define LEAF(A, P, N, O, R, I)                                                                               \
  PVZ_NATIVE_BATCH_REPLACEMENT(b008, P, dialog_##A, sub_##A, N, pvz::native::Safety::DeclaredOutputsOnly, O, \
                               R)
#define SNAP(A, P, N)                                                                                        \
  PVZ_NATIVE_BATCH_REPLACEMENT(b008, P, dialog_##A, sub_##A, N, pvz::native::Safety::SnapshotsOnly,          \
                               noOutputs, NoRanges)
#endif
LEAF(824A87A0, true, Modal, r3out, NoRanges, Modal)
LEAF(824A88C0, true, Identity, r3out, NoRanges, Identity)
LEAF(824A88C8, true, Identity, r3out, NoRanges, Identity)
LEAF(824A8900, true, Identity, r3out, NoRanges, Identity)
SNAP(824A7F28, true, Native_824A7F28)
SNAP(824A7FE8, false, Native_824A7FE8)
SNAP(824A8060, true, SetFont<true>)
SNAP(824A80D0, true, SetFont<false>)
SNAP(824A8140, true, ManagerChanged<false>)
SNAP(824A81A0, true, ManagerChanged<true>)
SNAP(824A8200, false, Native_824A8200)
SNAP(824A8260, true, Native_824A8260)
SNAP(824A8588, false, MouseButton<true>)
// 824A85F0 is the existing mouse drag hook; never registered here.
SNAP(824A8730, false, MouseButton<false>)
SNAP(824A87A8, true, Native_824A87A8)
SNAP(824A8850, false, Listener<false>)
SNAP(824A8888, true, Listener<true>)
SNAP(824A88D0, true, Native_824A88D0)
SNAP(824A8908, true, Native_824A8908)
SNAP(824A8960, true, Native_824A8960)
SNAP(824A8B28, true, Native_824A8B28)
SNAP(824A92D8, true, Native_824A92D8)
SNAP(824A94E0, true, Native_824A94E0)
SNAP(824A96D8, false, Native_824A96D8)
SNAP(824A9B50, false, Native_824A9B50)
#undef LEAF
#undef SNAP
