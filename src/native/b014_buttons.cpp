// B014. Complete Xbox 584109FF / 0.0.1.4 translations; T17 evidence manifest.
#include "native/replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/Buttons.h"
#include <array>
#include <bit>
#include <cmath>
#ifndef PVZ_BATCH_SYNTHETIC_TEST
#include "native/guest_call.h"
PVZ_NATIVE_BATCH(b014)
REX_EXTERN(sub_82125658);
REX_EXTERN(sub_82125760);
REX_EXTERN(sub_821257D8);
REX_EXTERN(sub_821264E0);
REX_EXTERN(sub_821269C0);
REX_EXTERN(sub_82187970);
REX_EXTERN(sub_82187D90);
REX_EXTERN(sub_82187F70);
REX_EXTERN(sub_82189250);
REX_EXTERN(sub_8219F4D0);
REX_EXTERN(sub_82284D98);
REX_EXTERN(sub_824729D8);
REX_EXTERN(sub_82472AD8);
REX_EXTERN(sub_82472BF8);
REX_EXTERN(sub_82472DE0);
REX_EXTERN(sub_82472F90);
REX_EXTERN(sub_82472FF0);
REX_EXTERN(sub_82472FF8);
REX_EXTERN(sub_82473040);
REX_EXTERN(sub_824730D0);
REX_EXTERN(sub_82473588);
REX_EXTERN(sub_82473C60);
REX_EXTERN(sub_82473D68);
REX_EXTERN(sub_82473E88);
REX_EXTERN(sub_82475EB8);
REX_EXTERN(sub_824918E0);
REX_EXTERN(sub_82491928);
REX_EXTERN(sub_824919C8);
REX_EXTERN(sub_82491B38);
REX_EXTERN(sub_8249A0B8);
REX_EXTERN(sub_8249A0C0);
REX_EXTERN(sub_8249A0F0);
REX_EXTERN(sub_8249A100);
REX_EXTERN(sub_8249B1A0);
REX_EXTERN(sub_8249B4A0);
REX_EXTERN(sub_8249B530);
REX_EXTERN(sub_8249B650);
REX_EXTERN(sub_8249B730);
REX_EXTERN(sub_8249BA90);
REX_EXTERN(sub_8249BC50);
REX_EXTERN(sub_8249C058);
REX_EXTERN(sub_8249C230);
REX_EXTERN(sub_8249C6A8);
REX_EXTERN(sub_8249D0E8);
REX_EXTERN(sub_8249D388);
REX_EXTERN(sub_8249DB80);
REX_EXTERN(sub_8249DD50);
REX_EXTERN(sub_8249DE38);
REX_EXTERN(sub_8249E3C0);
REX_EXTERN(sub_824AC938);
REX_EXTERN(sub_824AC940);
REX_EXTERN(sub_824AE260);
REX_EXTERN(sub_824AE730);
REX_EXTERN(sub_824AE8B0);
REX_EXTERN(sub_824AE998);
REX_EXTERN(sub_824B9A50);
REX_EXTERN(sub_824DCF20);
REX_EXTERN(sub_824DD5E0);
REX_EXTERN(sub_824E2DE8);
REX_EXTERN(sub_825DDB78);
REX_EXTERN(sub_825F8F50);
REX_EXTERN(sub_825F9350);
REXCVAR_DECLARE(bool, pvz_native_button_824AE098_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_824AE998_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_8254C100_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_824DCFD0_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_82582818_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_82593CE8_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_821880C8_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_822180A8_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_82189220_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_824E3610_inject_error);
REXCVAR_DECLARE(bool, pvz_native_button_824E3AD0_inject_error);
#endif
namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 0> noOutputs{};
const std::array r3out{RegisterOutput{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}};
const auto NoRanges = [](const PPCContext&) { return std::array<Range, 0>{}; };
auto& W(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::Widget*>(b + p);
}
void Identity(PPCContext&, uint8_t*) {}
void True(PPCContext& c, uint8_t*) {
  c.r3.u64 = 1;
}
void Disabled(PPCContext& c, uint8_t* b) {
  W(b, c.r3.u32).disabled = c.r4.u8;
}
const auto DisabledRange = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32 + uint32_t(offsetof(pvz::xbox::Widget, disabled)), 1}};
};
void Checked(PPCContext& c, uint8_t* b) {
  c.r3.u64 = reinterpret_cast<pvz::xbox::Checkbox*>(b + c.r3.u32)->checked;
}
void Pressed(PPCContext& c, uint8_t* b) {
  const auto& w = W(b, c.r3.u32);
  c.r3.u64 = w.is_down && (w.is_over || w.gamepad_focus) && !w.disabled;
}
auto& G(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::GameButton*>(b + p);
}
void Store64(uint8_t* b, uint32_t p, uint64_t v) {
  pvz::guest::Write(b, p, uint32_t(v >> 32));
  pvz::guest::Write(b, p + 4, uint32_t(v));
}
void GamePressed(PPCContext& c, uint8_t* b) {
  const auto& w = W(b, c.r3.u32);
  const auto& button = reinterpret_cast<pvz::xbox::ButtonWidget*>(b + c.r3.u32)[0];
  c.r3.u64 = w.is_down && w.is_over && !w.disabled && !button.field_149;
}
void GameCoordinates(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  G(b, self).field_190 = c.r4.u32;
  G(b, self).field_194 = c.r5.u32;
  Store64(b, c.r1.u32 - 16, uint64_t(int64_t(c.r5.s32)));
  Store64(b, c.r1.u32 - 16, uint64_t(int64_t(c.r4.s32)));
  c.fpscr.disableFlushMode();
  G(b, self).field_1a4 = c.r6.u32;
  G(b, self).field_1a8 = c.r7.u32;
  G(b, self).field_1a0 = float(c.r5.s32);
  G(b, self).field_19c = float(c.r4.s32);
}
const auto GameCoordinateRanges = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32 + 0x190, 0x1C}, Range{c.r1.u32 - 16, 8}};
};
void ZeroPair(PPCContext& c, uint8_t* b) {
  auto& p = *reinterpret_cast<pvz::xbox::ButtonPair*>(b + c.r3.u32);
  p.first = 0;
  p.second = 0;
}
const auto PairRange = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32, sizeof(pvz::xbox::ButtonPair)}};
};
auto& S(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::ScrollbarWidget*>(b + p);
}
void CustomTrack(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  const auto& scrollbar = S(b, self);
  const uint32_t dimension = scrollbar.horizontal ? uint32_t(W(b, self).width) : uint32_t(W(b, self).height);
  uint32_t amount = 0;
  if (int32_t(scrollbar.field_104) > 0)
    amount = uint32_t(scrollbar.field_104) << 1;
  else if (const uint32_t up = scrollbar.up_button)
    amount = uint32_t(W(b, up).width) << 1;
  c.r3.u64 = uint64_t(dimension) - uint64_t(amount);
}
#ifndef PVZ_BATCH_SYNTHETIC_TEST
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
using pvz::native::Virtual;
auto& B(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::ButtonWidget*>(b + p);
}
void V(PPCContext& c, uint8_t* b, uint32_t object, uint32_t slot, uint32_t lr,
       std::initializer_list<uint32_t> args) {
  Args(c, args);
  Virtual(c, b, object, slot, lr);
}
void Native_82120008(PPCContext& c, uint8_t* b) {
  Virtual(c, b, c.r3.u32, 4, uint32_t(c.lr));
}
void Native_821DDC80(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, text = c.r4.u32;
  Call(c, b, sub_821257D8, uint32_t(c.lr), {self + 0xD4, text, 0, 0xFFFFFFFF});
}
void Native_824ADFC0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, font = c.r4.u32;
  Frame f(c, b, 0x70);
  if (const uint32_t old = B(b, self).font)
    V(c, b, old, 0, 0x824ADFFC, {old, 1});
  V(c, b, font, 0x2C, 0x824AE010, {font});
  B(b, self).font = c.r3.u32;
}
void Native_824AE098(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32, image = c.r5.u32, rect = c.r6.u32, x = c.r7.u32,
                 y = c.r8.u32;
  const uint32_t drawX = x + uint32_t(REXCVAR_GET(pvz_native_button_824AE098_inject_error));
  if (reinterpret_cast<pvz::xbox::ButtonRect*>(b + rect)->width)
    Call(c, b, sub_8249BC50, uint32_t(c.lr), {graphics, uint32_t(B(b, self).image), drawX, y, rect});
  else
    Call(c, b, sub_8249D0E8, uint32_t(c.lr), {graphics, image, drawX, y});
}
void Native_824AE0D0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82472AD8, 0x824AE0E8, {self});
  if (uint32_t(B(b, self).disabled_image) || uint32_t(B(b, self).rects[3][2]))
    V(c, b, self, 0x10, 0x824AE124, {self});
}
void Native_824AE138(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82472FF8, 0x824AE154, {self, x, y});
  const uint32_t listener = B(b, self).listener;
  V(c, b, listener, 0x18, 0x824AE174, {listener, uint32_t(W(b, self).id), x, y});
}
void Native_824AE180(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, clicks = c.r6.u32, button = c.r7.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82473040, 0x824AE1A0, {self, x, y, clicks, button});
  const uint32_t listener = B(b, self).listener;
  V(c, b, listener, 0, 0x824AE1BC, {listener, uint32_t(W(b, self).id), button});
  V(c, b, self, 0x10, 0x824AE1D0, {self});
}
void Native_824AE1E8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, clicks = c.r6.u32, button = c.r7.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_824730D0, 0x824AE200, {self, x, y, clicks, button});
  if (W(b, self).is_over &&
      reinterpret_cast<pvz::xbox::ButtonManagerState*>(b + uint32_t(W(b, self).manager))->field_a0) {
    const uint32_t listener = B(b, self).listener;
    V(c, b, listener, 8, 0x824AE234, {listener, uint32_t(W(b, self).id)});
  }
  V(c, b, self, 0x10, 0x824AE248, {self});
}
void Native_824AE260(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82472BF8, 0x824AE278, {self});
  auto& w = W(b, self);
  auto& button = B(b, self);
  if (w.is_down && (w.is_over || w.gamepad_focus))
    if (const uint32_t listener = button.listener)
      V(c, b, listener, 0xC, 0x824AE2BC, {listener, uint32_t(w.id)});
  c.fpscr.disableFlushMode();
  const double zero = std::bit_cast<double>(uint64_t(0));
  bool dirty = false;
  if (!w.is_down && !w.is_over && !w.gamepad_focus && double(button.over_alpha) > zero) {
    if (double(button.fade_out) > zero) {
      double next = double(button.over_alpha) - double(button.fade_out);
      if (next < zero)
        next = zero;
      button.over_alpha = next;
    } else
      button.over_alpha = zero;
    dirty = true;
  } else if ((w.is_over || w.gamepad_focus) && double(button.fade_in) > zero &&
             double(button.over_alpha) < 1.0) {
    double next = double(button.fade_in) + double(button.over_alpha);
    if (next > 1.0)
      next = 1.0;
    button.over_alpha = next;
    dirty = true;
  }
  if (dirty)
    V(c, b, self, 0x10, 0x824AE374, {self});
}
void Native_824AE388(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, button = c.r4.u32, user = c.r5.u32, flags = c.r6.u32;
  Frame f(c, b, 0x60);
  const uint32_t selectedUser = W(b, self).user_index;
  if (selectedUser != 0xFFFFFFFF && selectedUser != user)
    return;
  const uint32_t linked = W(b, self).field_ac;
  if (linked && uint32_t(W(b, linked).user_index) != 0xFFFFFFFF && uint32_t(W(b, linked).user_index) != user)
    return;
  if (button == 6) {
    if (flags & 1)
      return;
    B(b, self).button_user = user;
    V(c, b, self, 0x12C, 0x824AE3F8, {self, button, user, flags});
    W(b, self).is_down = 1;
    if (const uint32_t listener = B(b, self).listener)
      V(c, b, listener, 0, 0x824AE424, {listener, uint32_t(W(b, self).id), 1});
    V(c, b, self, 0x10, 0x824AE438, {self});
  } else if (W(b, self).is_down) {
    if (linked)
      V(c, b, linked, 0xC0, 0x824AE464, {linked, button, user, flags | 8});
  } else
    Call(c, b, sub_82472DE0, 0x824AE470, {self, button, user, flags});
}
void Native_824AE610(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82472FF0, 0x824AE628, {self});
  auto& button = B(b, self);
  if (double(button.fade_out) == 0.0 && double(button.over_alpha) > 0.0)
    button.over_alpha = 0.0;
  else if (double(button.fade_out) > 0.0 && double(button.over_alpha) == 0.0)
    button.over_alpha = 1.0;
  bool dirty = W(b, self).is_down || uint32_t(button.over_image) || uint32_t(button.rects[1][2]);
  if (!dirty) {
    const uint32_t first = W(b, self).colors_begin;
    if ((int32_t(uint32_t(W(b, self).colors_end) - first) >> 4) == 0)
      ppc_trap(c, b, 22);
    const uint32_t count = uint32_t(int32_t(uint32_t(W(b, self).colors_end) - first) >> 4);
    if (count <= 1)
      ppc_trap(c, b, 22);
    Call(c, b, sub_82491B38, 0x824AE6DC, {first + 16, first});
    dirty = c.r3.u8 != 0;
  }
  if (dirty)
    V(c, b, self, 0x10, 0x824AE6F8, {self});
  const uint32_t listener = button.listener;
  V(c, b, listener, 0x14, 0x824AE710, {listener, uint32_t(W(b, self).id)});
}
void Native_824AE730(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, id = c.r4.u32, listener = c.r5.u32;
  Frame f(c, b, 0x90);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xA4, self);
  Call(c, b, sub_82473E88, 0x824AE754, {self});
  W(b, self).vtable = 0x82064CB4;
  W(b, self).listener_vtable = 0x82064C7C;
  Call(c, b, sub_82125760, 0x824AE77C, {self + 0xD4, 0, 0});
  auto& button = B(b, self);
  for (auto& rect : button.rects)
    for (auto& component : rect)
      component = 0;
  button.over_alpha = 0.0;
  button.fade_out = 0.0;
  button.fade_in = 0.0;
  W(b, self).id = id;
  button.font = 0;
  button.field_f0 = 0;
  button.image = 0;
  button.over_image = 0;
  button.down_image = 0;
  button.disabled_image = 0;
  button.field_148 = 0;
  button.field_149 = 0;
  button.field_14a = 0;
  button.listener = listener;
  W(b, self).dirty = 1;
  for (uint32_t offset : {0x108u, 0x118u, 0x128u, 0x138u})
    pvz::guest::Write(b, sp + 0x50, self + offset);
  Call(c, b, sub_82473D68, 0x824AE83C, {self, 0x827D2EC8, 6});
  button.button_user = 0xFFFFFFFF;
  c.r3.u64 = self;
}
void Native_824AE8B0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  pvz::guest::Write(b, c.r1.u32 + 0x84, self);
  W(b, self).vtable = 0x82064CB4;
  W(b, self).listener_vtable = 0x82064C7C;
  if (const uint32_t font = B(b, self).font)
    V(c, b, font, 0, 0x824AE908, {font, 1});
  Call(c, b, sub_82125760, 0x824AE918, {self + 0xD4, 1, 0});
  Call(c, b, sub_82473C60, 0x824AE920, {self});
}
void Native_824AF248(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824AE8B0, 0x824AF268, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x824AF278, {self});
  c.r3.u64 = self;
}
uint32_t ButtonColor(PPCContext& c, uint8_t* b, uint32_t self, uint32_t index) {
  const uint32_t begin = W(b, self).colors_begin;
  const int32_t count = int32_t(uint32_t(W(b, self).colors_end) - begin) >> 4;
  if (index == 0 ? count == 0 : uint32_t(count) <= index)
    ppc_trap(c, b, 22);
  return begin + index * 16;
}
void Native_824AE998(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0xD0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0x50, 0);
  auto& button = B(b, self);
  if (button.field_149)
    return;
  if (!uint32_t(button.font) && uint32_t(button.label.length)) {
    Call(c, b, sub_825DDB78, 0x824AE9E4, {0x20});
    const uint32_t font = c.r3.u32;
    pvz::guest::Write(b, sp + 0x60, font);
    if (font) {
      Call(c, b, sub_821264E0, 0x824AEA00, {sp + 0x70, 0x82064B64});
      pvz::guest::Write(b, sp + 0x50, 1);
      const uint32_t app = pvz::guest::Read(b, uint32_t(W(b, self).manager) + 0x70);
      Call(c, b, sub_824DD5E0, 0x824AEA30, {font, app, sp + 0x70, 10, 0, 0, 0, 0});
    }
    button.font = c.r3.u32;
    if (font)
      Call(c, b, sub_821269C0, 0x824AEA54, {sp + 0x70, 1, 0});
  }
  const auto& w = W(b, self);
  const uint8_t pressed =
      button.field_148 ^ uint8_t(w.is_down && (w.is_over || w.gamepad_focus) && !w.disabled);
  uint32_t x = 0, y = 0;
  if (const uint32_t font = button.font) {
    if (int32_t(button.field_f0) == 0 || int32_t(button.field_f0) == 1) {
      const bool center = int32_t(button.field_f0) == 0;
      V(c, b, font, 0x1C, center ? 0x824AEACC : 0x824AEAFC, {font, self + 0xD4});
      x = uint32_t(W(b, self).width) - c.r3.u32;
      if (center)
        x = uint32_t(int32_t(x) / 2);
    }
    V(c, b, uint32_t(button.font), 4, 0x824AEB18, {uint32_t(button.font)});
    const uint32_t height = uint32_t(W(b, self).height) - uint32_t(c.r3.s32 / 6) - 1;
    V(c, b, uint32_t(button.font), 4, 0x824AEB44, {uint32_t(button.font)});
    y = uint32_t(int32_t(c.r3.u32 + height) / 2);
  }
  Call(c, b, sub_8249A0B8, 0x824AEB5C, {graphics, uint32_t(button.font)});
  auto color = [&](uint32_t index, uint32_t lr) {
    pvz::guest::Write(b, sp + 0x60, self + 0x78);
    Call(c, b, sub_8249A0C0, lr, {graphics, ButtonColor(c, b, self, index)});
  };
  auto fill = [&](uint32_t lr, uint32_t dx, uint32_t dy, uint32_t dw, uint32_t dh) {
    Call(c, b, sub_8249B650, lr, {graphics, dx, dy, dw, dh});
  };
  const bool noImages = !uint32_t(button.image) && !uint32_t(button.down_image);
  if (noImages) {
    if (!button.field_14a) {
      color(5, 0x824AEBB0);
      fill(0x824AEBC8, 0, 0, uint32_t(W(b, self).width), uint32_t(W(b, self).height));
    }
    if (!button.field_14a) {
      color(pressed ? 2 : 3, pressed ? 0x824AEC0C : 0x824AED80);
      fill(pressed ? 0x824AEC28 : 0x824AED9C, 0, 0, uint32_t(W(b, self).width) - 1, 1);
      fill(pressed ? 0x824AEC44 : 0x824AEDB8, 0, 0, 1, uint32_t(W(b, self).height) - 1);
      color(pressed ? 3 : 2, pressed ? 0x824AEC70 : 0x824AEDE4);
      fill(pressed ? 0x824AEC8C : 0x824AEE00, 0, uint32_t(W(b, self).height) - 1, uint32_t(W(b, self).width),
           1);
      fill(pressed ? 0x824AECA8 : 0x824AEE1C, uint32_t(W(b, self).width) - 1, 0, 1,
           uint32_t(W(b, self).height));
      color(4, pressed ? 0x824AECD4 : 0x824AEE48);
      if (pressed) {
        fill(0x824AECF0, 1, 1, uint32_t(W(b, self).width) - 3, 1);
        fill(0x824AED0C, 1, 1, 1, uint32_t(W(b, self).height) - 3);
      } else {
        fill(0x824AEE68, 1, uint32_t(W(b, self).height) - 2, uint32_t(W(b, self).width) - 2, 1);
        fill(0x824AEE88, uint32_t(W(b, self).width) - 2, 1, 1, uint32_t(W(b, self).height) - 2);
      }
    }
  } else {
    auto available = [&](uint32_t index) {
      const uint32_t image = index == 0   ? uint32_t(button.image)
                             : index == 1 ? uint32_t(button.over_image)
                             : index == 2 ? uint32_t(button.down_image)
                                          : uint32_t(button.disabled_image);
      return image || uint32_t(button.rects[index][2]);
    };
    auto draw = [&](uint32_t index, uint32_t shift, uint32_t lr, bool useNormalImage = false) {
      const uint32_t image = useNormalImage || index == 0 ? uint32_t(button.image)
                             : index == 1                 ? uint32_t(button.over_image)
                             : index == 2                 ? uint32_t(button.down_image)
                                                          : uint32_t(button.disabled_image);
      V(c, b, self, 0x11C, lr, {self, graphics, image, self + 0x108 + index * 16, shift, shift});
    };
    if (pressed) {
      if (available(2))
        draw(2, 0, 0x824AF190);
      else if (available(1))
        draw(1, 1, 0x824AF190);
      else
        draw(0, 1, 0x824AF190);
    } else if (W(b, self).disabled && available(3))
      draw(3, 0, 0x824AF0E4);
    else if (double(button.over_alpha) > 0.0 && available(1)) {
      if (available(0) && double(button.over_alpha) < 1.0)
        draw(0, 0, 0x824AEFC8);
      Call(c, b, sub_8249A100, 0x824AEFD4, {graphics, 1});
      c.fpscr.disableFlushMode();
      const double scaled = double(button.over_alpha) * 255.0;
      const int64_t alpha = std::isnan(scaled)          ? int64_t(0x80000000U)
                            : scaled >= double(INT_MAX) ? INT_MAX
                                                        : simde_mm_cvttsd_si32(simde_mm_load_sd(&scaled));
      pvz::guest::Write(b, sp + 0x60, uint32_t(uint64_t(alpha) >> 32));
      pvz::guest::Write(b, sp + 0x64, uint32_t(alpha));
      Call(c, b, sub_82491928, 0x824AF004, {sp + 0x60, 255, 255, 255, uint32_t(alpha)});
      Call(c, b, sub_8249A0C0, 0x824AF010, {graphics, c.r3.u32});
      draw(1, 0, 0x824AF038);
      Call(c, b, sub_8249A100, 0x824AF044, {graphics, 0});
    } else if ((W(b, self).is_over || W(b, self).gamepad_focus || W(b, self).is_down) && available(1))
      draw(1, 0, 0x824AF0E4);
    else if (available(0))
      draw(0, 0, 0x824AF0E4);
  }
  const bool highlighted = W(b, self).is_over || W(b, self).gamepad_focus;
  if (noImages && pressed) {
    color(highlighted ? 1 : 0, 0x824AF1C0);
    ++x;
    ++y;
  } else if (!noImages && pressed) {
    color(1, 0x824AF1C0);
    ++x;
    ++y;
  } else
    color(highlighted ? 1 : 0, 0x824AEEE0);
  x += uint32_t(REXCVAR_GET(pvz_native_button_824AE998_inject_error));
  Call(c, b, sub_8249B1A0, 0x824AF1D4, {graphics, self + 0xD4, x, y});
}
auto& C(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::Checkbox*>(b + p);
}
void Native_8254C088(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, checked = c.r4.u32;
  const bool notify = c.r5.u8 != 0;
  Frame f(c, b, 0x60);
  C(b, self).checked = uint8_t(checked);
  if (notify)
    if (const uint32_t listener = C(b, self).listener)
      V(c, b, listener, 0, 0x8254C0CC, {listener, uint32_t(W(b, self).id), checked});
  V(c, b, self, 0x10, 0x8254C0E0, {self});
}
void Native_8254C100(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824729D8, 0x8254C120, {self, graphics});
  const auto& box = C(b, self);
  const uint32_t x = uint32_t(REXCVAR_GET(pvz_native_button_8254C100_inject_error));
  if (!uint32_t(box.checked_rect[2]) && uint32_t(box.checked_image) && uint32_t(box.unchecked_image)) {
    Call(c, b, sub_8249D0E8, 0x8254C164,
         {graphics, box.checked ? uint32_t(box.checked_image) : uint32_t(box.unchecked_image), x, 0});
    return;
  }
  if (uint32_t(box.checked_rect[2]) && uint32_t(box.unchecked_image)) {
    Call(c, b, sub_8249BC50, 0x8254C1A0,
         {graphics, uint32_t(box.unchecked_image), x, 0, self + (box.checked ? 0xE4u : 0xF4u)});
    return;
  }
  if (uint32_t(box.unchecked_image) || uint32_t(box.checked_image))
    return;
  Call(c, b, sub_8249A0C0, 0x8254C1C8, {graphics, self + 0x104});
  Call(c, b, sub_8249B650, 0x8254C1E0,
       {graphics, x, 0, uint32_t(W(b, self).width), uint32_t(W(b, self).height)});
  Call(c, b, sub_8249A0C0, 0x8254C1EC, {graphics, self + 0x114});
  Call(c, b, sub_8249B650, 0x8254C20C,
       {graphics, 1, 1, uint32_t(W(b, self).width) - 2, uint32_t(W(b, self).height) - 2});
  if (C(b, self).checked) {
    Call(c, b, sub_8249A0C0, 0x8254C224, {graphics, self + 0x124});
    Call(c, b, sub_8249BA90, 0x8254C244,
         {graphics, 1, 1, uint32_t(W(b, self).width) - 2, uint32_t(W(b, self).height) - 2});
    Call(c, b, sub_8249BA90, 0x8254C264,
         {graphics, uint32_t(W(b, self).width) - 1, 1, 1, uint32_t(W(b, self).height) - 2});
  }
}
void Native_8254C280(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, clicks = c.r6.u32, button = c.r7.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82473040, 0x8254C298, {self, x, y, clicks, button});
  const uint32_t checked = C(b, self).checked == 0;
  C(b, self).checked = uint8_t(checked);
  if (const uint32_t listener = C(b, self).listener)
    V(c, b, listener, 0, 0x8254C2C8, {listener, uint32_t(W(b, self).id), checked});
  V(c, b, self, 0x10, 0x8254C2DC, {self});
}
void Native_8254C2F0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, button = c.r4.u32, user = c.r5.u32, flags = c.r6.u32;
  Frame f(c, b, 0x60);
  if (button == 6) {
    if (!(flags & 1)) {
      V(c, b, self, 0x124, 0x8254C324, {self, button, user, flags});
      W(b, self).is_down = 1;
      V(c, b, self, 0x10, 0x8254C340, {self});
    }
  } else
    Call(c, b, sub_82472DE0, 0x8254C34C, {self, button, user, flags});
}
void Native_8254C360(PPCContext& c, uint8_t* b) {
  if (c.r4.s32 != 6)
    Call(c, b, sub_82472F90, uint32_t(c.lr), {c.r3.u32, c.r4.u32, c.r5.u32, c.r6.u32});
}
void Native_8254C380(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, uncheckedImage = c.r4.u32, checkedImage = c.r5.u32, id = c.r6.u32,
                 listener = c.r7.u32;
  Frame f(c, b, 0x90);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xA4, self);
  Call(c, b, sub_82473E88, 0x8254C3AC, {self});
  auto& box = C(b, self);
  box.listener = listener;
  box.unchecked_image = uncheckedImage;
  box.checked_image = checkedImage;
  box.checked = 0;
  W(b, self).vtable = 0x82072094;
  W(b, self).listener_vtable = 0x8207205C;
  for (uint32_t i = 0; i < 4; ++i) {
    box.checked_rect[i] = 0;
    box.unchecked_rect[i] = 0;
  }
  for (uint32_t i = 0; i < 4; ++i)
    box.outline_color[i] = pvz::guest::Read(b, 0x827D2D34 + i * 4);
  pvz::guest::Write(b, sp + 0x50, self + 0xE4);
  pvz::guest::Write(b, sp + 0x50, self + 0xF4);
  Call(c, b, sub_824918E0, 0x8254C444, {self + 0x114, 80, 80, 80});
  Call(c, b, sub_824918E0, 0x8254C458, {self + 0x124, 255, 255, 0});
  W(b, self).id = id;
  W(b, self).flag_a0 = 1;
  c.r3.u64 = self;
}
auto& DB(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::DialogButton*>(b + p);
}
void Native_824DCF20(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, image = c.r4.u32, id = c.r5.u32, listener = c.r6.u32;
  Frame f(c, b, 0x70);
  pvz::guest::Write(b, c.r1.u32 + 0x84, self);
  Call(c, b, sub_824AE730, 0x824DCF48, {self, id, listener});
  auto& button = DB(b, self);
  button.dialog_image = image;
  button.down_y = 1;
  button.text_y = 0;
  W(b, self).vtable = 0x8206917C;
  W(b, self).listener_vtable = 0x82069144;
  button.text_x = 0;
  button.down_x = 1;
  W(b, self).flag_a0 = 1;
  Call(c, b, sub_82473D68, 0x824DCF94, {self, 0x827D3548, 6});
  c.r3.u64 = self;
}
void Native_824DCFD0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0x140);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0x50, 0);
  if (B(b, self).field_149)
    return;
  if (!uint32_t(DB(b, self).dialog_image)) {
    Call(c, b, sub_824AE998, 0x824DD010, {self, graphics});
    return;
  }
  if (!uint32_t(B(b, self).font) && uint32_t(B(b, self).label.length)) {
    Call(c, b, sub_825DDB78, 0x824DD034, {0x20});
    const uint32_t font = c.r3.u32;
    pvz::guest::Write(b, sp + 0x60, font);
    if (font) {
      Call(c, b, sub_821264E0, 0x824DD050, {sp + 0xE0, 0x82069094});
      pvz::guest::Write(b, sp + 0x50, 1);
      const uint32_t app = pvz::guest::Read(b, uint32_t(W(b, self).manager) + 0x70);
      Call(c, b, sub_824DD5E0, 0x824DD080, {font, app, sp + 0xE0, 12, 1, 0, 0, 0});
    }
    B(b, self).font = c.r3.u32;
    if (font)
      Call(c, b, sub_821269C0, 0x824DD0A4, {sp + 0xE0, 1, 0});
  }
  V(c, b, self, 0x128, 0x824DD0B8, {self});
  const bool pressed = c.r3.u8 != 0;
  const uint32_t injectedX = uint32_t(REXCVAR_GET(pvz_native_button_824DCFD0_inject_error));
  auto rectangle = [&](uint32_t p) {
    pvz::guest::Write(b, p, injectedX);
    pvz::guest::Write(b, p + 4, 0);
    pvz::guest::Write(b, p + 8, uint32_t(W(b, self).width));
    pvz::guest::Write(b, p + 12, uint32_t(W(b, self).height));
  };
  auto draw = [&](uint32_t source, uint32_t dest, uint32_t lr) {
    rectangle(dest);
    Call(c, b, sub_8249DE38, lr, {graphics, self + source, dest, uint32_t(DB(b, self).dialog_image)});
  };
  if (!uint32_t(B(b, self).rects[0][2])) {
    if (pressed)
      Call(c, b, sub_8249B4A0, 0x824DD0E0,
           {graphics, uint32_t(DB(b, self).down_x), uint32_t(DB(b, self).down_y)});
    rectangle(sp + 0xD0);
    Call(c, b, sub_8249E3C0, 0x824DD108, {graphics, sp + 0xD0, uint32_t(DB(b, self).dialog_image)});
  } else {
    if (W(b, self).disabled && int32_t(B(b, self).rects[3][2]) > 0 && int32_t(B(b, self).rects[3][3]) > 0)
      draw(0x138, sp + 0x90, 0x824DD2BC);
    else {
      V(c, b, self, 0x128, 0x824DD168, {self});
      if (c.r3.u8)
        draw(0x128, sp + 0x70, 0x824DD2BC);
      else if (double(B(b, self).over_alpha) > 0.0) {
        if (double(B(b, self).over_alpha) < 1.0)
          draw(0x108, sp + 0xB0, 0x824DD1E4);
        Call(c, b, sub_8249A100, 0x824DD1F0, {graphics, 1});
        c.fpscr.disableFlushMode();
        const double scaled = double(B(b, self).over_alpha) * 255.0;
        const int64_t alpha = std::isnan(scaled)          ? int64_t(0x80000000U)
                              : scaled >= double(INT_MAX) ? INT_MAX
                                                          : simde_mm_cvttsd_si32(simde_mm_load_sd(&scaled));
        pvz::guest::Write(b, sp + 0x60, uint32_t(uint64_t(alpha) >> 32));
        pvz::guest::Write(b, sp + 0x64, uint32_t(alpha));
        Call(c, b, sub_82491928, 0x824DD220, {sp + 0x60, 255, 255, 255, uint32_t(alpha)});
        Call(c, b, sub_8249A0C0, 0x824DD22C, {graphics, c.r3.u32});
        draw(0x118, sp + 0x80, 0x824DD258);
        Call(c, b, sub_8249A100, 0x824DD264, {graphics, 0});
      } else if (W(b, self).is_over)
        draw(0x118, sp + 0xA0, 0x824DD2BC);
      else
        draw(0x108, sp + 0xC0, 0x824DD2BC);
    }
    if (pressed)
      Call(c, b, sub_8249B4A0, 0x824DD2D4,
           {graphics, uint32_t(DB(b, self).down_x), uint32_t(DB(b, self).down_y)});
  }
  if (const uint32_t font = B(b, self).font) {
    Call(c, b, sub_8249A0B8, 0x824DD2E8, {graphics, font});
    pvz::guest::Write(b, sp + 0x60, self + 0x78);
    Call(c, b, sub_8249A0C0, 0x824DD348, {graphics, ButtonColor(c, b, self, W(b, self).is_over ? 1 : 0)});
    V(c, b, uint32_t(B(b, self).font), 0x1C, 0x824DD364, {uint32_t(B(b, self).font), self + 0xD4});
    const uint32_t x = uint32_t(int32_t(uint32_t(W(b, self).width) - c.r3.u32) / 2);
    V(c, b, uint32_t(B(b, self).font), 4, 0x824DD38C, {uint32_t(B(b, self).font)});
    uint32_t y = uint32_t(W(b, self).height) - uint32_t(c.r3.s32 / 6);
    V(c, b, uint32_t(B(b, self).font), 8, 0x824DD3B4, {uint32_t(B(b, self).font)});
    y -= c.r3.u32 + 1;
    V(c, b, uint32_t(B(b, self).font), 4, 0x824DD3D4, {uint32_t(B(b, self).font)});
    y = uint32_t(int32_t(y + c.r3.u32) / 2);
    Call(c, b, sub_8249B1A0, 0x824DD3FC,
         {graphics, self + 0xD4, x + uint32_t(DB(b, self).text_x), y + uint32_t(DB(b, self).text_y)});
  }
  if (pressed)
    Call(c, b, sub_8249B4A0, 0x824DD41C,
         {graphics, 0u - uint32_t(DB(b, self).down_x), 0u - uint32_t(DB(b, self).down_y)});
}
auto& Scroll(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::ScrollbuttonFull*>(b + p);
}
void Native_82582798(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, id = c.r4.u32, listener = c.r5.u32, direction = c.r6.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824AE730, 0x825827B8, {self, id, listener});
  Scroll(b, self).direction = direction;
  W(b, self).vtable = 0x8207DD84;
  W(b, self).listener_vtable = 0x8207DD50;
  Scroll(b, self).horizontal = 0;
  c.r3.u64 = self;
}
void Native_825827F8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  W(b, self).vtable = 0x8207DD84;
  W(b, self).listener_vtable = 0x8207DD50;
  Call(c, b, sub_824AE8B0, uint32_t(c.lr), {self});
}
void Native_82582818(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0x90);
  const uint32_t temp = c.r1.u32 + 0x50;
  Call(c, b, sub_824918E0, 0x82582844, {temp, 212, 212, 212});
  Call(c, b, sub_8249A0C0, 0x82582850, {graphics, c.r3.u32});
  Call(c, b, sub_8249B650, 0x82582868,
       {graphics, uint32_t(REXCVAR_GET(pvz_native_button_82582818_inject_error)), 0,
        uint32_t(W(b, self).width), uint32_t(W(b, self).height)});
  const uint32_t pressed = W(b, self).is_down && W(b, self).is_over && !W(b, self).disabled;
  if (pressed) {
    Call(c, b, sub_824918E0, 0x825828AC, {temp, 132, 132, 132});
    Call(c, b, sub_8249A0C0, 0x825828B8, {graphics, c.r3.u32});
    Call(c, b, sub_8249B730, 0x825828D8,
         {graphics, 0, 0, uint32_t(W(b, self).width) - 1, uint32_t(W(b, self).height) - 1});
  } else {
    Call(c, b, sub_824918E0, 0x825828F0, {temp, 255, 255, 255});
    Call(c, b, sub_8249A0C0, 0x825828FC, {graphics, c.r3.u32});
    Call(c, b, sub_8249B650, 0x82582918, {graphics, 1, 1, uint32_t(W(b, self).width) - 2, 1});
    Call(c, b, sub_8249B650, 0x82582934, {graphics, 1, 1, 1, uint32_t(W(b, self).height) - 2});
    Call(c, b, sub_8249A0C0, 0x82582940, {graphics, 0x827D2D24});
    Call(c, b, sub_8249B650, 0x8258295C,
         {graphics, 0, uint32_t(W(b, self).height) - 1, uint32_t(W(b, self).width), 1});
    Call(c, b, sub_8249B650, 0x82582978,
         {graphics, uint32_t(W(b, self).width) - 1, 0, 1, uint32_t(W(b, self).height)});
    Call(c, b, sub_824918E0, 0x8258298C, {temp, 132, 132, 132});
    Call(c, b, sub_8249A0C0, 0x82582998, {graphics, c.r3.u32});
    Call(c, b, sub_8249B650, 0x825829B8,
         {graphics, 1, uint32_t(W(b, self).height) - 2, uint32_t(W(b, self).width) - 2, 1});
    Call(c, b, sub_8249B650, 0x825829D8,
         {graphics, uint32_t(W(b, self).width) - 2, 1, 1, uint32_t(W(b, self).height) - 2});
  }
  uint32_t color = 0x827D2D24;
  if (W(b, self).disabled) {
    Call(c, b, sub_824918E0, 0x82582A00, {temp, 132, 132, 132});
    color = c.r3.u32;
  }
  Call(c, b, sub_8249A0C0, 0x82582A0C, {graphics, color});
  const bool horizontal = Scroll(b, self).horizontal || int32_t(Scroll(b, self).direction) == 3 ||
                          int32_t(Scroll(b, self).direction) == 4;
  for (uint32_t row = 0, size = 1; size < 9; ++row, size += 2) {
    uint32_t x, y;
    if (!horizontal) {
      const bool up = int32_t(W(b, self).id) == 0 || int32_t(Scroll(b, self).direction) == 1;
      x = uint32_t(int32_t(W(b, self).width) / 2) - row + pressed - 1;
      const uint32_t center = uint32_t(int32_t(uint32_t(W(b, self).height) - 4) / 2);
      y = up ? center + row + pressed : center - row + pressed + 3;
    } else {
      const bool left = int32_t(W(b, self).id) == 0 || int32_t(Scroll(b, self).direction) == 3;
      const uint32_t center = uint32_t(int32_t(uint32_t(W(b, self).width) - 4) / 2);
      x = left ? center + row + pressed : center - row + pressed + 3;
      y = uint32_t(int32_t(W(b, self).height) / 2) - row + pressed - 1;
    }
    Call(c, b, sub_8249B650, horizontal ? 0x82582B68 : 0x82582ABC,
         {graphics, x, y, horizontal ? 1u : size, horizontal ? size : 1u});
  }
}
void Native_82582B80(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  W(b, self).vtable = 0x8207DD84;
  W(b, self).listener_vtable = 0x8207DD50;
  Call(c, b, sub_824AE8B0, 0x82582BB8, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x82582BC8, {self});
  c.r3.u64 = self;
}
auto& Label(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::LabelWidget*>(b + p);
}
void Native_82593CE8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0x70);
  if (!uint32_t(Label(b, self).label.length))
    return;
  if (const uint32_t font = Label(b, self).font)
    Call(c, b, sub_8249A0B8, 0x82593D1C, {graphics, font});
  uint32_t x = 0;
  if (int32_t(Label(b, self).alignment) == 1 &&
      uint32_t(reinterpret_cast<pvz::xbox::LabelGraphics*>(b + graphics)->font)) {
    Call(c, b, sub_8249B530, 0x82593D44, {graphics, self + 0xD8});
    x = 0u - uint32_t(c.r3.s32 / 2);
  }
  V(c, b, self, 0x6C, 0x82593D68, {self, 0});
  Call(c, b, sub_8249A0C0, 0x82593D74, {graphics, c.r3.u32});
  x += uint32_t(REXCVAR_GET(pvz_native_button_82593CE8_inject_error));
  Call(c, b, sub_8249B1A0, 0x82593D88, {graphics, self + 0xD8, x, 0});
}
void Native_82593D98(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  pvz::guest::Write(b, c.r1.u32 + 0x84, self);
  Call(c, b, sub_82473E88, 0x82593DBC, {self});
  W(b, self).vtable = 0x82082174;
  W(b, self).listener_vtable = 0x8208213C;
  Label(b, self).font = 0;
  Call(c, b, sub_82125658, 0x82593DEC, {self + 0xD8, 0x820820FC});
  c.r3.u64 = self;
}
void Native_82593E30(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82125760, 0x82593E5C, {self + 0xD8, 1, 0});
  Call(c, b, sub_82473C60, 0x82593E64, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x82593E74, {self});
  c.r3.u64 = self;
}
double DoubleAt(uint8_t* b, uint32_t p) {
  return std::bit_cast<double>((uint64_t(pvz::guest::Read(b, p)) << 32) | pvz::guest::Read(b, p + 4));
}
int64_t Fctiwz(double value) {
  return std::isnan(value)          ? int64_t(0x80000000U)
         : value >= double(INT_MAX) ? INT_MAX
                                    : simde_mm_cvttsd_si32(simde_mm_load_sd(&value));
}
void Native_82187918(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  // The installed navigation cleanup hook must remain in this call chain.
  Call(c, b, sub_82187F70, 0x82187938, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x8218794C, {self});
  c.r3.u64 = self;
}
void Native_82187D90(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, id = c.r4.u32, listener = c.r5.u32;
  Frame f(c, b, 0xB0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xC4, self);
  Call(c, b, sub_824DCF20, 0x82187DC0, {self, 0, id, listener});
  auto& button = G(b, self);
  W(b, self).id = id;
  const float zero = pvz::guest::Float(b, 0x820B5244);
  const double fade = DoubleAt(b, 0x820B5358);
  button.field_19c = zero;
  button.field_1a0 = zero;
  B(b, self).over_alpha = fade;
  B(b, self).fade_out = fade;
  button.app = pvz::guest::Read(b, 0x828923E8);
  B(b, self).fade_in = fade;
  W(b, self).vtable = 0x820BA0AC;
  W(b, self).listener_vtable = 0x820BA1E4;
  B(b, self).font = 0;
  B(b, self).field_f0 = 0;
  B(b, self).image = 0;
  B(b, self).over_image = 0;
  button.field_1ac = 0;
  B(b, self).down_image = 0;
  B(b, self).disabled_image = 0;
  B(b, self).field_148 = 0;
  B(b, self).field_149 = 0;
  B(b, self).field_14a = 0;
  W(b, self).disabled = 0;
  W(b, self).is_down = 0;
  W(b, self).is_over = 0;
  button.field_190 = 0;
  button.field_194 = 0;
  button.field_198 = 0;
  button.field_1a4 = 0;
  button.field_1a8 = 0;
  button.field_18c = 0;
  button.field_1b0 = 0;
  button.field_1b4 = 0;
  button.field_1b8 = 0;
  button.field_1bc = 0;
  button.field_1c0 = 0;
  for (uint32_t i = 0; i < 6; ++i) {
    Call(c, b, sub_824919C8, 0x82187E98, {sp + 0x50, 0x828155F0 + i * 12});
    const uint32_t dest = ButtonColor(c, b, self, i);
    for (uint32_t j = 0; j < 4; ++j)
      pvz::guest::Write(b, dest + j * 4, pvz::guest::Read(b, sp + 0x50 + j * 4));
  }
  button.field_1d4 = zero;
  button.field_1dc = 0;
  button.field_1e0 = 0;
  button.field_1d8 = -1;
  button.field_1e4 = -1;
  const float one = pvz::guest::Float(b, 0x820B526C);
  button.field_1c4 = one;
  button.field_1c8 = one;
  button.field_1cc = one;
  button.field_1d0 = one;
  c.r3.u64 = self;
}
void Native_82188018(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, font = c.r4.u32;
  Frame f(c, b, 0x70);
  if (const uint32_t old = B(b, self).font)
    V(c, b, old, 0, 0x82188054, {old, 1});
  V(c, b, font, 0x2C, 0x82188068, {font});
  B(b, self).font = c.r3.u32;
}
void Native_821880C8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32, image = c.r5.u32, rect = c.r6.u32,
                 x = c.r7.u32 + uint32_t(REXCVAR_GET(pvz_native_button_821880C8_inject_error)), y = c.r8.u32;
  Frame f(c, b, 0xB0);
  const uint32_t sp = c.r1.u32;
  const bool highlighted = (W(b, self).is_over || W(b, self).gamepad_focus) && !W(b, self).disabled;
  const uint8_t pressed =
      B(b, self).field_148 ^ uint8_t(W(b, self).is_down && W(b, self).is_over && !W(b, self).disabled);
  if (G(b, self).field_1b0) {
    Call(c, b, sub_82187970, 0x82188180,
         {graphics, x, y, uint32_t(G(b, self).field_1a4), uint32_t(G(b, self).field_1a8), pressed,
          uint32_t(highlighted), self + 0xD4});
    return;
  }
  c.fpscr.disableFlushMode();
  c.r5.u64 = y;
  const double fx = double(float(int32_t(x))), fy = double(float(int32_t(y)));
  if (reinterpret_cast<pvz::xbox::ButtonRect*>(b + rect)->width) {
    Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(y))));
    Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(x))));
    c.f1.f64 = fx;
    c.f2.f64 = fy;
    Call(c, b, sub_8249C230, 0x821881DC, {graphics, uint32_t(B(b, self).image), y, rect, rect});
  } else {
    Store64(b, sp + 0x60, uint64_t(int64_t(int32_t(x))));
    Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(y))));
    c.f1.f64 = fx;
    c.f2.f64 = fy;
    Call(c, b, sub_8249D388, 0x8218821C, {graphics, image});
  }
  if (highlighted) {
    Call(c, b, sub_8249A0F0, 0x82188234, {graphics, 1});
    Call(c, b, sub_8249A100, 0x82188240, {graphics, 1});
    const int32_t count = int32_t(G(b, self).field_1dc);
    Store64(b, sp + 0x60, uint64_t(int64_t(count)));
    c.f1.f64 = double(float(double(float(count)) * double(pvz::guest::Float(b, 0x820B55D4))));
    Call(c, b, sub_825F9350, 0x8218826C, {});
    const double wave = double(float(c.f1.f64));
    double strength = double(float(
        std::fma(wave, double(pvz::guest::Float(b, 0x820B5348)), double(pvz::guest::Float(b, 0x820B526C)))));
    if (int32_t(G(b, self).field_1e0) <= 15)
      strength = double(float(double(float(G(b, self).field_1d4)) + strength));
    strength = double(float(strength * double(pvz::guest::Float(b, 0x820B50C4))));
    const double max = pvz::guest::Float(b, 0x820B52B0);
    if (strength > max)
      strength = max;
    strength = double(float(strength * double(pvz::guest::Float(b, 0x820B53F0))));
    const int64_t alpha = Fctiwz(strength);
    Store64(b, sp + 0x60, uint64_t(alpha));
    Call(c, b, sub_82491928, 0x821882D8, {sp + 0x60, 255, 255, 255, uint32_t(alpha)});
    Call(c, b, sub_8249A0C0, 0x821882E4, {graphics, c.r3.u32});
    c.f1.f64 = fx;
    c.f2.f64 = fy;
    Call(c, b, sub_8249D388, 0x821882F8, {graphics, image});
  }
}
void Native_82188CC0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_824AE260, 0x82188CD8, {self});
  c.fpscr.disableFlushMode();
  auto& button = B(b, self);
  const double zero = DoubleAt(b, 0x820B5358);
  if (!W(b, self).is_down && !W(b, self).is_over && double(button.over_alpha) > zero) {
    if (double(button.fade_out) > zero) {
      double next = double(button.over_alpha) - double(button.fade_out);
      if (next < zero)
        next = zero;
      button.over_alpha = next;
    } else
      button.over_alpha = zero;
  } else if (W(b, self).is_over && double(button.fade_in) > zero) {
    const double one = DoubleAt(b, 0x820B5388);
    if (double(button.over_alpha) < one) {
      double next = double(button.fade_in) + double(button.over_alpha);
      if (next > one)
        next = one;
      button.over_alpha = next;
    }
  }
  auto& game = G(b, self);
  game.field_1dc = uint32_t(game.field_1dc) + 1;
  if ((W(b, self).is_over || W(b, self).gamepad_focus) && !W(b, self).disabled) {
    const uint32_t count = uint32_t(game.field_1e0) + 1;
    game.field_1e0 = count;
    if (count == 15)
      game.field_1dc = 0;
  } else
    game.field_1e0 = 0;
}
void Native_82188DF0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, value = c.r4.u32;
  G(b, self).field_1d8 = value;
  G(b, self).field_1dc = 0;
  const uint32_t app = G(b, self).app;
  V(c, b, app, 0xE0, uint32_t(c.lr), {app, pvz::guest::Read(b, 0x82D0A4F8), 1});
}
void Native_82188E68(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, app = G(b, self).app;
  const int32_t sound = G(b, self).field_1e4;
  uint32_t value;
  if (sound != -1) {
    const uint32_t pointer = pvz::guest::Read(b, 0x82811D38 + (uint32_t(sound) << 2));
    value = pvz::guest::Read(b, pointer);
  } else
    value = pvz::guest::Read(b, G(b, self).field_1b0 ? 0x82D0A43Cu : 0x82D09F48u);
  V(c, b, app, 0xE0, uint32_t(c.lr), {app, value, 1});
}
void Native_82188EE0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x90);
  const uint32_t sp = c.r1.u32;
  Call(c, b, sub_82284D98, 0x82188F04, {sp + 0x50});
  Call(c, b, sub_821257D8, 0x82188F18, {self + 0xD4, c.r3.u32, 0, 0xFFFFFFFF});
  if (pvz::guest::Read(b, sp + 0x68) >= 8)
    Call(c, b, sub_825F8F50, 0x82188F2C, {pvz::guest::Read(b, sp + 0x54)});
}
auto& Start(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::StartButton*>(b + p);
}
void Native_82218040(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  auto& label = Start(b, self).label;
  if (uint32_t(label.capacity) >= 8)
    Call(c, b, sub_825F8F50, 0x8221806C, {pvz::guest::Read(b, self + 0x100)});
  label.length = 0;
  label.capacity = 7;
  label.storage[0] = 0;
  label.storage[1] = 0;
  Call(c, b, sub_82473C60, 0x82218088, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x8221809C, {self});
  c.r3.u64 = self;
}
void Native_822180A8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0xB0);
  const uint32_t sp = c.r1.u32;
  if (!uint32_t(Start(b, self).font))
    return;
  V(c, b, uint32_t(Start(b, self).font), 0x1C, 0x822180E0, {uint32_t(Start(b, self).font), self + 0xFC});
  const uint32_t x = uint32_t(int32_t(uint32_t(W(b, self).width) - c.r3.u32) / 2) +
                     uint32_t(REXCVAR_GET(pvz_native_button_822180A8_inject_error));
  V(c, b, uint32_t(Start(b, self).font), 4, 0x82218108, {uint32_t(Start(b, self).font)});
  const uint32_t y = uint32_t(int32_t(c.r3.u32 + uint32_t(W(b, self).height)) / 2) + 6;
  Call(c, b, sub_8249A0C0, 0x82218138, {graphics, self + (W(b, self).is_over ? 0xE4u : 0xD4u)});
  Call(c, b, sub_8249A0B8, 0x82218144, {graphics, uint32_t(Start(b, self).font)});
  Call(c, b, sub_8249B1A0, 0x82218158, {graphics, self + 0xFC, x, y});
  if (int32_t(Start(b, self).mode) == 2) {
    c.f1.f64 = double(float(Start(b, self).phase));
    Call(c, b, sub_825F9350, 0x82218174, {});
    c.fpscr.disableFlushMode();
    const double scaled = double(float(double(float(c.f1.f64)) * double(pvz::guest::Float(b, 0x820B53F0))));
    for (uint32_t i = 0; i < 4; ++i)
      pvz::guest::Write(b, sp + 0x60 + i * 4, pvz::guest::Read(b, 0x827D2D34 + i * 4));
    const int64_t alpha = Fctiwz(scaled);
    Store64(b, sp + 0x50, uint64_t(alpha));
    pvz::guest::Write(b, sp + 0x6C, uint32_t(alpha));
    Call(c, b, sub_8249A0C0, 0x822181C8, {graphics, sp + 0x60});
    Call(c, b, sub_8249B1A0, 0x822181DC, {graphics, self + 0xFC, x, y});
  }
  c.fpscr.disableFlushMode();
  float phase = float(double(float(Start(b, self).phase)) + double(pvz::guest::Float(b, 0x820B53F4)));
  Start(b, self).phase = phase;
  const float period = pvz::guest::Float(b, 0x820B53F8);
  if (phase > period)
    Start(b, self).phase = float(double(phase) - double(period));
}
template <bool Down> void StartInput(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, button = c.r4.u32, user = c.r5.u32, flags = c.r6.u32;
  const uint32_t listener = Start(b, self).listener;
  Start(b, self).last_user = user;
  c.r3.u64 = listener;
  if (listener)
    V(c, b, listener, Down ? 0xC0 : 0xC4, uint32_t(c.lr), {listener, button, user, flags});
}
void Native_82177D88(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  W(b, self).vtable = 0x820B99F4;
  W(b, self).listener_vtable = 0x820B9B70;
  S(b, self).button_listener_vtable = 0x820B9BA4;
  Call(c, b, sub_824B9A50, 0x82177DCC, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x82177DE0, {self});
  c.r3.u64 = self;
}
void Native_82177E00(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, width = c.r6.u32, height = c.r7.u32;
  Frame f(c, b, 0x80);
  V(c, b, self, 0x7C, 0x82177E28, {self, x, y, width, height});
  const bool horizontal = S(b, self).horizontal;
  const uint32_t thickness = int32_t(S(b, self).field_104) > 0 ? uint32_t(S(b, self).field_104)
                             : horizontal                      ? height
                                                               : width;
  if (horizontal) {
    const uint32_t up = S(b, self).up_button;
    V(c, b, up, 0x7C, 0x82177E6C, {up, 0, 0, thickness, height});
    const uint32_t down = S(b, self).down_button;
    V(c, b, down, 0x7C, 0x82177E90, {down, width - thickness, 0, thickness, height});
  } else {
    if (const uint32_t up = S(b, self).up_button)
      V(c, b, up, 0x7C, 0x82177ED8, {up, 0, 0, width, thickness});
    if (const uint32_t down = S(b, self).down_button)
      V(c, b, down, 0x7C, 0x82177F08, {down, 0, height - thickness, width, thickness});
  }
}
void Native_82177F10(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x80);
  c.fpscr.disableFlushMode();
  auto& scrollbar = S(b, self);
  const double maximum = scrollbar.max_value, page = scrollbar.page_size, old = scrollbar.value;
  const double last = maximum - page;
  if (old > last)
    scrollbar.value = last;
  const double zero = DoubleAt(b, 0x820B5358);
  if (double(scrollbar.value) < zero)
    scrollbar.value = zero;
  const uint32_t available = page < maximum;
  const uint32_t disabled = !available;
  V(c, b, self, 0x70, 0x82177F94, {self, disabled});
  if (const uint32_t up = scrollbar.up_button)
    V(c, b, up, 0x70, 0x82177FB4, {up, disabled});
  if (const uint32_t down = scrollbar.down_button)
    V(c, b, down, 0x70, 0x82177FD4, {down, disabled});
  if (scrollbar.invis_if_no_scroll) {
    V(c, b, self, 0x58, 0x82177FF8, {self, available});
    // Xbox tests one child and invokes the other. Preserve those exact guards.
    if (uint32_t(scrollbar.up_button)) {
      const uint32_t down = scrollbar.down_button;
      V(c, b, down, 0x58, 0x8217801C, {down, available});
    }
    if (uint32_t(scrollbar.down_button)) {
      const uint32_t up = scrollbar.up_button;
      V(c, b, up, 0x58, 0x82178040, {up, available});
    }
  }
  c.f1.f64 = double(scrollbar.value);
  if (c.f1.f64 != old) {
    const uint32_t listener = scrollbar.scroll_listener;
    V(c, b, listener, 0, 0x82178064, {listener, uint32_t(W(b, self).id)});
  }
}
auto& Stretch(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::StretchableGameButton*>(b + p);
}
void Native_82189070(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, listener = c.r4.u32, text = c.r5.u32, x = c.r6.u32;
  Frame f(c, b, 0xA0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xB4, self);
  Call(c, b, sub_82187D90, 0x8218909C, {self, 100, listener});
  auto& stretch = Stretch(b, self);
  stretch.alignment = 0;
  W(b, self).vtable = 0x820BA38C;
  W(b, self).listener_vtable = 0x820BA4C4;
  stretch.left = 0;
  stretch.right = 0;
  Call(c, b, sub_82284D98, 0x821890D0, {sp + 0x50, text});
  Call(c, b, sub_821257D8, 0x821890E4, {self + 0xD4, c.r3.u32, 0, 0xFFFFFFFF});
  if (pvz::guest::Read(b, sp + 0x68) >= 8)
    Call(c, b, sub_825F8F50, 0x821890F8, {pvz::guest::Read(b, sp + 0x54)});
  pvz::guest::Write(b, sp + 0x64, 0);
  b[sp + 0x54] = 0;
  b[sp + 0x55] = 0;
  G(b, self).field_194 = 500;
  G(b, self).field_1a8 = 42;
  pvz::guest::Write(b, sp + 0x68, 7);
  G(b, self).field_190 = x;
  Call(c, b, sub_82189250, 0x82189128, {self, 0});
  if (int32_t(G(b, self).field_1a4) < 210)
    G(b, self).field_1a4 = 210;
  G(b, self).field_190 = uint32_t(G(b, self).field_190) - uint32_t(int32_t(G(b, self).field_1a4) / 2);
  stretch.alignment = 0;
  c.r3.u64 = self;
}
void Native_821891B8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  W(b, self).vtable = 0x820BA38C;
  W(b, self).listener_vtable = 0x820BA4C4;
  Call(c, b, sub_82187F70, 0x821891F0, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x82189204, {self});
  c.r3.u64 = self;
}
void Native_82189220(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32, image = c.r5.u32, x = c.r7.u32, y = c.r8.u32;
  Call(c, b, sub_8249C058, uint32_t(c.lr),
       {graphics, image,
        uint32_t(G(b, self).field_1bc) + x + uint32_t(REXCVAR_GET(pvz_native_button_82189220_inject_error)),
        uint32_t(G(b, self).field_1c0) + y, uint32_t(G(b, self).field_1a4), uint32_t(G(b, self).field_1a8)});
}
void Native_82189250(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, source = c.r4.u32;
  Frame f(c, b, 0xB0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0x50, 0);
  const uint32_t old = B(b, self).font;
  c.r3.u64 = old;
  if (old == source)
    return;
  if (old)
    V(c, b, old, 0, 0x82189298, {old, 1});
  if (!source) {
    Call(c, b, sub_825DDB78, 0x821892A8, {0x20});
    const uint32_t font = c.r3.u32;
    pvz::guest::Write(b, sp + 0x54, font);
    if (font) {
      Call(c, b, sub_821264E0, 0x821892C8, {sp + 0x60, 0x820BA094});
      pvz::guest::Write(b, sp + 0x50, 1);
      Call(c, b, sub_824DD5E0, 0x821892F4, {font, uint32_t(G(b, self).app), sp + 0x60, 10, 0, 0, 0, 0});
    }
    B(b, self).font = c.r3.u32;
    if (font)
      Call(c, b, sub_8219F4D0, 0x82189314, {sp + 0x60});
  } else {
    V(c, b, source, 0x2C, 0x8218932C, {source});
    B(b, self).font = c.r3.u32;
  }
  if (uint32_t(B(b, self).font) && uint32_t(B(b, self).label.length)) {
    const uint32_t font = B(b, self).font;
    V(c, b, font, 0x1C, 0x8218935C, {font, self + 0xD4});
    Stretch(b, self).text_width = c.r3.u32;
    G(b, self).field_1a4 = uint32_t(Stretch(b, self).right) + uint32_t(Stretch(b, self).left) + c.r3.u32;
  } else
    Stretch(b, self).text_width = 0;
}
void Native_82189400(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, text = c.r4.u32;
  Frame f(c, b, 0xA0);
  const uint32_t sp = c.r1.u32;
  Call(c, b, sub_82284D98, 0x8218941C, {sp + 0x50, text});
  Call(c, b, sub_821257D8, 0x82189434, {self + 0xD4, c.r3.u32, 0, 0xFFFFFFFF});
  if (pvz::guest::Read(b, sp + 0x68) >= 8)
    Call(c, b, sub_825F8F50, 0x82189448, {pvz::guest::Read(b, sp + 0x54)});
  pvz::guest::Write(b, sp + 0x64, 0);
  pvz::guest::Write(b, sp + 0x68, 7);
  b[sp + 0x54] = 0;
  b[sp + 0x55] = 0;
  G(b, self).field_190 = uint32_t(G(b, self).field_190) + uint32_t(int32_t(G(b, self).field_1a4) / 2);
  V(c, b, self, 0x124, 0x8218948C, {self, uint32_t(B(b, self).font)});
  if (uint32_t(B(b, self).font)) {
    if (uint32_t(B(b, self).label.length)) {
      const uint32_t font = B(b, self).font;
      V(c, b, font, 0x1C, 0x821894B8, {font, self + 0xD4});
      if (c.r3.s32 > int32_t(G(b, self).field_1a4)) {
        Stretch(b, self).text_width = c.r3.u32;
        G(b, self).field_1a4 = uint32_t(Stretch(b, self).right) + uint32_t(Stretch(b, self).left) + c.r3.u32;
      }
    } else
      Stretch(b, self).text_width = 0;
    if (int32_t(B(b, self).field_f0) == 0 || int32_t(B(b, self).field_f0) == 1) {
      const uint32_t font = B(b, self).font;
      V(c, b, font, 0x1C, 0x82189510, {font, self + 0xD4});
    }
    V(c, b, uint32_t(B(b, self).font), 4, 0x82189524, {uint32_t(B(b, self).font)});
    V(c, b, uint32_t(B(b, self).font), 4, 0x82189538, {uint32_t(B(b, self).font)});
  }
  if (int32_t(Stretch(b, self).alignment) == -1)
    G(b, self).field_190 = uint32_t(G(b, self).field_190) - uint32_t(G(b, self).field_1a4);
  else if (int32_t(Stretch(b, self).alignment) == 0)
    G(b, self).field_190 = uint32_t(G(b, self).field_190) - uint32_t(int32_t(G(b, self).field_1a4) / 2);
}
void Native_821895A8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32, width = c.r6.u32, height = c.r7.u32;
  Frame f(c, b, 0x70);
  const uint32_t oldHalf = uint32_t(int32_t(G(b, self).field_1a4) / 2);
  G(b, self).field_194 = y;
  G(b, self).field_1a8 = height;
  G(b, self).field_1a4 = width;
  Stretch(b, self).text_width = width - uint32_t(Stretch(b, self).right) - uint32_t(Stretch(b, self).left);
  G(b, self).field_190 = oldHalf + x;
  if (uint32_t(B(b, self).font)) {
    if (int32_t(B(b, self).field_f0) == 0 || int32_t(B(b, self).field_f0) == 1) {
      const uint32_t font = B(b, self).font;
      V(c, b, font, 0x1C, 0x8218962C, {font, self + 0xD4});
    }
    V(c, b, uint32_t(B(b, self).font), 4, 0x82189640, {uint32_t(B(b, self).font)});
    V(c, b, uint32_t(B(b, self).font), 4, 0x82189654, {uint32_t(B(b, self).font)});
  }
  if (int32_t(Stretch(b, self).alignment) == -1)
    G(b, self).field_190 = x - uint32_t(G(b, self).field_1a4);
  else if (int32_t(Stretch(b, self).alignment) == 0)
    G(b, self).field_190 = x - uint32_t(int32_t(G(b, self).field_1a4) / 2);
}
void Native_821896A8(PPCContext& c, uint8_t* b) {
  const uint32_t out = c.r3.u32, self = c.r4.u32, left = c.r5.u32, right = c.r6.u32;
  Frame f(c, b, 0x60);
  const uint32_t oldLeft = Stretch(b, self).left, oldRight = Stretch(b, self).right;
  const uint32_t width = uint32_t(Stretch(b, self).text_width) + left + right;
  Stretch(b, self).right = right;
  Stretch(b, self).left = left;
  G(b, self).field_1a4 = width;
  auto& pair = *reinterpret_cast<pvz::xbox::ButtonPair*>(b + out);
  pair.first = oldLeft;
  pair.second = oldRight;
  V(c, b, self, 0x7C, 0x82189708,
    {self, uint32_t(G(b, self).field_190), uint32_t(G(b, self).field_194), width,
     uint32_t(G(b, self).field_1a8)});
  c.r3.u64 = out;
}
auto& Slider(uint8_t* b, uint32_t p) {
  return *reinterpret_cast<pvz::xbox::Slider*>(b + p);
}
void Native_821980E8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, id = c.r4.u32, listener = c.r5.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_824E2DE8, 0x8219811C,
       {self, pvz::guest::Read(b, 0x82D0A66C), pvz::guest::Read(b, 0x82D0A920), id, listener, 0});
  W(b, self).vtable = 0x820BAD94;
  W(b, self).listener_vtable = 0x820BAEC8;
  c.r3.u64 = self;
}
void Native_821981A0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82473588, 0x821981B0, {self});
  const uint32_t app = pvz::guest::Read(b, 0x82D0BC88);
  V(c, b, app, 0xE0, 0x821981D4, {app, pvz::guest::Read(b, 0x82D0A4F8), 1});
}
void Native_824E2DE8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, track = c.r4.u32, thumb = c.r5.u32, id = c.r6.u32, listener = c.r7.u32,
                 buttonListener = c.r8.u32;
  Frame f(c, b, 0x90);
  Call(c, b, sub_82473E88, 0x824E2E10, {self});
  auto& slider = Slider(b, self);
  slider.listener = listener;
  slider.button_listener = buttonListener;
  slider.track_image = track;
  slider.thumb_image = thumb;
  W(b, self).id = id;
  slider.value = 0.0;
  W(b, self).listener_vtable = 0x8206ACD8;
  W(b, self).vtable = 0x8206ABA4;
  slider.dragging = 0;
  slider.horizontal = 1;
  slider.drag_y = 0;
  slider.drag_x = 0;
  slider.left_down = 0;
  slider.right_down = 0;
  slider.sound = -1;
  slider.discrete = 0;
  slider.steps = 1;
  slider.step = 0;
  c.r3.u64 = self;
}
void Native_824E2E88(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  const double value = c.f1.f64;
  Frame f(c, b, 0x60);
  const double old = Slider(b, self).value;
  Slider(b, self).value = value;
  if (value < 0.0)
    Slider(b, self).value = 0.0;
  else if (value > 1.0)
    Slider(b, self).value = 1.0;
  c.f1.f64 = double(Slider(b, self).value);
  if (c.f1.f64 != old) {
    const uint32_t listener = Slider(b, self).listener;
    V(c, b, listener, 0, 0x824E2EEC, {listener, uint32_t(W(b, self).id)});
  }
  V(c, b, self, 0x18, 0x824E2F00, {self});
}
void Native_824E2F18(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, steps = c.r4.u32, step = c.r5.u32, sound = c.r6.u32;
  Frame f(c, b, 0x70);
  Slider(b, self).steps = steps;
  Slider(b, self).discrete = 1;
  V(c, b, self, 0x124, 0x824E2F54, {self, step, step, sound});
  Slider(b, self).sound = sound;
}
void Native_824E2F70(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, requested = c.r4.u32;
  Frame f(c, b, 0x70);
  const uint32_t steps = Slider(b, self).steps;
  uint32_t value = int32_t(requested) < 0 ? 0 : requested;
  if (int32_t(value) > int32_t(steps))
    value = steps;
  if (uint32_t(Slider(b, self).step) == value)
    return;
  Slider(b, self).step = value;
  Store64(b, c.r1.u32 + 0x50, uint64_t(int64_t(int32_t(steps))));
  Store64(b, c.r1.u32 + 0x50, uint64_t(int64_t(int32_t(value))));
  c.fpscr.disableFlushMode();
  c.f1.f64 = double(int32_t(value)) / double(int32_t(steps));
  V(c, b, self, 0x11C, 0x824E2FE8, {self});
  const uint32_t sound = Slider(b, self).sound;
  if (sound != 0xFFFFFFFF) {
    const uint32_t app = pvz::guest::Read(b, 0x828A39A4);
    V(c, b, app, 0xE0, 0x824E3010, {app, sound, 1});
  }
}
void Native_824E3030(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82472BF8, 0x824E304C, {self});
  if (W(b, self).gamepad_focus) {
    const double amount = DoubleAt(b, 0x8206AD08);
    if (Slider(b, self).left_down) {
      c.fpscr.disableFlushMode();
      c.f1.f64 = double(Slider(b, self).value) - amount;
      V(c, b, self, 0x11C, 0x824E3088, {self});
    }
    if (Slider(b, self).right_down) {
      c.fpscr.disableFlushMode();
      c.f1.f64 = double(Slider(b, self).value) + amount;
      V(c, b, self, 0x11C, 0x824E30B0, {self});
    }
  } else {
    Slider(b, self).left_down = 0;
    Slider(b, self).right_down = 0;
  }
}
void Native_824E30D8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x80);
  const uint32_t sp = c.r1.u32;
  const bool horizontal = Slider(b, self).horizontal;
  Call(c, b, horizontal ? sub_824AC938 : sub_824AC940, horizontal ? 0x824E3104 : 0x824E3190,
       {uint32_t(Slider(b, self).thumb_image)});
  const uint32_t dimension = horizontal ? uint32_t(W(b, self).width) : uint32_t(W(b, self).height);
  Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(dimension - c.r3.u32))));
  c.fpscr.disableFlushMode();
  const int64_t thumb = Fctiwz(double(int32_t(dimension - c.r3.u32)) * double(Slider(b, self).value));
  Store64(b, sp + 0x50, uint64_t(thumb));
  const uint32_t position = horizontal ? x : y;
  if (int32_t(position) >= int32_t(thumb)) {
    Call(c, b, horizontal ? sub_824AC938 : sub_824AC940, horizontal ? 0x824E3140 : 0x824E31CC,
         {uint32_t(Slider(b, self).thumb_image)});
    if (int32_t(position) < int32_t(c.r3.u32 + uint32_t(thumb))) {
      const uint32_t app = pvz::guest::Read(b, uint32_t(W(b, self).manager) + 0x70);
      Call(c, b, sub_82475EB8, horizontal ? 0x824E315C : 0x824E31E8, {app, 2});
      Slider(b, self).dragging = 1;
      if (horizontal)
        Slider(b, self).drag_x = position - uint32_t(thumb);
      else
        Slider(b, self).drag_y = position - uint32_t(thumb);
      return;
    }
  }
  Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(position))));
  Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(dimension))));
  c.f1.f64 = double(int32_t(position)) / double(int32_t(dimension));
  V(c, b, self, 0x11C, 0x824E3234, {self});
}
void Native_824E3240(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x80);
  const uint32_t sp = c.r1.u32;
  const bool horizontal = Slider(b, self).horizontal;
  Call(c, b, horizontal ? sub_824AC938 : sub_824AC940, horizontal ? 0x824E326C : 0x824E32C0,
       {uint32_t(Slider(b, self).thumb_image)});
  const uint32_t dimension = horizontal ? uint32_t(W(b, self).width) : uint32_t(W(b, self).height);
  const uint32_t extent = dimension - c.r3.u32;
  Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(extent))));
  c.fpscr.disableFlushMode();
  const int64_t thumb = Fctiwz(double(int32_t(extent)) * double(Slider(b, self).value));
  Store64(b, sp + 0x50, uint64_t(thumb));
  const uint32_t position = horizontal ? x : y;
  uint32_t mode = 0;
  if (int32_t(position) >= int32_t(thumb)) {
    Call(c, b, horizontal ? sub_824AC938 : sub_824AC940, horizontal ? 0x824E32A8 : 0x824E32FC,
         {uint32_t(Slider(b, self).thumb_image)});
    if (int32_t(position) < int32_t(c.r3.u32 + uint32_t(thumb)))
      mode = 2;
  }
  const uint32_t app = pvz::guest::Read(b, uint32_t(W(b, self).manager) + 0x70);
  Call(c, b, sub_82475EB8, 0x824E331C, {app, mode});
}
void Native_824E3328(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x80);
  if (!Slider(b, self).dragging)
    return;
  const uint32_t sp = c.r1.u32;
  const double old = Slider(b, self).value;
  const bool horizontal = Slider(b, self).horizontal;
  const uint32_t position =
      horizontal ? x - uint32_t(Slider(b, self).drag_x) : y - uint32_t(Slider(b, self).drag_y);
  Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(position))));
  Call(c, b, horizontal ? sub_824AC938 : sub_824AC940, horizontal ? 0x824E3380 : 0x824E33B4,
       {uint32_t(Slider(b, self).thumb_image)});
  const uint32_t dimension = horizontal ? uint32_t(W(b, self).width) : uint32_t(W(b, self).height);
  const uint32_t denominator = dimension - c.r3.u32;
  Store64(b, sp + 0x50, uint64_t(int64_t(int32_t(denominator))));
  c.fpscr.disableFlushMode();
  double value = double(int32_t(position)) / double(int32_t(denominator));
  Slider(b, self).value = value;
  if (value < 0.0)
    Slider(b, self).value = 0.0;
  value = Slider(b, self).value;
  if (value > 1.0)
    Slider(b, self).value = 1.0;
  c.f1.f64 = double(Slider(b, self).value);
  if (c.f1.f64 != old) {
    const uint32_t listener = Slider(b, self).listener;
    V(c, b, listener, 0, 0x824E3428, {listener, uint32_t(W(b, self).id)});
    V(c, b, self, 0x18, 0x824E343C, {self});
  }
}
void Native_824E3458(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  const uint32_t app = pvz::guest::Read(b, uint32_t(W(b, self).manager) + 0x70);
  Slider(b, self).dragging = 0;
  Call(c, b, sub_82475EB8, 0x824E3484, {app, 0});
  const uint32_t listener = Slider(b, self).listener;
  c.f1.f64 = double(Slider(b, self).value);
  V(c, b, listener, 0, 0x824E34A0, {listener, uint32_t(W(b, self).id)});
}
void Native_824E34B8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  if (!Slider(b, self).dragging) {
    const uint32_t app = pvz::guest::Read(b, uint32_t(W(b, self).manager) + 0x70);
    Call(c, b, sub_82475EB8, uint32_t(c.lr), {app, 0});
  }
}
void Native_824E34D8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, button = c.r4.u32, user = c.r5.u32, flags = c.r6.u32;
  if (button == 6) {
    const uint32_t listener = Slider(b, self).button_listener;
    c.r3.u64 = listener;
    if (listener)
      V(c, b, listener, 8, uint32_t(c.lr), {listener, uint32_t(W(b, self).id)});
  } else if (button == 3 || button == 19 || button == 2 || button == 18) {
    const bool right = button == 3 || button == 19;
    if (Slider(b, self).discrete)
      V(c, b, self, 0x124, uint32_t(c.lr),
        {self, uint32_t(Slider(b, self).step) + (right ? 1u : 0xFFFFFFFFu)});
    else if (right)
      Slider(b, self).right_down = 1;
    else
      Slider(b, self).left_down = 1;
  } else
    Call(c, b, sub_82472DE0, uint32_t(c.lr), {self, button, user, flags});
}
void Native_824E3590(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, button = c.r4.u32, user = c.r5.u32, flags = c.r6.u32;
  if (button == 3 || button == 19 || button == 2 || button == 18) {
    if (button == 3 || button == 19)
      Slider(b, self).right_down = 0;
    else
      Slider(b, self).left_down = 0;
    const uint32_t listener = Slider(b, self).listener;
    c.f1.f64 = double(Slider(b, self).value);
    V(c, b, listener, 0, uint32_t(c.lr), {listener, uint32_t(W(b, self).id)});
  } else
    Call(c, b, sub_82472F90, uint32_t(c.lr), {self, button, user, flags});
}
// The Xbox checks both division by zero and the signed divw overflow case.
int32_t TileCount(PPCContext& c, uint8_t* b, uint32_t end, uint32_t tile) {
  if (!tile)
    ppc_trap(c, b, 0);
  const uint32_t n = end - 1;
  const int32_t result =
      tile && !(int32_t(n) == INT32_MIN && int32_t(tile) == -1) ? int32_t(n) / int32_t(tile) : 0;
  if ((tile & ~(std::rotl(n, 1) - 1)) == UINT32_MAX)
    ppc_trap(c, b, 0);
  return result;
}
template <bool Rect> void SliderDraw(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32, rect = c.r5.u32;
  Frame f(c, b, Rect ? 0x1C0 : 0x1B0);
  const uint32_t sp = c.r1.u32;
  bool inject = Rect ? REXCVAR_GET(pvz_native_button_824E3AD0_inject_error)
                     : REXCVAR_GET(pvz_native_button_824E3610_inject_error);
  auto drawRect = [&](uint32_t target, uint32_t source, uint32_t x, uint32_t y, uint32_t sx, uint32_t sy,
                      uint32_t width, uint32_t height, uint32_t lr) {
    auto& r = *reinterpret_cast<pvz::xbox::ButtonRect*>(b + source);
    r.x = sx;
    r.y = sy;
    r.width = width;
    r.height = height;
    if (inject) {
      ++x;
      inject = false;
    }
    Call(c, b, sub_8249BC50, lr, {target, uint32_t(Slider(b, self).track_image), x, y, source});
  };
  if (const uint32_t image = Slider(b, self).track_image) {
    const bool widthThird = Slider(b, self).horizontal;
    Call(c, b, sub_824AC938,
         Rect ? (widthThird ? 0x824E3B0C : 0x824E3B18) : (widthThird ? 0x824E3648 : 0x824E3654), {image});
    const uint32_t tileW = widthThird ? uint32_t(c.r3.s32 / 3) : c.r3.u32;
    const bool heightThird = !Slider(b, self).horizontal;
    Call(c, b, sub_824AC940,
         Rect ? (heightThird ? 0x824E3B3C : 0x824E3B30) : (heightThird ? 0x824E3678 : 0x824E366C),
         {uint32_t(Slider(b, self).track_image)});
    const uint32_t tileH = heightThird ? uint32_t(c.r3.s32 / 3) : c.r3.u32;
    if (Slider(b, self).horizontal) {
      const uint32_t y = uint32_t(int32_t(uint32_t(W(b, self).height) - tileH) / 2);
      drawRect(graphics, sp + 0x90, 0, y, 0, 0, tileW, tileH, Rect ? 0x824E3B88 : 0x824E36C4);
      Call(c, b, sub_8249DB80, Rect ? 0x824E3B94 : 0x824E36D0, {sp + 0xC0, graphics});
      Call(c, b, sub_8249C6A8, Rect ? 0x824E3BB4 : 0x824E36F0,
           {sp + 0xC0, tileW, y, uint32_t(W(b, self).width) - 2 * tileW, tileH});
      uint32_t end = uint32_t(W(b, self).width) - tileW;
      int32_t count = TileCount(c, b, end, tileW);
      uint32_t i = 0, x = tileW;
      while (int32_t(i) < count) {
        drawRect(sp + 0xC0, sp + 0xB0, x, y, tileW, 0, tileW, tileH, Rect ? 0x824E3C0C : 0x824E3748);
        ++i;
        x += tileW;
        end = uint32_t(W(b, self).width) - tileW;
        count = TileCount(c, b, end, tileW);
      }
      drawRect(graphics, sp + 0x60, end, y, 2 * tileW, 0, tileW, tileH, Rect ? 0x824E3C64 : 0x824E37A0);
    } else {
      drawRect(graphics, sp + 0x70, 0, 0, 0, 0, tileW, tileH, Rect ? 0x824E3C88 : 0x824E37C4);
      Call(c, b, sub_8249DB80, Rect ? 0x824E3C94 : 0x824E37D0, {sp + 0xC0, graphics});
      Call(c, b, sub_8249C6A8, Rect ? 0x824E3CB4 : 0x824E37F0,
           {sp + 0xC0, 0, tileH, tileW, uint32_t(W(b, self).height) - 2 * tileH});
      uint32_t end = uint32_t(W(b, self).height) - tileH;
      int32_t count = TileCount(c, b, end, tileH);
      uint32_t i = 0, y = tileH;
      while (int32_t(i) < count) {
        drawRect(sp + 0xC0, sp + 0x80, 0, y, 0, tileH, tileW, tileH, Rect ? 0x824E3D0C : 0x824E3848);
        ++i;
        y += tileH;
        end = uint32_t(W(b, self).height) - tileH;
        count = TileCount(c, b, end, tileH);
      }
      drawRect(graphics, sp + 0xA0, 0, end, 0, 2 * tileH, tileW, tileH, Rect ? 0x824E3D64 : 0x824E38A0);
    }
    Call(c, b, sub_8249DD50, Rect ? 0x824E3D6C : 0x824E38A8, {sp + 0xC0});
  }
  auto knob = [&](bool over) {
    const bool horizontal = Slider(b, self).horizontal;
    const uint32_t image = Slider(b, self).thumb_image;
    if (!image)
      return;
    auto dimension = [&](bool width) -> uint32_t {
      if constexpr (Rect) {
        auto& r = *reinterpret_cast<pvz::xbox::ButtonRect*>(b + rect);
        return width ? uint32_t(r.width) : uint32_t(r.height);
      } else
        return width ? uint32_t(W(b, self).width) : uint32_t(W(b, self).height);
    };
    auto offset = [&](bool x) -> uint32_t {
      if constexpr (Rect) {
        auto& r = *reinterpret_cast<pvz::xbox::ButtonRect*>(b + rect);
        return x ? uint32_t(r.x) : uint32_t(r.y);
      } else
        return 0;
    };
    Call(c, b, sub_824AC940,
         Rect ? (horizontal ? 0x824E3D88 : 0x824E3DF0)
              : (horizontal ? (over ? 0x824E39AC : 0x824E38C4) : (over ? 0x824E3A10 : 0x824E3928)),
         {image});
    uint32_t x, y;
    if (horizontal) {
      y = uint32_t(int32_t(dimension(false) - c.r3.u32) / 2) + offset(false);
      Call(c, b, sub_824AC938, Rect ? 0x824E3DA4 : (over ? 0x824E39C4 : 0x824E38DC),
           {uint32_t(Slider(b, self).thumb_image)});
      const int32_t extent = int32_t(dimension(true) - c.r3.u32);
      Store64(b, sp + 0x50, uint64_t(int64_t(extent)));
      c.fpscr.disableFlushMode();
      const int64_t value = Fctiwz(double(extent) * double(Slider(b, self).value));
      Store64(b, sp + 0x50, uint64_t(value));
      x = uint32_t(value) + offset(true);
    } else {
      const int32_t extent = int32_t(dimension(false) - c.r3.u32);
      const uint32_t yOffset = offset(false);
      Store64(b, sp + 0x50, uint64_t(int64_t(extent)));
      c.fpscr.disableFlushMode();
      const int64_t value = Fctiwz(double(extent) * double(Slider(b, self).value));
      Store64(b, sp + 0x50, uint64_t(value));
      Call(c, b, sub_824AC938, Rect ? 0x824E3E24 : (over ? 0x824E3A40 : 0x824E3958),
           {uint32_t(Slider(b, self).thumb_image)});
      x = uint32_t(int32_t(dimension(true) - c.r3.u32) / 2) + offset(true);
      y = pvz::guest::Read(b, sp + 0x54) + yOffset;
    }
    if (inject) {
      ++x;
      inject = false;
    }
    Call(c, b, sub_8249D0E8, Rect ? 0x824E3E50 : (over ? 0x824E3A60 : 0x824E3978),
         {graphics, uint32_t(Slider(b, self).thumb_image), x, y});
  };
  knob(false);
  if constexpr (!Rect) {
    if (W(b, self).is_over) {
      Call(c, b, sub_8249A0F0, 0x824E3990, {graphics, 1});
      knob(true);
      Call(c, b, sub_8249A0F0, 0x824E3A6C, {graphics, 0});
    }
  }
}
#endif
} // namespace
#ifndef PVZ_BATCH_SYNTHETIC_TEST
#define LEAF(A, P, N, O, R, I)                                                                               \
  PVZ_NATIVE_BATCH_REPLACEMENT(b014, P, button_##A, sub_##A, N, pvz::native::Safety::DeclaredOutputsOnly, O, \
                               R)
