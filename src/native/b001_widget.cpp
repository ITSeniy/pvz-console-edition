// B001, Xbox 584109FF 0.0.1.4. Each address is a distinct registration.
// Source of behavior: generated/default and .cache/pvz-image.bin (SHA in T13).
// PC Widget.cpp was read as a naming/algorithm reference; no GPL code used.
// Unknown method names deliberately retain their Xbox address.
#include "replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/Widget.h"
#ifndef PVZ_B001_SYNTHETIC_TEST
#include "guest_call.h"
#endif

#ifndef PVZ_B001_SYNTHETIC_TEST
PVZ_NATIVE_BATCH(b001)
REXCVAR_DECLARE(bool, pvz_native_widget_82473730_inject_error);
#endif

REX_EXTERN(sub_824A62C0);
REX_EXTERN(sub_824A5FE8);
REX_EXTERN(sub_82475EB8);
REX_EXTERN(sub_824A68F0);
REX_EXTERN(sub_824A6208);
REX_EXTERN(sub_824A5F48);
REX_EXTERN(sub_82491858);
REX_EXTERN(sub_824918E0);
REX_EXTERN(sub_82491928);
REX_EXTERN(sub_824A5840);
REX_EXTERN(sub_824745D0);
REX_EXTERN(sub_821B0F00);
REX_EXTERN(sub_82199CE8);
REX_EXTERN(sub_825FB4E8);
REX_EXTERN(sub_825FB2F8);
REX_EXTERN(sub_82564B78);
REX_EXTERN(sub_82492FD8);
REX_EXTERN(sub_82492F28);
REX_EXTERN(sub_825557B0);
REX_EXTERN(sub_8254C4A0);
REX_EXTERN(sub_825F8F50);
REX_EXTERN(sub_82473C60);
REX_EXTERN(sub_8249E418);
REX_EXTERN(sub_8249E900);
REX_EXTERN(sub_8249EF58);
REX_EXTERN(sub_824AC938);
REX_EXTERN(sub_824AC940);
REX_EXTERN(sub_8249DDB0);
REX_EXTERN(sub_8249C6A8);
REX_EXTERN(sub_8249D0E8);
REX_EXTERN(sub_8249A0B0);
REX_EXTERN(sub_8249A0C0);
REX_EXTERN(sub_8249B1A0);

namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
using Widget = pvz::xbox::Widget;

#ifdef PVZ_B001_SYNTHETIC_TEST
// Input domains are declared with this batch; LEAF selects one by name.
// Registers and Xbox Widget offsets below are independent of host objects.
uint64_t Random(uint64_t& s) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
constexpr uint64_t words[]{0,1,0x7fffffff,0x80000000,0xffffffff,
    0x100000000,0x7fffffffffffffff,0x8000000000000000,~uint64_t{}};
const SyntheticInputs<PPCContext> IdentityInputs{
    "r3.u64: 0,1,INT32_MAX,INT32_MIN,UINT32_MAX,2^32,INT64_MAX,INT64_MIN,UINT64_MAX; 1024 xorshift64 seed 42",
    std::size(words),1024,42, [](size_t i,PPCContext& c,uint8_t*,uint64_t& s) {
      c.r3.u64=i<std::size(words)?words[i]:Random(s);
    }};
const SyntheticInputs<PPCContext> FocusByteInputs{
    "r3=0x1000; Widget.wants_focus (+A1): all 256 byte values, plus 1024 seeded cases",
    256,1024,42, [](size_t i,PPCContext& c,uint8_t* b,uint64_t& s) {
      c.r3.u64=0x1000; b[0x10A1]=uint8_t(i<256?i:Random(s));
    }};
// The original loops forever on INT32_MAX (eventually its word divisor is 0).
// Its terminating domain is <= 1874919423: below the largest word divisor.
// Include both sides of EVERY positive/negative wrapped-divisor boundary that
// lies inside that domain, in addition to each ordinary decimal transition.
const auto digitBoundaries=[] {
  std::vector<int32_t> v{INT32_MIN,-1,0,1,1874919423};
  uint32_t divisor=10;
  while (divisor) {
    int32_t d=std::bit_cast<int32_t>(divisor);
    for (int64_t n=int64_t(d)-1;n<=int64_t(d)+1;++n)
      if(n>=INT32_MIN && n<=1874919423) v.push_back(int32_t(n));
    divisor*=10u;
  }
  std::sort(v.begin(),v.end()); v.erase(std::unique(v.begin(),v.end()),v.end()); return v;
}();
const SyntheticInputs<PPCContext> DigitsInputs{
    "r4.s32: every decimal/wrapped divisor +/-1 in terminating domain [INT32_MIN,1874919423]; 1024 seeded values; larger inputs do not terminate",
    digitBoundaries.size(),1024,42, [](size_t i,PPCContext& c,uint8_t*,uint64_t& s) {
      c.r4.u64=i<digitBoundaries.size()?uint32_t(digitBoundaries[i]):uint32_t(int64_t(INT32_MIN)+int64_t(Random(s)%4022403072ULL));
    }};
