// B005, Xbox 584109FF 0.0.1.4. Behavior is derived from generated/default
// and .cache/pvz-image.bin; see docs/native-port/reports/T16.md.
#include "native/replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/ScrollbarWidget.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#include "native/guest_call.h"
PVZ_NATIVE_BATCH(b005)
REXCVAR_DECLARE(bool, pvz_native_scrollbar_824BA9D0_inject_error);
REXCVAR_DECLARE(bool, pvz_native_scrollbar_824BAB78_inject_error);
REX_EXTERN(sub_82472AD8);
REX_EXTERN(sub_82472BF8);
REX_EXTERN(sub_82473040);
REX_EXTERN(sub_824730D0);
REX_EXTERN(sub_824730D8);
REX_EXTERN(sub_82473C60);
REX_EXTERN(sub_82473D68);
REX_EXTERN(sub_82473E88);
REX_EXTERN(sub_824930D0);
REX_EXTERN(sub_82493558);
REX_EXTERN(sub_82493E10);
REX_EXTERN(sub_8249B718);
REX_EXTERN(sub_824B9A50);
REX_EXTERN(sub_82582798);
REX_EXTERN(sub_825DDB78);
REX_EXTERN(sub_825F8F50);
REX_EXTERN(sub_8249A0C0);
REX_EXTERN(sub_8249B650);
REX_EXTERN(sub_8249E3C0);
#endif

namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;

const std::array<RegisterOutput, 0> noOutputs{};
const std::array r3out{RegisterOutput{"r3", &PPCContext::r3, 64, pvz::native::Kind::Unsigned}};
const auto NoRanges = [](const PPCContext&) { return std::array<Range, 0>{}; };
const auto Field130Range = [](const PPCContext& input) {
  return std::array{
      Range{input.r3.u32 + uint32_t(offsetof(pvz::xbox::ScrollbarWidget, field_130)), sizeof(uint32_t)}};
};
const auto Field134Ranges = [](const PPCContext& input) {
  return std::array{
      Range{input.r3.u32 + uint32_t(offsetof(pvz::xbox::ScrollbarWidget, field_134)), sizeof(uint32_t)},
      Range{input.r3.u32 + uint32_t(offsetof(pvz::xbox::ScrollbarWidget, field_138)), sizeof(uint32_t)}};
};

void Store64(uint8_t* base, uint32_t address, uint64_t value) {
  pvz::guest::Write(base, address, uint32_t(value >> 32));
  pvz::guest::Write(base, address + 4, uint32_t(value));
}

double ReadDouble(uint8_t* base, uint32_t address) {
  const uint64_t bits =
      (uint64_t(pvz::guest::Read(base, address)) << 32) | pvz::guest::Read(base, address + 4);
  return std::bit_cast<double>(bits);
}

void StoreField130(PPCContext& ctx, uint8_t* base) {
  const auto address = ctx.r3.u32 + offsetof(pvz::xbox::ScrollbarWidget, field_130);
  pvz::guest::Write(base, address, ctx.r4.u32);
}

void StoreFields134And138(PPCContext& ctx, uint8_t* base) {
  const auto self = ctx.r3.u32;
  pvz::guest::Write(base, self + offsetof(pvz::xbox::ScrollbarWidget, field_134), ctx.r4.u32);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::ScrollbarWidget, field_138), ctx.r5.u32);
}

void AtBottom(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32;
  ctx.fpscr.disableFlushMode();
  const double remaining = ReadDouble(base, self + offsetof(pvz::xbox::ScrollbarWidget, max_value)) -
                           ReadDouble(base, self + offsetof(pvz::xbox::ScrollbarWidget, page_size)) -
                           ReadDouble(base, self + offsetof(pvz::xbox::ScrollbarWidget, value));
  const double tolerance = ReadDouble(base, 0x82008BD0);
  ctx.r3.u64 = (!std::isnan(remaining) && remaining <= tolerance) ? 1 : 0;
}

void TrackSize(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32;
  const int32_t selector =
      std::bit_cast<int32_t>(pvz::guest::Read(base, self + offsetof(pvz::xbox::ScrollbarWidget, field_104)));
  const bool horizontal = base[self + offsetof(pvz::xbox::ScrollbarWidget, horizontal)] != 0;
  uint32_t amount;
  if (selector > 0) {
    amount = uint32_t(selector) << 1;
  } else {
    const uint32_t up = pvz::guest::Read(base, self + offsetof(pvz::xbox::ScrollbarWidget, up_button));
    amount = pvz::guest::Read(base, up + offsetof(pvz::xbox::ScrollbuttonWidget, width)) << 1;
  }
  const uint32_t dimension = pvz::guest::Read(
      base, self + (horizontal ? offsetof(pvz::xbox::Widget, width) : offsetof(pvz::xbox::Widget, height)));
  ctx.r3.u64 = uint64_t(dimension) - uint64_t(amount);
}