#define SNAP(A, P, N)                                                                                        \
  PVZ_NATIVE_BATCH_REPLACEMENT(b014, P, button_##A, sub_##A, N, pvz::native::Safety::SnapshotsOnly,          \
                               noOutputs, NoRanges)
#endif
// Gates are selected only after measured discovery and the appropriate proof.
LEAF(82188CB8, true, Disabled, noOutputs, DisabledRange, Disabled)
LEAF(824AE510, true, Identity, r3out, NoRanges, Identity)
LEAF(824E3028, true, True, r3out, NoRanges, Identity)
LEAF(8254C0F8, true, Checked, r3out, NoRanges, Checked)
LEAF(824AE030, true, Pressed, r3out, NoRanges, Pressed)
SNAP(82120008, true, Native_82120008)
SNAP(821DDC80, false, Native_821DDC80)
SNAP(824ADFC0, false, Native_824ADFC0)
SNAP(824AE098, false, Native_824AE098)
SNAP(824AE0D0, false, Native_824AE0D0)
SNAP(824AE138, true, Native_824AE138)
SNAP(824AE180, false, Native_824AE180)
SNAP(824AE1E8, false, Native_824AE1E8)
SNAP(824AE260, true, Native_824AE260)
SNAP(824AE388, true, Native_824AE388)
SNAP(824AE610, true, Native_824AE610)
SNAP(824AE730, true, Native_824AE730)
SNAP(824AE8B0, true, Native_824AE8B0)
SNAP(824AF248, true, Native_824AF248)
SNAP(824AE998, true, Native_824AE998)
SNAP(8254C088, false, Native_8254C088)
SNAP(8254C100, false, Native_8254C100)
SNAP(8254C280, false, Native_8254C280)
SNAP(8254C2F0, false, Native_8254C2F0)
SNAP(8254C360, false, Native_8254C360)
LEAF(8254C370, true, Identity, r3out, NoRanges, Identity)
SNAP(8254C380, false, Native_8254C380)
SNAP(824DCF20, true, Native_824DCF20)
SNAP(824DCFD0, false, Native_824DCFD0)
SNAP(82582798, false, Native_82582798)
SNAP(825827F8, false, Native_825827F8)
SNAP(82582818, false, Native_82582818)
SNAP(82582B80, false, Native_82582B80)
SNAP(82593CE8, true, Native_82593CE8)
SNAP(82593D98, true, Native_82593D98)
SNAP(82593E30, true, Native_82593E30)
SNAP(82187918, true, Native_82187918)
SNAP(82187D90, true, Native_82187D90)
SNAP(82188018, true, Native_82188018)
LEAF(82188088, true, GamePressed, r3out, NoRanges, GamePressed)
SNAP(821880C8, true, Native_821880C8)
SNAP(82188CC0, true, Native_82188CC0)
SNAP(82188DF0, true, Native_82188DF0)
LEAF(82188E20, true, GameCoordinates, noOutputs, GameCoordinateRanges, GameCoordinates)
SNAP(82188E68, true, Native_82188E68)
SNAP(82188EE0, true, Native_82188EE0)
LEAF(82663EB0, true, ZeroPair, noOutputs, PairRange, Pair)
SNAP(82218040, true, Native_82218040)
SNAP(822180A8, true, Native_822180A8)
SNAP(82218208, false, StartInput<true>)
SNAP(82218230, false, StartInput<false>)
SNAP(82177D88, false, Native_82177D88)
SNAP(82177E00, false, Native_82177E00)
SNAP(82177F10, false, Native_82177F10)
LEAF(82178070, true, CustomTrack, r3out, NoRanges, CustomTrack)
SNAP(82189070, false, Native_82189070)
SNAP(821891B8, false, Native_821891B8)
SNAP(82189220, false, Native_82189220)
SNAP(82189250, false, Native_82189250)
SNAP(82189400, false, Native_82189400)
SNAP(821895A8, false, Native_821895A8)
SNAP(821896A8, false, Native_821896A8)
SNAP(821980E8, true, Native_821980E8)
SNAP(821981A0, false, Native_821981A0)
SNAP(824E2DE8, true, Native_824E2DE8)
SNAP(824E2E88, true, Native_824E2E88)
SNAP(824E2F18, false, Native_824E2F18)
SNAP(824E2F70, false, Native_824E2F70)
SNAP(824E3030, true, Native_824E3030)
SNAP(824E30D8, true, Native_824E30D8)
SNAP(824E3240, true, Native_824E3240)
SNAP(824E3328, true, Native_824E3328)
SNAP(824E3458, true, Native_824E3458)
SNAP(824E34B8, true, Native_824E34B8)
SNAP(824E34D8, false, Native_824E34D8)
SNAP(824E3590, false, Native_824E3590)
SNAP(824E3610, true, SliderDraw<false>)
SNAP(824E3AD0, false, SliderDraw<true>)
#undef LEAF
#undef SNAP