constexpr uint32_t colorCounts[]{0,1,2,3};
constexpr uint32_t colorIndices[]{0,1,2,3,4,0x7fffffff};
constexpr uint32_t aliases[]{0x2000,0x3000,0x3010,0x3020,0x4000,0x3004,0x4004};
const SyntheticInputs<PPCContext> ColorInputs{
    "r4=Widget@1000; colors_begin/end (+84/+88) count 0..3; r5 index 0..4,INT_MAX; r6 default@4000; output distinct or aliases source/default including partial overlap; 1024 seeded colors",
    std::size(colorCounts)*std::size(colorIndices)*std::size(aliases),1024,42,
    [](size_t i,PPCContext& c,uint8_t* b,uint64_t& s) {
      size_t key=i<168?i:size_t(Random(s)%168);
      c.r3.u64=aliases[key%7]; key/=7; c.r5.u64=colorIndices[key%6];key/=6;
      c.r4.u64=0x1000;c.r6.u64=0x4000;
      pvz::guest::Write(b,0x1084,0x3000);pvz::guest::Write(b,0x1088,0x3000+colorCounts[key]*16);
      for(uint32_t at=0x3000;at<0x3040;at+=4) pvz::guest::Write(b,at,uint32_t(Random(s)));
      for(uint32_t at=0x4000;at<0x4020;at+=4) pvz::guest::Write(b,at,uint32_t(Random(s)));
    }};
#endif

Widget& W(uint8_t* base, uint32_t address) {
  return *reinterpret_cast<Widget*>(base + address);
}
const std::array<RegisterOutput, 1> r3out{{{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}}};
const std::array<RegisterOutput, 0> noOutputs{};
auto NoRanges(const PPCContext&) { return std::array<Range, 0>{}; }
// A blr preserves the entire r3, including its high half. It has no memory
// effects. r3 is an identity output of this instruction, not a C++ return value.
void NoOp(PPCContext&, uint8_t*) {}
void True(PPCContext& ctx, uint8_t*) { ctx.r3.u64 = 1; }
void WantsFocus(PPCContext& ctx, uint8_t* base) { ctx.r3.u64 = W(base, ctx.r3.u32).wants_focus; }
template<bool Value> void Focus(PPCContext& ctx, uint8_t* base) { W(base, ctx.r3.u32).flag_73 = Value; }
auto FocusRange(const PPCContext& c) { return std::array{Range{c.r3.u32 + uint32_t(offsetof(Widget, flag_73)), 1}}; }
void Leave(PPCContext& ctx, uint8_t* base) {
  auto& w = W(base, ctx.r3.u32);
  w.gamepad_focus = 0; w.is_over = 0; w.is_down = 0;
}
auto LeaveRanges(const PPCContext& c) {
  return std::array{Range{c.r3.u32 + uint32_t(offsetof(Widget, gamepad_focus)), 1},
                    Range{c.r3.u32 + uint32_t(offsetof(Widget, is_down)), 2}};
}
void Navigation(PPCContext& ctx, uint8_t* base) {
  auto& w = W(base, ctx.r3.u32);
  w.field_b0 = ctx.r4.u32; w.navigation_1 = ctx.r5.u32;
  w.field_b8 = ctx.r6.u32; w.field_bc = ctx.r7.u32;
}
auto NavigationRange(const PPCContext& c) { return std::array{Range{c.r3.u32 + uint32_t(offsetof(Widget, field_b0)), 16}}; }
void Proxy(PPCContext& ctx, uint8_t* base) { W(base, ctx.r3.u32).field_ac = ctx.r4.u32; }
auto ProxyRange(const PPCContext& c) { return std::array{Range{c.r3.u32 + uint32_t(offsetof(Widget, field_ac)), 4}}; }
void Next(PPCContext& ctx, uint8_t* base) {
  unsigned direction;
  switch (ctx.r4.s32) {
    case 0: case 16: direction = 0; break;
    case 1: case 17: direction = 1; break;
    case 2: case 18: direction = 2; break;
    case 3: case 19: direction = 3; break;
    default: ctx.r3.u64 = 0; return;
  }
  auto next = [&](uint32_t address) -> uint32_t {
    const auto& w = W(base, address);
    switch (direction) {
      case 0: return w.field_b0;
      case 1: return w.navigation_1;
      case 2: return w.field_b8;
      default: return w.field_bc;
    }
  };
  uint32_t result = next(ctx.r3.u32);
  // Xbox skips only invisible widgets; it does not test disabled or break cycles.
  while (result && !W(base, result).visible) result = next(result);
  ctx.r3.u64 = result;
}
void Digits(PPCContext& ctx, uint8_t*) {
  int32_t divisor = 10;
  uint64_t count = 1;
  // mulli truncates/sign-extends at 32 bits. Unsigned multiplication avoids C++ UB.
  while (ctx.r4.s32 >= divisor) {
    divisor = std::bit_cast<int32_t>(uint32_t(divisor) * 10u);
    ++count;
  }
  ctx.r3.u64 = count;
}
void Contains(PPCContext& ctx, uint8_t* base) {
  const auto& w = W(base, ctx.r3.u32);
  const int64_t x = int32_t(w.x), y = int32_t(w.y);
  // Xbox add operates in 64 bits on zero-extended lwz, then cmpw uses low 32.
  const auto right = std::bit_cast<int32_t>(uint32_t(w.x) + uint32_t(w.width));
  const auto bottom = std::bit_cast<int32_t>(uint32_t(w.y) + uint32_t(w.height));
  ctx.r3.u64 = ctx.r4.s32 >= x && ctx.r4.s32 < right && ctx.r5.s32 >= y && ctx.r5.s32 < bottom;
}
void Insets(PPCContext& ctx, uint8_t* base) {
  const auto& w = W(base, ctx.r4.u32);
  // Load every input before the first output store, as in 82473960..82473994.
  const uint32_t left = w.mouse_insets[0], top = w.mouse_insets[1];
  const uint32_t width = uint32_t(w.width) - uint32_t(w.mouse_insets[2]) - left;
  const uint32_t height = uint32_t(w.height) - uint32_t(w.mouse_insets[3]) - top;
  const uint32_t x = uint32_t(w.x) + left, y = uint32_t(w.y) + top;
  pvz::guest::Write(base, ctx.r3.u32 + 8, width);
  pvz::guest::Write(base, ctx.r3.u32 + 12, height);
  pvz::guest::Write(base, ctx.r3.u32, x);
  pvz::guest::Write(base, ctx.r3.u32 + 4, y);
}
auto RectRange(const PPCContext& c) { return std::array{Range{c.r3.u32, 16}}; }