void SetHorizontal(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32;
  const uint8_t horizontal = ctx.r4.u8;
  const uint32_t down = pvz::guest::Read(base, self + offsetof(pvz::xbox::ScrollbarWidget, down_button));
  base[self + offsetof(pvz::xbox::ScrollbarWidget, horizontal)] = horizontal;
  base[down + offsetof(pvz::xbox::ScrollbuttonWidget, horizontal)] = horizontal;
  const uint32_t up = pvz::guest::Read(base, self + offsetof(pvz::xbox::ScrollbarWidget, up_button));
  base[up + offsetof(pvz::xbox::ScrollbuttonWidget, horizontal)] =
      base[self + offsetof(pvz::xbox::ScrollbarWidget, horizontal)];
}

void SetButtonFields(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32;
  for (const auto offset :
       {offsetof(pvz::xbox::ScrollbarWidget, up_button), offsetof(pvz::xbox::ScrollbarWidget, down_button)}) {
    const uint32_t child = pvz::guest::Read(base, self + uint32_t(offset));
    pvz::guest::Write(base, child + offsetof(pvz::xbox::ScrollbuttonWidget, field_F8), ctx.r4.u32);
    const uint32_t child2 = pvz::guest::Read(base, self + uint32_t(offset));
    pvz::guest::Write(base, child2 + offsetof(pvz::xbox::ScrollbuttonWidget, field_100), ctx.r5.u32);
    const uint32_t child3 = pvz::guest::Read(base, self + uint32_t(offset));
    pvz::guest::Write(base, child3 + offsetof(pvz::xbox::ScrollbuttonWidget, field_104), ctx.r6.u32);
  }
}

#ifndef PVZ_BATCH_SYNTHETIC_TEST
void DrawThumb(PPCContext& ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32;
  const uint32_t graphics = ctx.r4.u32;
  const uint32_t x = ctx.r5.u32 + uint32_t(REXCVAR_GET(pvz_native_scrollbar_824BA9D0_inject_error)),
                 y = ctx.r6.u32;
  const uint32_t width = ctx.r7.u32, height = ctx.r8.u32;
  pvz::native::Frame frame(ctx, base, 0xC0);
  if (pvz::guest::Read(base, self + offsetof(pvz::xbox::ScrollbarWidget, field_130))) {
    const uint32_t rect = ctx.r1.u32 + 0x50;
    pvz::guest::Write(base, rect, x);
    pvz::guest::Write(base, rect + 4, y);
    pvz::guest::Write(base, rect + 8, width);
    pvz::guest::Write(base, rect + 12, height);
    pvz::native::Call(ctx, base, sub_8249E3C0, 0x824BAB6C, {graphics, rect});
    return;
  }
  auto set_color = [&](uint32_t index, uint32_t virtual_lr, uint32_t color_lr) {
    pvz::native::Args(ctx, {self, index});
    pvz::native::Virtual(ctx, base, self, 0x6C, virtual_lr);
    const uint32_t color = ctx.r3.u32;
    pvz::native::Call(ctx, base, sub_8249A0C0, color_lr, {graphics, color});
  };
  auto fill = [&](uint32_t fx, uint32_t fy, uint32_t fw, uint32_t fh, uint32_t lr) {
    pvz::native::Call(ctx, base, sub_8249B650, lr, {graphics, fx, fy, fw, fh});
  };

  set_color(2, 0x824BAA14, 0x824BAA20);
  fill(x, y, width, height, 0x824BAA38);
  set_color(5, 0x824BAA50, 0x824BAA5C);
  fill(x + 1, y + 1, width - 2, 1, 0x824BAA80);
  fill(x + 1, y + 1, 1, height - 2, 0x824BAA9C);
  set_color(3, 0x824BAAB4, 0x824BAAC0);
  fill(x, y + height - 1, width, 1, 0x824BAADC);
  fill(x + width - 1, y, 1, height, 0x824BAAF8);
  set_color(4, 0x824BAB10, 0x824BAB1C);
  fill(x + 1, y + height - 2, width - 2, 1, 0x824BAB34);
  fill(x + width - 2, y + 1, 1, height - 2, 0x824BAB4C);
}
#endif

void ReturnOnly(PPCContext&, uint8_t*) {}

#ifdef PVZ_BATCH_SYNTHETIC_TEST
const pvz::native::SyntheticInputs<PPCContext> Field130Inputs{
    "self=0x1000; r4 full-width boundary values and 1024 xorshift64 seed=42; store word at self+0x130", 9,
    1024, 42, [](size_t i, PPCContext& ctx, uint8_t*, uint64_t& seed) {
      ctx.r3.u64 = 0x1000;
      ctx.r4.u64 = i < std::size(Words) ? Words[i] : Random(seed);
    }};
const pvz::native::SyntheticInputs<PPCContext> Field134Inputs{
    "self=0x1000; r4/r5 full-width word boundaries and 1024 xorshift64 seed=42; stores +0x134/+0x138", 9,
    1024, 42, [](size_t i, PPCContext& ctx, uint8_t*, uint64_t& seed) {
      ctx.r3.u64 = 0x1000;
      ctx.r4.u64 = i < std::size(Words) ? Words[i] : Random(seed);
      ctx.r5.u64 = i < std::size(Words) ? Words[(i + 3) % std::size(Words)] : Random(seed);
    }};
const pvz::native::SyntheticInputs<PPCContext> AtBottomInputs{
    "self=0x1000; value/max/page each traverse all 14 IEEE-754 boundaries and 1024 seeded triples",
    14 * 14 * 14, 1024, 42, [](size_t i, PPCContext& ctx, uint8_t* base, uint64_t& seed) {
      constexpr size_t count = std::size(Floats);
      ctx.r3.u64 = 0x1000;
      const uint64_t max = i < count * count * count ? Floats[i % count] : Random(seed);
      const uint64_t page = i < count * count * count ? Floats[(i / count) % count] : Random(seed);
      const uint64_t value = i < count * count * count ? Floats[i / (count * count)] : Random(seed);
      Store64(base, 0x10F0, max);
      Store64(base, 0x10F8, page);
      Store64(base, 0x10E8, value);
    }};
const pvz::native::SyntheticInputs<PPCContext> TrackSizeInputs{
    "self=0x1000, up button=0x2000; dimensions/selector/axis boundary product and 1024 seeded cases",
    2 * 4 * 5 * 5 * 5, 1024, 42, [](size_t i, PPCContext& ctx, uint8_t* base, uint64_t& seed) {
      constexpr uint32_t values[]{0, 1, 8, 64, 0xFFFFFFFF};
      constexpr int32_t selectors[]{-1, 0, 1, 16};
      const size_t boundaries = 2 * 4 * 5 * 5 * 5;
      ctx.r3.u64 = 0x1000;
      pvz::guest::Write(base, 0x10D8, 0x2000);
      if (i < boundaries) {
        size_t n = i;
        const uint32_t child = values[n % 5];
        n /= 5;
        const uint32_t height = values[n % 5];
        n /= 5;
        const uint32_t width = values[n % 5];
        n /= 5;
        const int32_t selector = selectors[n % 4];
        n /= 4;
        const uint32_t horizontal = uint32_t(n % 2);
        pvz::guest::Write(base, 0x1048, width);
        pvz::guest::Write(base, 0x104C, height);
        pvz::guest::Write(base, 0x1104, uint32_t(selector));
        pvz::guest::Write(base, 0x2048, child);
        base[0x1100] = uint8_t(horizontal);
      } else {
        pvz::guest::Write(base, 0x1048, uint32_t(Random(seed)));
        pvz::guest::Write(base, 0x104C, uint32_t(Random(seed)));
        pvz::guest::Write(base, 0x1104, uint32_t(Random(seed)));
        pvz::guest::Write(base, 0x2048, uint32_t(Random(seed)));
        base[0x1100] = uint8_t(Random(seed));
      }
    }};
#endif