#ifndef PVZ_B001_SYNTHETIC_TEST
// Snapshot-only wrappers call guest code once. A real guest stack frame keeps
// child home slots away from the caller; host locals survive volatile registers.
using pvz::native::Frame;
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Virtual;

void Visible(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32; const auto value = ctx.r4.u8;
  auto& w = W(base, self);
  if (w.visible == value) return;
  Frame frame(ctx, base, 128);
  w.visible = value;
  Virtual(ctx, base, self, value ? 16 : 24, 0x824729B0);
  if (uint32_t(w.manager)) {
    Args(ctx, {w.manager}); ctx.lr = 0x824729C0; sub_824A62C0(ctx, base);
  }
}
void Overlay(PPCContext& ctx, uint8_t* base) { Virtual(ctx, base, ctx.r3.u32, 140, uint32_t(ctx.lr)); }
void Resize(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, x = ctx.r4.u32, y = ctx.r5.u32, width = ctx.r6.u32, height = ctx.r7.u32;
  auto& w = W(base, self);
  if (uint32_t(w.x) == x && uint32_t(w.y) == y && uint32_t(w.width) == width && uint32_t(w.height) == height) return;
  Frame frame(ctx, base, 128);
  Virtual(ctx, base, self, 24, 0x82472A5C);
  w.x = x; w.y = y; w.width = width; w.height = height;
  Args(ctx, {self}); Virtual(ctx, base, self, 16, 0x82472A80);
  if (uint32_t(w.manager)) { Args(ctx, {w.manager}); ctx.lr = 0x82472A90; sub_824A62C0(ctx, base); }
}
void ResizeRect(PPCContext& ctx, uint8_t* base) {
  const auto rect = ctx.r4.u32;
  Args(ctx, {ctx.r3.u32,pvz::guest::Read(base, rect),pvz::guest::Read(base, rect+4),
      pvz::guest::Read(base, rect+8),pvz::guest::Read(base, rect+12)});
  Virtual(ctx, base, ctx.r3.u32, 124, uint32_t(ctx.lr));
}
void Move(PPCContext& ctx, uint8_t* base) {
  const auto& w = W(base, ctx.r3.u32);
  ctx.r6.u64 = uint32_t(w.width); ctx.r7.u64 = uint32_t(w.height);
  Virtual(ctx, base, ctx.r3.u32, 124, uint32_t(ctx.lr));
}
void FluentDisable(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32; Frame frame(ctx, base, 128);
  Virtual(ctx, base, self, 144, 0x82472BBC); ctx.r3.u64 = self;
}
void Enter(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, parent = W(base, self).parent;
  auto& w = W(base, self); w.gamepad_focus = 1; w.is_over = 1;
  Args(ctx, {parent,self});
  if (parent) Virtual(ctx, base, parent, 80, uint32_t(ctx.lr));
}
void ProxyFocus(PPCContext& ctx, uint8_t* base) {
  const uint32_t proxy = W(base, ctx.r3.u32).field_ac;
  if (proxy) { ctx.r3.u64 = proxy; Virtual(ctx, base, proxy, 188, uint32_t(ctx.lr)); }
}
void ProxyRelease(PPCContext& ctx, uint8_t* base) {
  const uint32_t proxy = W(base, ctx.r3.u32).field_ac;
  if (proxy) { ctx.r3.u64 = proxy; ctx.r6.u64 |= 8; Virtual(ctx, base, proxy, 196, uint32_t(ctx.lr)); }
}
void MouseUp(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, x = ctx.r4.u32, y = ctx.r5.u32;
  const int32_t button = ctx.r6.s32;
  Frame frame(ctx, base, 128);
  Virtual(ctx, base, self, 232, 0x8247307C);
  const uint32_t kind = button == 3 ? 2 : button < 0 ? 1 : 0;
  const uint32_t count = button == 3 ? 1 : button < 0 ? 0u - uint32_t(button) : uint32_t(button);
  Args(ctx, {self,x,y,kind,count}); Virtual(ctx, base, self, 224, 0x824730B0);
}
auto& Manager(uint8_t* base, uint32_t address) {
  return *reinterpret_cast<pvz::xbox::WidgetManagerInput*>(base + address);
}
void Disabled(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32; const auto value = ctx.r4.u8;
  auto& w = W(base, self);
  if (w.disabled == value) return;
  Frame frame(ctx, base, 128); w.disabled = value;
  if (value && uint32_t(w.manager)) {
    Args(ctx, {w.manager,self}); ctx.lr = 0x82472B20; sub_824A68F0(ctx, base);
  }
  Args(ctx, {self}); Virtual(ctx, base, self, 16, 0x82472B34);
  if (!value && uint32_t(w.manager)) {
    const auto& m = Manager(base, w.manager);
    Args(ctx, {self,uint32_t(m.mouse_x),uint32_t(m.mouse_y)});
    Virtual(ctx, base, self, 276, 0x82472B64);
    if (ctx.r3.u8) {
      const auto manager = uint32_t(w.manager); const auto& current = Manager(base, manager);
      Args(ctx, {manager,uint32_t(current.mouse_x),uint32_t(current.mouse_y)});
      ctx.lr = 0x82472B7C; sub_824A6208(ctx, base);
    }
  }
}
void KeyDown(PPCContext& ctx, uint8_t* base) {
  if (ctx.r4.s32 != 9) return;
  const auto& w = W(base, ctx.r3.u32); const uint32_t manager = w.manager;
  const uint32_t target = Manager(base, manager).shift_down ? uint32_t(w.field_a4) : uint32_t(w.field_a8);
  ctx.r3.u64 = manager; ctx.r4.u64 = target;
  if (target) sub_824A5FE8(ctx, base);
}
void Finger(PPCContext& ctx, uint8_t* base) {
  const uint32_t manager = W(base, ctx.r3.u32).manager;
  if (!manager) return;
  const uint32_t on = ctx.r4.u8 != 0;
  Args(ctx, {Manager(base, manager).app,on}); sub_82475EB8(ctx, base);
}
void GamepadDown(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, button = ctx.r4.u32, user = ctx.r5.u32, flags = ctx.r6.u32;
  auto& w = W(base, self);
  if (int32_t(w.user_index) != -1 && uint32_t(w.user_index) != user) return;
  const uint32_t proxy = w.field_ac;
  if (proxy && int32_t(W(base, proxy).user_index) != -1 && uint32_t(W(base, proxy).user_index) != user) return;
  Frame frame(ctx, base, 128);
  Args(ctx, {self,button}); Virtual(ctx, base, self, 184, 0x82472E48);
  const uint32_t target = ctx.r3.u32;
  if (target && uint32_t(w.manager)) {
    int direction = -1;
    if (button <= 3 && !(flags & 2)) direction = int(button);
    if (button >= 16 && button <= 19) direction = int(button - 16);
    if (direction >= 0) {
      Args(ctx, {target,uint32_t(direction)}); Virtual(ctx, base, target, 200, 0x82472ED4);
      Args(ctx, {self,uint32_t(direction)}); Virtual(ctx, base, self, 200, 0x82472EEC);
    }
    // D-pad with repeat bit 2 skips both notification and manager focus change.
    if (!(button <= 3 && (flags & 2))) {
      Args(ctx, {w.manager,target}); ctx.lr = 0x82472EF8; sub_824A5F48(ctx, base);
    }
  }
  const uint32_t current_proxy = w.field_ac;
  if (current_proxy) {
    Args(ctx, {current_proxy,button,user,flags | 8}); Virtual(ctx, base, current_proxy, 192, 0x82472F24);
  }
}
#endif
void Color(PPCContext& ctx, uint8_t* base) {
  const auto& w = W(base, ctx.r4.u32);
  const uint32_t first = w.colors_begin, end = w.colors_end;
  const int32_t count = std::bit_cast<int32_t>(end - first) >> 4;
  uint32_t source = ctx.r6.u32;
  if (ctx.r5.s32 < count) {
    if (ctx.r5.u32 >= uint32_t(count)) ppc_trap(ctx, base, 22);
    source = first + (ctx.r5.u32 << 4);
  }
  const uint32_t a = pvz::guest::Read(base, source), b = pvz::guest::Read(base, source + 4);
  const uint32_t c = pvz::guest::Read(base, source + 8), d = pvz::guest::Read(base, source + 12);
  pvz::guest::Write(base, ctx.r3.u32 + 8, c); pvz::guest::Write(base, ctx.r3.u32 + 12, d);
  pvz::guest::Write(base, ctx.r3.u32 + 4, b); pvz::guest::Write(base, ctx.r3.u32, a);
}
#ifndef PVZ_B001_SYNTHETIC_TEST
constexpr uint32_t ColorsOffset = offsetof(Widget, colors_header);
constexpr uint32_t AnimationOffset = offsetof(Widget, animation);
constexpr uint32_t QueueOffset = offsetof(Widget, animation_queue);