#ifndef PVZ_BATCH_SYNTHETIC_TEST
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
using pvz::native::Virtual;
auto& Scroll(uint8_t* b, uint32_t self) {
  return *reinterpret_cast<pvz::xbox::ScrollbarWidget*>(b + self);
}
void SetDouble(uint8_t* b, uint32_t at, double v) {
  Store64(b, at, std::bit_cast<uint64_t>(v));
}
int64_t TruncateWord(double v) {
  if (std::isnan(v))
    return int64_t(0x80000000u);
  if (v >= double(INT_MAX))
    return INT_MAX;
  if (v <= double(INT_MIN))
    return INT_MIN;
  return int32_t(v);
}
void Native_824B9A50(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  pvz::guest::Write(b, c.r1.u32 + 0x84, self);
  auto& s = Scroll(b, self);
  s.widget.vtable = 0x820661C4;
  s.widget.listener_vtable = 0x8206618C;
  s.button_listener_vtable = 0x8206616C;
  uint32_t child = s.up_button;
  if (child) {
    Call(c, b, sub_824930D0, 0x824B9AA4, {self, child});
    child = s.up_button;
    if (child) {
      Args(c, {child, 1});
      Virtual(c, b, child, 0, 0x824B9AC4);
    }
  }
  child = s.down_button;
  if (child) {
    Call(c, b, sub_824930D0, 0x824B9AD8, {self, child});
    child = s.down_button;
    if (child) {
      Args(c, {child, 1});
      Virtual(c, b, child, 0, 0x824B9AF8);
    }
  }
  Call(c, b, sub_82473C60, 0x824B9B00, {self});
}
void Native_824B9B40(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  const uint8_t value = c.r4.u8;
  Frame f(c, b, 0x60);
  auto& s = Scroll(b, self);
  s.invis_if_no_scroll = value;
  if (value) {
    Args(c, {self, 0});
    Virtual(c, b, self, 0x58, 0x824B9B74);
    uint32_t child = s.down_button;
    Args(c, {child, 0});
    Virtual(c, b, child, 0x58, 0x824B9B8C);
    child = s.up_button;
    Args(c, {child, 0});
    Virtual(c, b, child, 0x58, 0x824B9BA4);
  }
}
template <uint32_t Slot, uint32_t Lr1, uint32_t Lr2> void ButtonPair(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, a = c.r4.u32, d = c.r5.u32;
  Frame f(c, b, 0x70);
  uint32_t child = Scroll(b, self).up_button;
  Args(c, {child, a, d});
  Virtual(c, b, child, Slot, Lr1);
  child = Scroll(b, self).down_button;
  Args(c, {child, a, d});
  Virtual(c, b, child, Slot, Lr2);
}
void Native_824B9C28(PPCContext& c, uint8_t* b) {
  ButtonPair<0x60, 0x824B9C54, 0x824B9C70>(c, b);
}
void Native_824B9C78(PPCContext& c, uint8_t* b) {
  ButtonPair<0x5C, 0x824B9CA4, 0x824B9CC0>(c, b);
}
void Native_824B9CC8(PPCContext& c, uint8_t* b) {
  ButtonPair<0x64, 0x824B9CF4, 0x824B9D10>(c, b);
}
void Native_824B9D18(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, w = c.r6.u32, h = c.r7.u32;
  Frame f(c, b, 0x80);
  Virtual(c, b, self, 0x7C, 0x824B9D40);
  auto& s = Scroll(b, self);
  const int32_t selector = s.field_104;
  const bool horizontal = s.horizontal != 0;
  const uint32_t size = selector > 0 ? uint32_t(selector) : (horizontal ? h : w);
  uint32_t child = s.up_button;
  Args(c, {child, 0, 0, horizontal ? size : w, horizontal ? h : size});
  Virtual(c, b, child, 0x7C, horizontal ? 0x824B9D84 : 0x824B9DC8);
  child = s.down_button;
  Args(c, {child, horizontal ? w - size : 0, horizontal ? 0 : h - size, horizontal ? size : w,
           horizontal ? h : size});
  Virtual(c, b, child, 0x7C, 0x824B9DEC);
}
template <uint32_t Offset, uint32_t ClampLr, uint32_t DirtyLr> void RangeChanged(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Store64(b, self + Offset, c.f1.u64);
  Virtual(c, b, self, 0x168, ClampLr);
  Args(c, {self});
  Virtual(c, b, self, 0x10, DirtyLr);
}
void Native_824B9DF8(PPCContext& c, uint8_t* b) {
  RangeChanged<0xF0, 0x824B9E20, 0x824B9E34>(c, b);
}
void Native_824B9E48(PPCContext& c, uint8_t* b) {
  RangeChanged<0xF8, 0x824B9E70, 0x824B9E84>(c, b);
}
void Native_824B9E98(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Store64(b, self + 0xE8, c.f1.u64);
  Virtual(c, b, self, 0x168, 0x824B9EC0);
  auto& s = Scroll(b, self);
  c.f1.f64 = s.value;
  Args(c, {uint32_t(s.scroll_listener), uint32_t(s.widget.id)});
  Virtual(c, b, c.r3.u32, 0, 0x824B9EDC);
  Args(c, {self});
  Virtual(c, b, self, 0x10, 0x824B9EF0);
}
void Native_824B9F40(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  auto& s = Scroll(b, self);
  c.fpscr.disableFlushMode();
  s.value = double(s.max_value) - double(s.page_size);
  Virtual(c, b, self, 0x168, 0x824B9F74);
  Args(c, {self});
  c.f1.f64 = s.value;
  Virtual(c, b, self, 0x128, 0x824B9F8C);
}
void Native_824B9FA0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, rect = c.r5.u32;
  c.r8.u64 = pvz::guest::Read(b, rect + 12);
  c.r7.u64 = pvz::guest::Read(b, rect + 8);
  c.r6.u64 = pvz::guest::Read(b, rect + 4);
  c.r5.u64 = pvz::guest::Read(b, rect);
  Virtual(c, b, self, 0x158, uint32_t(c.lr));
}
void Native_824BA008(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  c.fpscr.disableFlushMode();
  auto& s = Scroll(b, self);
  if (double(s.page_size) > double(s.max_value)) {
    c.r3.u64 = 0;
    return;
  }
  Virtual(c, b, self, 0x15C, 0x824BA048);
  const int64_t track = c.r3.s32;
  Store64(b, c.r1.u32 + 0x50, uint64_t(track));
  const double scaled = (double(track) * double(s.page_size)) / double(s.max_value);
  const int64_t result = TruncateWord(scaled + ReadDouble(b, 0x82001010));
  Store64(b, c.r1.u32 + 0x50, uint64_t(result));
  c.r3.u64 = int32_t(uint32_t(result)) < 8 ? 8 : uint32_t(result);
}
void Native_824BA0A8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  c.fpscr.disableFlushMode();
  auto& s = Scroll(b, self);
  if (double(s.page_size) > double(s.max_value)) {
    c.r3.u64 = 0;
    return;
  }
  Virtual(c, b, self, 0x160, 0x824BA0EC);
  const uint64_t thumb = c.r3.u64;
  Args(c, {self});
  Virtual(c, b, self, 0x15C, 0x824BA104);
  const int64_t span = int32_t(uint32_t(c.r3.u64 - thumb));
  Store64(b, c.r1.u32 + 0x50, uint64_t(span));
  const double denominator = double(s.max_value) - double(s.page_size);
  const double scaled = (double(span) * double(s.value)) / denominator;
  const int64_t result = TruncateWord(scaled + ReadDouble(b, 0x82001010));
  Store64(b, c.r1.u32 + 0x50, uint64_t(result));
  c.r3.u64 = uint32_t(result);
}
void Native_824BA160(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x80);
  c.fpscr.disableFlushMode();
  auto& s = Scroll(b, self);
  const double max = s.max_value, page = s.page_size, old = s.value;
  const double upper = max - page;
  if (double(s.value) > upper)
    s.value = upper;
  const double zero = ReadDouble(b, 0x82009A20);
  if (double(s.value) < zero)
    s.value = zero;
  const uint32_t scrollable = page < max ? 1 : 0, disabled = scrollable ? 0 : 1;
  Args(c, {self, disabled});
  Virtual(c, b, self, 0x70, 0x824BA1E0);
  uint32_t child = s.up_button;
  Args(c, {child, disabled});
  Virtual(c, b, child, 0x70, 0x824BA1F8);
  child = s.down_button;
  Args(c, {child, disabled});
  Virtual(c, b, child, 0x70, 0x824BA210);
  if (s.invis_if_no_scroll) {
    Args(c, {self, scrollable});
    Virtual(c, b, self, 0x58, 0x824BA234);
    child = s.down_button;
    Args(c, {child, scrollable});
    Virtual(c, b, child, 0x58, 0x824BA24C);
    child = s.up_button;
    Args(c, {child, scrollable});
    Virtual(c, b, child, 0x58, 0x824BA264);
  }
  c.f1.f64 = s.value;
  if (c.f1.f64 != old) {
    Args(c, {uint32_t(s.scroll_listener), uint32_t(s.widget.id)});
    Virtual(c, b, c.r3.u32, 0, 0x824BA288);
  }
}
void Native_824BA298(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x80);
  c.fpscr.disableFlushMode();
  auto& s = Scroll(b, self);
  const int64_t position = c.r4.s32;
  Store64(b, c.r1.u32 + 0x50, uint64_t(position));
  const double numerator = (double(s.max_value) - double(s.page_size)) * double(position);
  // Xbox keeps this vtable across the two callbacks.
  const uint32_t vtable = pvz::guest::Read(b, self);
  Virtual(c, b, self, 0x160, 0x824BA2DC);
  const uint64_t thumb = c.r3.u64;
  Args(c, {self});
  Virtual(c, b, self, 0x15C, 0x824BA2F4);
  const int64_t span = int32_t(uint32_t(c.r3.u64 - thumb));
  Store64(b, c.r1.u32 + 0x50, uint64_t(span));
  c.f1.f64 = numerator / double(span);
  Args(c, {self});
  const uint32_t target = pvz::guest::Read(b, vtable + 0x128);
  Call(c, b,
       [target](PPCContext& ctx, uint8_t* base) {
         ctx.ctr.u64 = target;
         REX_CALL_INDIRECT_FUNC(target);
       },
       0x824BA31C, {});
}
void Native_824BA328(PPCContext& c, uint8_t* b) {
  const uint32_t listener = c.r3.u32, self = listener - 0xD4;
  pvz::guest::Write(b, listener + 0x4C, 0);
  c.fpscr.disableFlushMode();
  const double value = ReadDouble(b, listener + 0x14), step = ReadDouble(b, 0x82008BD0);
  c.f1.f64 = c.r4.s32 == 0 ? value - step : value + step;
  Args(c, {self});
  Virtual(c, b, self, 0x128, uint32_t(c.lr));
}
void Native_824BA370(PPCContext& c, uint8_t* b) {
  const uint32_t listener = c.r3.u32;
  const bool down = c.r4.s32 != 0;
  Frame f(c, b, 0x60);
  const uint32_t count = pvz::guest::Read(b, listener + 0x4C) + 1;
  pvz::guest::Write(b, listener + 0x4C, count);
  if (int32_t(count) >= 25) {
    c.fpscr.disableFlushMode();
    const double value = ReadDouble(b, listener + 0x14), step = ReadDouble(b, 0x82008BD0);
    c.f1.f64 = down ? value + step : value - step;
    Args(c, {listener - 0xD4});
    Virtual(c, b, listener - 0xD4, 0x128, 0x824BA3E0);
    pvz::guest::Write(b, listener + 0x4C, 24);
  }
}
void Native_824BA400(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_82472BF8, 0x824BA418, {self});
  auto& s = Scroll(b, self);
  const int32_t mode = s.update_mode;
  if (mode != 1 && mode != 2)
    return;
  Args(c, {self, uint32_t(s.last_mouse_x), uint32_t(s.last_mouse_y)});
  Virtual(c, b, self, 0x170, mode == 1 ? 0x824BA490 : 0x824BA448);
  if (c.r3.s32 != (mode == 1 ? -1 : 1)) {
    s.update_mode = 0;
    Args(c, {self});
    Virtual(c, b, self, 0x10, 0x824BA4B4);
    return;
  }
  const uint32_t count = uint32_t(s.update_acc) + 1;
  s.update_acc = count;
  if (int32_t(count) < 25)
    return;
  c.fpscr.disableFlushMode();
  c.f1.f64 = mode == 1 ? double(s.value) - double(s.page_size) : double(s.page_size) + double(s.value);
  Args(c, {self});
  Virtual(c, b, self, 0x128, 0x824BA4EC);
  s.update_acc = 20;
}
void Native_824BA508(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x70);
  auto& s = Scroll(b, self);
  const bool horizontal = s.horizontal != 0;
  Virtual(c, b, self, 0x15C, horizontal ? 0x824BA53C : 0x824BA558);
  const uint32_t dimension = horizontal ? uint32_t(s.widget.width) : uint32_t(s.widget.height);
  const int64_t margin = int64_t(int32_t(uint32_t(uint64_t(dimension) - c.r3.u64))) / 2;
  const uint32_t position = (horizontal ? x : y) - uint32_t(margin);
  Args(c, {self});
  Virtual(c, b, self, 0x164, 0x824BA580);
  if (int32_t(position) < c.r3.s32) {
    c.r3.s64 = -1;
    return;
  }
  Args(c, {self});
  Virtual(c, b, self, 0x164, 0x824BA5A4);
  const uint64_t start = c.r3.u64;
  Args(c, {self});
  Virtual(c, b, self, 0x160, 0x824BA5BC);
  c.r3.u64 = int32_t(position) >= int32_t(uint32_t(start + c.r3.u64)) ? 1 : 0;
}
void Native_824BA5D8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82473040, 0x824BA5F4, {self, x, y});
  auto& s = Scroll(b, self);
  if (!s.widget.disabled) {
    Args(c, {self, x, y});
    Virtual(c, b, self, 0x170, 0x824BA61C);
    const int32_t hit = c.r3.s32;
    if (hit == 0) {
      s.pressed_on_thumb = 1;
      Args(c, {self});
      Virtual(c, b, self, 0x164, 0x824BA678);
      s.thumb_position = c.r3.u32;
      s.mouse_down_x = x;
      s.mouse_down_y = y;
    } else if (hit == 1 || hit == -1) {
      c.fpscr.disableFlushMode();
      c.f1.f64 = hit == 1 ? double(s.page_size) + double(s.value) : double(s.value) - double(s.page_size);
      Args(c, {self});
      Virtual(c, b, self, 0x128, hit == 1 ? 0x824BA654 : 0x824BA6A8);
      s.update_mode = hit == 1 ? 2 : 1;
      s.update_acc = 0;
    }
  }
  s.last_mouse_x = x;
  s.last_mouse_y = y;
}
void Native_824BA6C8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_824730D0, 0x824BA6E0, {});
  auto& s = Scroll(b, self);
  s.update_mode = 0;
  s.pressed_on_thumb = 0;
  Args(c, {self});
  Virtual(c, b, self, 0x10, 0x824BA700);
}
void Native_824BA718(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, x = c.r4.u32, y = c.r5.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824730D8, 0x824BA734, {self, x, y});
  auto& s = Scroll(b, self);
  if (s.pressed_on_thumb) {
    const uint32_t position = uint32_t(s.thumb_position) +
                              (s.horizontal ? x - uint32_t(s.mouse_down_x) : y - uint32_t(s.mouse_down_y));
    Args(c, {self, position});
    Virtual(c, b, self, 0x16C, 0x824BA780);
  }
  s.last_mouse_x = x;
  s.last_mouse_y = y;
}
void Native_824BA790(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flag = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_82493558, 0x824BA7B0, {self, flag});
  if (uint8_t(flag)) {
    Scroll(b, self).up_button = 0;
    Scroll(b, self).down_button = 0;
  }
}
void Native_824BA7E8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, id = c.r4.u32, listener = c.r5.u32;
  Frame f(c, b, 0x90);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xA4, self);
  Call(c, b, sub_82473E88, 0x824BA80C, {self});
  auto& s = Scroll(b, self);
  s.button_listener_vtable = 0x82060CD8;
  s.widget.id = id;
  s.scroll_listener = listener;
  s.widget.vtable = 0x820661C4;
  s.widget.listener_vtable = 0x8206618C;
  s.button_listener_vtable = 0x8206616C;
  pvz::guest::Write(b, sp + 0x50, self + 0xD4);
  Call(c, b, sub_82472AD8, 0x824BA858, {self, 1});
  Call(c, b, sub_825DDB78, 0x824BA860, {0x178});
  pvz::guest::Write(b, sp + 0x50, c.r3.u32);
  if (c.r3.u32)
    Call(c, b, sub_82582798, 0x824BA880, {c.r3.u32, 0, self + 0xD4, 0});
  else
    c.r3.u64 = 0;
  s.up_button = c.r3.u32;
  Args(c, {c.r3.u32, 1});
  Virtual(c, b, c.r3.u32, 0x70, 0x824BA8A0);
  Call(c, b, sub_825DDB78, 0x824BA8A8, {0x178});
  pvz::guest::Write(b, sp + 0x50, c.r3.u32);
  if (c.r3.u32)
    Call(c, b, sub_82582798, 0x824BA8C4, {c.r3.u32, 1, self + 0xD4, 0});
  else
    c.r3.u64 = 0;
  s.down_button = c.r3.u32;
  Args(c, {c.r3.u32, 1});
  Virtual(c, b, c.r3.u32, 0x70, 0x824BA8E4);
  s.invis_if_no_scroll = 0;
  s.pressed_on_thumb = 0;
  s.update_acc = 0;
  s.button_acc = 0;
  s.update_mode = 0;
  s.horizontal = 0;
  const double zero = ReadDouble(b, 0x82009A20);
  s.value = zero;
  s.max_value = zero;
  s.page_size = zero;
  s.field_130 = 0;
  s.field_134 = 0;
  s.field_138 = 0;
  s.field_104 = 0;
  Call(c, b, sub_82493E10, 0x824BA92C, {self, uint32_t(s.up_button)});
  Call(c, b, sub_82493E10, 0x824BA938, {self, uint32_t(s.down_button)});
  Call(c, b, sub_82473D68, 0x824BA94C, {self, 0x827D33E0, 6});
  c.r3.u64 = self;
}
void RectWords(uint8_t* b, uint32_t at, std::array<uint32_t, 4> v) {
  for (uint32_t i = 0; i < 4; ++i)
    pvz::guest::Write(b, at + i * 4, v[i]);
}
void Native_824BAB78(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, graphics = c.r4.u32;
  Frame f(c, b, 0xF0);
  const uint32_t sp = c.r1.u32;
  auto& s = Scroll(b, self);
  Virtual(c, b, self, 0x160, 0x824BAB9C);
  const uint32_t thumb = c.r3.u32;
  const bool horizontal = s.horizontal != 0;
  Args(c, {self});
  Virtual(c, b, self, 0x15C, horizontal ? 0x824BABC0 : 0x824BABCC);
  const uint32_t dimension = horizontal ? uint32_t(s.widget.width) : uint32_t(s.widget.height);
  const uint32_t margin = uint32_t(int64_t(int32_t(uint32_t(uint64_t(dimension) - c.r3.u64))) / 2);
  Args(c, {self});
  Virtual(c, b, self, 0x164, 0x824BABF0);
  const uint32_t position = c.r3.u32;
  RectWords(b, sp + 0x50, {0, 0, 0, 0});
  const uint32_t half = uint32_t(int64_t(int32_t(thumb)) / 2);
  std::array<uint32_t, 4> before, after, handle;
  if (s.horizontal) {
    const uint32_t height = s.widget.height;
    before = {margin, 0, half + position, height};
    RectWords(b, sp + 0x60, before);
    Args(c, {self});
    Virtual(c, b, self, 0x15C, 0x824BAC7C);
    after = {half + position + margin, 0, c.r3.u32 - half - position, height};
    handle = {position + margin, 0, thumb, uint32_t(s.widget.height)};
  } else {
    before = {0, margin, uint32_t(s.widget.width), half + position};
    RectWords(b, sp + 0x60, before);
    Args(c, {self});
    Virtual(c, b, self, 0x15C, 0x824BAD4C);
    after = {0, half + position + margin, uint32_t(s.widget.width), c.r3.u32 - half - position};
    handle = {0, position + margin, uint32_t(s.widget.width), thumb};
  }
  RectWords(b, sp + 0x50, after);
  RectWords(b, sp + 0x70, handle);
  if (REXCVAR_GET(pvz_native_scrollbar_824BAB78_inject_error))
    pvz::guest::Write(b, sp + 0x60, pvz::guest::Read(b, sp + 0x60) + 1);
  auto draw = [&](bool first) {
    const uint32_t rect = sp + (first ? 0x60 : 0x50);
    const bool active = int32_t(s.update_mode) == (first ? 1 : 2);
    const uint32_t image = active ? uint32_t(s.field_138) : uint32_t(s.field_134);
    if (image) {
      Call(c, b, sub_8249E3C0, first ? 0x824BAE08 : 0x824BAE74, {graphics, rect, image});
      return;
    }
    Args(c, {self, active ? 1u : 0u});
    Virtual(c, b, self, 0x6C, first ? 0x824BAE24 : 0x824BAE90);
    Call(c, b, sub_8249A0C0, first ? 0x824BAE30 : 0x824BAE9C, {graphics, c.r3.u32});
    Call(c, b, sub_8249B718, first ? 0x824BAE3C : 0x824BAEA8, {graphics, rect});
  };
  draw(true);
  draw(false);
  if (int32_t(thumb) > 0) {
    for (uint32_t i = 0; i < 4; ++i)
      pvz::guest::Write(b, sp + 0x90 + i * 4, pvz::guest::Read(b, sp + 0x70 + i * 4));
    Args(c, {self, graphics, sp + 0x90});
    Virtual(c, b, self, 0x154, 0x824BAEEC);
  }
}
void Native_824BAEF8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_824B9A50, 0x824BAF18, {self});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x824BAF28, {self});
  c.r3.u64 = self;
}
#endif
} // namespace

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#define LEAF(A, P, NATIVE, OUTPUTS, RANGES, INPUTS)                                                          \
  PVZ_NATIVE_BATCH_REPLACEMENT(b005, P, scrollbar_##A, sub_##A, NATIVE,                                      \
                               pvz::native::Safety::DeclaredOutputsOnly, OUTPUTS, RANGES)
#define SNAP(A, P, NATIVE)                                                                                   \
  PVZ_NATIVE_BATCH_REPLACEMENT(b005, P, scrollbar_##A, sub_##A, NATIVE, pvz::native::Safety::SnapshotsOnly,  \
                               noOutputs, NoRanges)
#endif

// Pure or closed-memory leaves: complete Xbox instruction translations above.
LEAF(824B9C10, true, StoreField130, noOutputs, Field130Range, Field130)
LEAF(824B9C18, true, StoreFields134And138, noOutputs, Field134Ranges, Field134)
LEAF(824B9F08, true, AtBottom, r3out, NoRanges, AtBottom)
LEAF(824B9FC0, true, TrackSize, r3out, NoRanges, TrackSize)
LEAF(824BA368, true, ReturnOnly, r3out, NoRanges, Identity)

#ifndef PVZ_BATCH_SYNTHETIC_TEST
// All members have native implementations; gates follow measured discovery and proof.
#endif
SNAP(824B9A50, true, Native_824B9A50)
SNAP(824B9B40, true, Native_824B9B40)
SNAP(824B9BB8, true, SetHorizontal)
SNAP(824B9BD8, true, SetButtonFields)
SNAP(824B9C28, false, Native_824B9C28)
SNAP(824B9C78, false, Native_824B9C78)
SNAP(824B9CC8, false, Native_824B9CC8)
SNAP(824B9D18, false, Native_824B9D18)
SNAP(824B9DF8, true, Native_824B9DF8)
SNAP(824B9E48, true, Native_824B9E48)
SNAP(824B9E98, true, Native_824B9E98)
SNAP(824B9F40, false, Native_824B9F40)
SNAP(824B9FA0, true, Native_824B9FA0)
SNAP(824BA008, true, Native_824BA008)
SNAP(824BA0A8, true, Native_824BA0A8)
SNAP(824BA160, false, Native_824BA160)
SNAP(824BA298, true, Native_824BA298)
SNAP(824BA328, false, Native_824BA328)
SNAP(824BA370, false, Native_824BA370)
SNAP(824BA400, true, Native_824BA400)
SNAP(824BA508, false, Native_824BA508)
SNAP(824BA5D8, false, Native_824BA5D8)
SNAP(824BA6C8, false, Native_824BA6C8)
SNAP(824BA718, false, Native_824BA718)
SNAP(824BA790, false, Native_824BA790)
SNAP(824BA7E8, true, Native_824BA7E8)
SNAP(824BA9D0, true, DrawThumb)
SNAP(824BAB78, true, Native_824BAB78)
SNAP(824BAEF8, false, Native_824BAEF8)

#undef LEAF
#undef SNAP