void ColorReference(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, index = ctx.r4.u32;
  Frame frame(ctx, base, 128);
  // Static default Color and its guard are guest globals, shared with original.
  const uint32_t guard = pvz::guest::Read(base, 0x828923C8);
  if (!(guard & 1)) {
    pvz::guest::Write(base, 0x828923C8, guard | 1);
    Call(ctx, base, sub_82491858, 0x82473638, {0x828923B8});
  }
  const auto& w = W(base, self);
  const int32_t count = std::bit_cast<int32_t>(uint32_t(w.colors_end) - uint32_t(w.colors_begin)) >> 4;
  if (int32_t(index) < count) {
    if (index >= uint32_t(count)) ppc_trap(ctx, base, 22);
    ctx.r3.u64 = uint32_t(w.colors_begin) + (index << 4);
  } else ctx.r3.u64 = 0x828923B8;
}
void SetColor(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, index = ctx.r4.u32, source = ctx.r5.u32;
  Frame frame(ctx, base, 128); auto& w = W(base, self);
  const int32_t count = std::bit_cast<int32_t>(uint32_t(w.colors_end) - uint32_t(w.colors_begin)) >> 4;
  if (int32_t(index) >= count)
    Call(ctx, base, sub_824745D0, 0x82474038, {self + ColorsOffset,index + 1});
  const uint32_t first = w.colors_begin, end = w.colors_end;
  if (index >= uint32_t(std::bit_cast<int32_t>(end - first) >> 4)) ppc_trap(ctx, base, 22);
  const uint32_t dest = first + (index << 4);
  // Preserve the interleaved Xbox copy for aliased source/destination.
  for (uint32_t i = 0; i < 16; i += 4) pvz::guest::Write(base, dest + i, pvz::guest::Read(base, source + i));
  Args(ctx, {self}); Virtual(ctx, base, self, 16, 0x82474090);
}
template<bool Alpha> void SetColors(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, source = ctx.r4.u32;
  const int32_t count = ctx.r5.s32; Frame frame(ctx, base, 128);
  Call(ctx, base, sub_824A5840, Alpha ? 0x82473E10 : 0x82473D88, {self + ColorsOffset});
  for (int32_t i = 0; i < count; ++i) {
    const uint32_t src = source + uint32_t(i) * (Alpha ? 16 : 12), temp = ctx.r1.u32 + 80;
    const uint32_t red = pvz::guest::Read(base,src), green = pvz::guest::Read(base,src+4), blue = pvz::guest::Read(base,src+8);
    // Constructor's fourth argument is only used by the RGBA overload.
    ctx.r7.u64 = Alpha ? pvz::guest::Read(base,src+12) : ctx.r7.u64;
    const uint32_t vtable = uint32_t(W(base,self).vtable);
    Call(ctx, base, Alpha ? sub_82491928 : sub_824918E0, Alpha ? 0x82473E3C : 0x82473DB0, {temp,red,green,blue});
    Args(ctx,{self,uint32_t(i),ctx.r3.u32});
    ctx.ctr.u64 = pvz::guest::Read(base,vtable+100); ctx.lr = Alpha ? 0x82473E54 : 0x82473DC8;
    REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
  }
  Args(ctx,{self}); Virtual(ctx,base,self,16,Alpha ? 0x82473E74 : 0x82473DE8);
}
void Animate(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32; Frame frame(ctx, base, 128); auto& w = W(base,self);
  if (!uint32_t(w.animation)) return;
  Args(ctx,{w.animation}); Virtual(ctx,base,w.animation,8,0x824739E8);
  Args(ctx,{w.animation}); Virtual(ctx,base,w.animation,12,0x824739FC);
  if (!ctx.r3.u8) return;
  Args(ctx,{w.animation}); Virtual(ctx,base,w.animation,16,0x82473A18);
  if (ctx.r3.u8) {
    w.visible=0; w.disabled=1;
    Call(ctx,base,sub_821B0F00,0x82473A70,{self+AnimationOffset,0});
    return;
  }
  else {
    Call(ctx,base,sub_821B0F00,0x82473A40,{self+AnimationOffset,0});
    if (!uint32_t(w.animation_queue[0])) return;
    const auto queued = uint32_t(w.animation_queue[0]);
    Args(ctx,{queued}); Virtual(ctx,base,queued,12,0x82473A5C);
    if (ctx.r3.u8) return;
  }
  Call(ctx,base,sub_821B0F00,0x82473A70,{self+AnimationOffset,uint32_t(w.animation_queue[0])});
}
void Destruct(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32; Frame frame(ctx, base, 128); auto& w = W(base,self);
  w.vtable = 0x8205D504; w.listener_vtable = 0x8205D4D0;
  Call(ctx,base,sub_824A5840,0x82473C9C,{self+ColorsOffset});
  Call(ctx,base,sub_825FB4E8,0x82473CB4,{self+QueueOffset,4,3,0x82474108});
  if (uint32_t(w.animation)) Call(ctx,base,sub_82199CE8,0x82473CCC,{w.animation});
  Call(ctx,base,sub_82564B78,0x82473CD4,{self+ColorsOffset});
  Call(ctx,base,sub_82492FD8,0x82473CDC,{self});
}
void Delete(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32, flags=ctx.r4.u32; Frame frame(ctx, base, 128);
  // Call the dispatched destructor, so tracing/other overrides remain honored.
  Call(ctx,base,sub_82473C60,0x824E3EC8,{self});
  if (flags & 1) Call(ctx,base,sub_825F8F50,0x824E3ED8,{self});
  ctx.r3.u64 = self;
}
void Construct(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32; Frame frame(ctx, base, 128);
  Call(ctx,base,sub_82492F28,0x82473EA4,{self});
  auto& w=W(base,self);
  w.listener_vtable=0x8205D484; w.vtable=0x8205D504; w.listener_vtable=0x8205D4D0;
  Call(ctx,base,sub_825557B0,0x82473ED8,{self+ColorsOffset});
  Call(ctx,base,sub_8254C4A0,0x82473EE0,{self+uint32_t(offsetof(Widget,mouse_insets))});
  w.animation=0;
  Call(ctx,base,sub_825FB2F8,0x82473F10,{self+QueueOffset,4,3,0x82474120,0x82474108});
  w.manager=0; w.visible=1; w.id=-1; w.disabled=0; w.is_down=0; w.is_over=0;
  w.flag_a0=0; w.flag_71=1; w.flag_73=0; w.flag_76=0; w.wants_focus=0;
  w.field_a4=0; w.field_a8=0; w.gamepad_focus=0; w.field_ac=0;
  w.field_b0=0; w.navigation_1=0; w.field_b8=0; w.field_bc=0; w.user_index=-1;
  ctx.r3.u64=self;
}
auto& Graphics(uint8_t* base, uint32_t address) {
  return *reinterpret_cast<pvz::xbox::WidgetGraphicsText*>(base+address);
}
void WriteString(PPCContext& ctx, uint8_t* base) {
  const uint32_t g=ctx.r4.u32, text=ctx.r5.u32, x=ctx.r6.u32, y=ctx.r7.u32,
      width=ctx.r8.u32, justification=ctx.r9.u32, draw=ctx.r10.u32;
  Frame frame(ctx, base, 128);
  const auto old=Graphics(base,g).write_colored_string;
  Graphics(base,g).write_colored_string=base[0x827D2720];
  pvz::guest::Write(base,ctx.r1.u32+84,pvz::guest::Read(base,uint32_t(frame.sp)+92));
  pvz::guest::Write(base,ctx.r1.u32+92,~0u);
  Call(ctx,base,sub_8249E418,0x82473130,{g,text,x,y,width,justification,draw,pvz::guest::Read(base,uint32_t(frame.sp)+84)});
  Graphics(base,g).write_colored_string=old;
}
void WordWrapped(PPCContext& ctx, uint8_t* base) {
  const uint32_t g=ctx.r4.u32, rect=ctx.r5.u32, text=ctx.r6.u32, spacing=ctx.r7.u32, justification=ctx.r8.u32;
  Frame frame(ctx, base, 128); const auto old=Graphics(base,g).write_colored_string;
  Graphics(base,g).write_colored_string=base[0x827D2720];
  base[ctx.r1.u32+87]=0;
  Call(ctx,base,sub_8249E900,0x8247319C,{g,rect,text,spacing,justification,0,~0u,0});
  Graphics(base,g).write_colored_string=old;
}
void WrappedHeight(PPCContext& ctx, uint8_t* base) {
  Args(ctx,{ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32,0,0});
  sub_8249EF58(ctx,base);
}
// PPC word arithmetic wraps; keep intermediate UI coordinates in uint32_t.
void NumberStrip(PPCContext& ctx, uint8_t* base) {
  const uint32_t g=ctx.r4.u32, number=ctx.r5.u32, x=ctx.r6.u32, y=ctx.r7.u32, image=ctx.r8.u32, spacing=ctx.r9.u32;
  Frame frame(ctx, base, 128); int32_t divisor=10; uint32_t digits=1;
  while (int32_t(number)>=divisor) { divisor=std::bit_cast<int32_t>(uint32_t(divisor)*10u); ++digits; }
  if (!number) divisor=10;
  Call(ctx,base,sub_824AC938,0x8247325C,{image});
  const uint32_t digit_width=uint32_t(ctx.r3.s32/10), advance=digit_width+spacing;
  uint32_t offset=0;
  for (uint32_t i=0;i<digits;++i) {
    Call(ctx,base,sub_8249DDB0,0x82473280,{g}); const uint32_t clip=ctx.r3.u32;
    divisor/=10;
    Call(ctx,base,sub_824AC940,0x8247328C,{image}); const auto height=ctx.r3.u32;
    Call(ctx,base,sub_8249C6A8,0x824732A4,{clip,offset+x,y,digit_width,height});
    // divw/tw guards in the Xbox body trap on zero divisor and INT_MIN/-1.
    if (!divisor || (number==0x80000000u && divisor==-1)) ppc_trap(ctx,base,22);
    const int32_t digit=(int32_t(number)/divisor)%10;
    Call(ctx,base,sub_8249D0E8,0x824732E4,{clip,image,offset+x-uint32_t(digit)*digit_width,y});
    if (clip) { Args(ctx,{clip,1}); Virtual(ctx,base,clip,0,0x82473304); }
    offset+=advance;
  }
}
void Centered(PPCContext& ctx, uint8_t* base) {
  const uint32_t out=ctx.r3.u32,self=ctx.r4.u32,g=ctx.r5.u32,y=ctx.r6.u32,text=ctx.r7.u32;
  Frame frame(ctx, base, 128);
  Call(ctx,base,sub_8249A0B0,0x8247375C,{g}); const uint32_t font=ctx.r3.u32;
  Args(ctx,{font,text}); Virtual(ctx,base,font,28,0x82473770); const uint32_t width=ctx.r3.u32;
  const uint32_t x=uint32_t(std::bit_cast<int32_t>(uint32_t(W(base,self).width)-width)/2) +
      uint32_t(REXCVAR_GET(pvz_native_widget_82473730_inject_error));
  Call(ctx,base,sub_8249B1A0,0x82473794,{g,text,x,y});
  Args(ctx,{font}); Virtual(ctx,base,font,16,0x824737A8); const uint32_t height=ctx.r3.u32;
  Args(ctx,{font}); Virtual(ctx,base,font,4,0x824737BC); const uint32_t ascent=ctx.r3.u32;
  pvz::guest::Write(base,out,x); pvz::guest::Write(base,out+8,width);
  pvz::guest::Write(base,out+4,y-ascent); pvz::guest::Write(base,out+12,height); ctx.r3.u64=out;
}
void CenteredShadow(PPCContext& ctx, uint8_t* base) {
  const uint32_t out=ctx.r3.u32,self=ctx.r4.u32,g=ctx.r5.u32,y=ctx.r6.u32,text=ctx.r7.u32;
  const uint64_t color1=ctx.r8.u64,color2=ctx.r9.u64,color3=ctx.r10.u64;
  Frame frame(ctx, base, 128);
  // Struct arguments are homed as big-endian 64-bit registers before calling
  // Graphics::SetColor. Shadow Point* is the tenth argument, entry SP+100.
  const uint32_t shadow=pvz::guest::Read(base,uint32_t(frame.sp)+100);
  auto store64=[&](uint32_t address,uint64_t value) {
    pvz::guest::Write(base,address,uint32_t(value>>32)); pvz::guest::Write(base,address+4,uint32_t(value));
  };
  store64(ctx.r1.u32+64,color1); store64(ctx.r1.u32+72,color2); store64(ctx.r1.u32+80,color3);
  for (uint32_t offset=88;offset<100;offset+=4)
    pvz::guest::Write(base,ctx.r1.u32+offset,pvz::guest::Read(base,uint32_t(frame.sp)+offset));
  Call(ctx,base,sub_8249A0B0,0x82473820,{g}); const uint32_t font=ctx.r3.u32;
  Args(ctx,{font,text}); Virtual(ctx,base,font,28,0x82473834); const uint32_t width=ctx.r3.u32;
  const uint32_t x=uint32_t(std::bit_cast<int32_t>(uint32_t(W(base,self).width)-width)/2);
  Call(ctx,base,sub_8249A0C0,0x82473850,{g,ctx.r1.u32+80});
  Call(ctx,base,sub_8249B1A0,0x82473884,{g,text,x+pvz::guest::Read(base,shadow),y+pvz::guest::Read(base,shadow+4)});
  Call(ctx,base,sub_8249A0C0,0x82473890,{g,ctx.r1.u32+64});
  Call(ctx,base,sub_8249B1A0,0x824738B0,{g,text,x,y});
  const int32_t dx=int32_t(pvz::guest::Read(base,shadow)),dy=int32_t(pvz::guest::Read(base,shadow+4));
  Args(ctx,{font}); Virtual(ctx,base,font,16,0x824738F4); const uint32_t height=ctx.r3.u32;
  Args(ctx,{font}); Virtual(ctx,base,font,4,0x8247392C); const uint32_t ascent=ctx.r3.u32;
  auto magnitude=[](int32_t n) {return n<0 ? 0u-uint32_t(n) : uint32_t(n);};
  pvz::guest::Write(base,out+8,width+magnitude(dx)); pvz::guest::Write(base,out+12,height+magnitude(dy));
  pvz::guest::Write(base,out+4,y+uint32_t(std::min(0,dy))-ascent);
  pvz::guest::Write(base,out,x+uint32_t(std::min(0,dx))); ctx.r3.u64=out;
}
#endif
} // namespace

// Batch eligibility follows measured coverage in native-replacements.csv.
// Individual flags remain available for unproven implementations.
#ifndef PVZ_B001_SYNTHETIC_TEST
#define LEAF(ADDRESS, PROVEN, BODY, OUTPUTS, RANGES, INPUTS) \
  PVZ_NATIVE_BATCH_REPLACEMENT(b001, PROVEN, widget_##ADDRESS, sub_##ADDRESS, BODY, \
      pvz::native::Safety::DeclaredOutputsOnly, OUTPUTS, RANGES)
#define SNAP(ADDRESS, PROVEN, BODY) \
  PVZ_NATIVE_BATCH_REPLACEMENT(b001, PROVEN, widget_##ADDRESS, sub_##ADDRESS, BODY, \
      pvz::native::Safety::SnapshotsOnly, noOutputs, NoRanges)
#endif
LEAF(82472958, true, NoOp, r3out, NoRanges, None)
LEAF(82472960, true, True, r3out, NoRanges, None)
SNAP(82472968, true, Visible)
LEAF(824729D8, true, NoOp, r3out, NoRanges, None)
LEAF(824729E0, true, NoOp, r3out, NoRanges, None)
SNAP(824729E8, true, Overlay)
SNAP(824729F8, true, Resize)
SNAP(82472A98, true, ResizeRect)
SNAP(82472AB8, true, Move)
LEAF(82472AD0, true, WantsFocus, r3out, NoRanges, FocusByte)
SNAP(82472AD8, true, Disabled)
SNAP(82472B98, true, FluentDisable)
LEAF(82472BD8, true, Focus<true>, noOutputs, FocusRange, None)
LEAF(82472BE8, true, Focus<false>, noOutputs, FocusRange, None)
LEAF(82472C00, true, NoOp, r3out, NoRanges, Identity)
LEAF(82472C08, true, NoOp, r3out, NoRanges, Identity)
SNAP(82472C10, false, KeyDown)
LEAF(82472C50, true, NoOp, r3out, NoRanges, Identity)
SNAP(82472C58, true, Enter)
LEAF(82472C88, true, Leave, noOutputs, LeaveRanges, None)
LEAF(82472CA0, true, Navigation, noOutputs, NavigationRange, None)
LEAF(82472CB8, true, Next, r3out, NoRanges, None)
LEAF(82472DB0, true, Proxy, noOutputs, ProxyRange, None)
SNAP(82472DB8, false, ProxyFocus)
SNAP(82472DE0, true, GamepadDown)
SNAP(82472F90, true, ProxyRelease)
SNAP(82472FB8, true, Finger)
LEAF(82472FE8, true, NoOp, r3out, NoRanges, None)
LEAF(82472FF0, true, NoOp, r3out, NoRanges, None)
LEAF(82472FF8, true, NoOp, r3out, NoRanges, None)
// 82473000 is already hooked in game_settings.cpp. Never register it twice.
LEAF(82473040, true, NoOp, r3out, NoRanges, Identity)
LEAF(82473048, true, NoOp, r3out, NoRanges, Identity)
SNAP(82473050, false, MouseUp)
LEAF(824730D0, true, NoOp, r3out, NoRanges, Identity)
LEAF(824730D8, true, NoOp, r3out, NoRanges, Identity)
LEAF(824730E0, true, NoOp, r3out, NoRanges, Identity)
SNAP(824730E8, false, WriteString)
SNAP(82473148, true, WordWrapped)
SNAP(824731B8, true, WrappedHeight)
LEAF(824731D8, false, Digits, r3out, NoRanges, Digits)
SNAP(82473200, false, NumberStrip)
LEAF(82473318, true, Contains, r3out, NoRanges, None)
LEAF(82473588, true, NoOp, r3out, NoRanges, Identity)
LEAF(82473590, true, NoOp, r3out, NoRanges, Identity)
LEAF(82473598, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735A0, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735A8, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735B0, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735B8, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735C0, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735C8, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735D0, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735D8, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735E0, true, NoOp, r3out, NoRanges, Identity)
LEAF(824735E8, true, NoOp, r3out, NoRanges, Identity)
SNAP(824735F8, true, ColorReference)
LEAF(824736B0, true, Color, noOutputs, RectRange, Color)
SNAP(82473730, true, Centered)
SNAP(824737E8, false, CenteredShadow)
LEAF(82473960, true, Insets, noOutputs, RectRange, None)
SNAP(824739B0, true, Animate)
SNAP(82473C60, true, Destruct)
SNAP(82473D68, true, SetColors<false>)
SNAP(82473DF0, false, SetColors<true>)
SNAP(82473E88, true, Construct)
SNAP(82473FF8, true, SetColor)
SNAP(824E3EA8, true, Delete)
#undef LEAF
#undef SNAP
