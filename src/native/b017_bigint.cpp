// B017. The empty constructor has declared outputs; the allocating 32-bit
// constructor is translated and verified against snapshots.
// Allocating arithmetic, destruction and RNG execute once (SnapshotsOnly).
#include "native/replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/BigInt.h"
#include <array>
#include <bit>

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#include "native/guest_call.h"
PVZ_NATIVE_BATCH(b017)
REX_EXTERN(sub_82583AC8);
REX_EXTERN(sub_825840A0);
REX_EXTERN(sub_82584188);
REX_EXTERN(sub_825843E0);
REX_EXTERN(sub_825853E0);
REX_EXTERN(sub_825855A8);
REX_EXTERN(sub_82585998);
REX_EXTERN(sub_825F8F50);
REX_EXTERN(sub_825F9538);
REX_EXTERN(sub_825F9840);
REX_EXTERN(sub_825DDD38);
#endif

namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 0> noOutputs{};
const auto NoRanges = [](const PPCContext&) { return std::array<Range, 0>{}; };
const auto EmptyRanges = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32, sizeof(pvz::xbox::BigInt)}};
};

void BigIntEmpty(PPCContext& c, uint8_t* base) {
  const uint32_t self = c.r3.u32;
  base[self + offsetof(pvz::xbox::BigInt, negative)] = 0;
  pvz::guest::Write(base, self, 0x8207EB04);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::BigInt, limbs), 0);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::BigInt, count), 0);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::BigInt, capacity), 0);
}

#ifdef PVZ_BATCH_SYNTHETIC_TEST
const pvz::native::SyntheticInputs<PPCContext> EmptyInputs{
    "nine aligned guest addresses including the committed-buffer boundary; varying old fields and padding; "
    "1024 seeded cases",
    9, 1024, 42, [](size_t i, PPCContext& c, uint8_t* base, uint64_t& seed) {
      constexpr uint32_t addresses[]{0x1000, 0x1004, 0x1010, 0x1100, 0x1FF0, 0x3FF0, 0x7000, 0x7FD0, 0x7FE0};
      const uint32_t self = addresses[(i < 9 ? i : Random(seed)) % std::size(addresses)];
      c.r3.u64 = self;
      for (uint32_t j = 0; j < 32; ++j)
        base[self + j] = uint8_t(i < 9 ? Words[(i + j) % std::size(Words)] : Random(seed));
    }};
#endif

#ifndef PVZ_BATCH_SYNTHETIC_TEST
uint16_t Read16(uint8_t* base, uint32_t address) {
  return uint16_t((uint16_t(base[address]) << 8) | base[address + 1]);
}
void Write16(uint8_t* base, uint32_t address, uint16_t value) {
  base[address] = uint8_t(value >> 8);
  base[address + 1] = uint8_t(value);
}
uint64_t Read64(uint8_t* base, uint32_t address) {
  return (uint64_t(pvz::guest::Read(base, address)) << 32) | pvz::guest::Read(base, address + 4);
}
void Write64(uint8_t* base, uint32_t address, uint64_t value) {
  pvz::guest::Write(base, address, uint32_t(value >> 32));
  pvz::guest::Write(base, address + 4, uint32_t(value));
}
void BigIntFromInt32(PPCContext& c, uint8_t* base) {
  const uint64_t self_full = c.r3.u64;
  const uint32_t self = c.r3.u32;
  const int32_t input = std::bit_cast<int32_t>(c.r4.u32);
  const uint32_t entry_sp = c.r1.u32;
  pvz::guest::Write(base, entry_sp - 8, uint32_t(c.lr));
  Write64(base, entry_sp - 0x18, c.r30.u64);
  Write64(base, entry_sp - 0x10, c.r31.u64);
  pvz::native::Frame frame(c, base, 0x70);
  c.r31.u64 = self_full;
  c.r30.u64 = c.r4.u64;
  pvz::guest::Write(base, self, 0x8207EB04);
  pvz::native::Call(c, base, sub_825DDD38, 0x825841B8, {0x10});
  const uint32_t limbs = c.r3.u32;
  pvz::guest::Write(base, self + offsetof(pvz::xbox::BigInt, limbs), limbs);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::BigInt, capacity), 8);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::BigInt, count), 2);

  const bool negative = input < 0;
  const uint64_t magnitude = negative ? uint64_t(-int64_t(input)) : uint64_t(input);
  base[self + offsetof(pvz::xbox::BigInt, negative)] = negative ? 1 : 0;
  Write16(base, limbs, uint16_t(magnitude));
  Write16(base, limbs + 2, uint16_t(magnitude >> 16));
  for (int32_t count = 2; count >= 1;) {
    if (Read16(base, limbs + uint32_t(count) * 2 - 2) != 0)
      break;
    --count;
    pvz::guest::Write(base, self + offsetof(pvz::xbox::BigInt, count), uint32_t(count));
  }
  c.r3.u64 = self_full;
  c.r30.u64 = Read64(base, entry_sp - 0x18);
  c.r31.u64 = Read64(base, entry_sp - 0x10);
}
#endif

#ifndef PVZ_BATCH_SYNTHETIC_TEST
using pvz::native::Args;
using pvz::native::Call;
using pvz::native::Frame;
auto& Big(uint8_t* b, uint32_t self) {
  return *reinterpret_cast<pvz::xbox::BigInt*>(b + self);
}
void InitBig(uint8_t* b, uint32_t self, bool sign = true) {
  auto& v = Big(b, self);
  v.vtable = 0x8207EB04;
  if (sign)
    v.negative = 0;
  v.limbs = 0;
  v.count = 0;
  v.capacity = 0;
}
void NormalizeBig(uint8_t* b, uint32_t self) {
  auto& v = Big(b, self);
  const uint32_t limbs = v.limbs;
  while (int32_t(uint32_t(v.count)) >= 1) {
    const uint32_t count = v.count;
    if (Read16(b, limbs + count * 2 - 2))
      break;
    v.count = count - 1;
  }
}
void Native_82583918(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, limbs = Big(b, self).limbs;
  Big(b, self).vtable = 0x8207EB04;
  Call(c, b, sub_825F9538, uint32_t(c.lr), {limbs});
}
void Native_82584250(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64, input = c.r4.u64;
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  auto& v = Big(b, self);
  v.vtable = 0x8207EB04;
  Call(c, b, sub_825DDD38, 0x82584280, {16});
  const uint32_t limbs = c.r3.u32;
  v.limbs = limbs;
  v.capacity = 8;
  v.count = 4;
  const bool negative = std::bit_cast<int64_t>(input) < 0;
  const uint64_t magnitude = negative ? uint64_t(0) - input : input;
  v.negative = negative ? 1 : 0;
  Write16(b, limbs, uint16_t(magnitude));
  Write16(b, uint32_t(v.limbs) + 2, uint16_t(magnitude >> 16));
  Write16(b, uint32_t(v.limbs) + 4, uint16_t(magnitude >> 32));
  Write16(b, uint32_t(v.limbs) + 6, uint16_t(magnitude >> 48));
  NormalizeBig(b, self);
  c.r3.u64 = selfFull;
}
void Native_82584330(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64;
  const uint32_t self = c.r3.u32, source = c.r4.u32;
  Frame f(c, b, 0x60);
  InitBig(b, self, false);
  Call(c, b, sub_825840A0, 0x82584364, {self, source});
  c.r3.u64 = selfFull;
}
template <bool Remainder> void DivideResult(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64;
  const uint32_t self = c.r3.u32, a = c.r4.u32, d = c.r5.u32;
  Frame f(c, b, 0x90);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xA4, self);
  pvz::guest::Write(b, sp + 0x50, 0);
  if constexpr (Remainder) {
    InitBig(b, sp + 0x60);
    InitBig(b, self);
  } else {
    InitBig(b, self);
    InitBig(b, sp + 0x60);
  }
  pvz::guest::Write(b, sp + 0x50, 1);
  Call(c, b, sub_825843E0, Remainder ? 0x82584BF4 : 0x82584AEC,
       {a, d, Remainder ? sp + 0x60 : self, Remainder ? self : sp + 0x60});
  Call(c, b, sub_825F9538, Remainder ? 0x82584BFC : 0x82584AF4, {uint32_t(Big(b, sp + 0x60).limbs)});
  c.r3.u64 = selfFull;
}
void Native_82584A78(PPCContext& c, uint8_t* b) {
  DivideResult<false>(c, b);
}
void Native_82584B80(PPCContext& c, uint8_t* b) {
  DivideResult<true>(c, b);
}
template <bool And> void BitwiseResult(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64;
  const uint32_t self = c.r3.u32, a = c.r4.u32, d = c.r5.u32;
  Frame f(c, b, 0xA0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xB4, self);
  pvz::guest::Write(b, sp + 0x50, 0);
  InitBig(b, self);
  const uint32_t aCount = Big(b, a).count, dCount = Big(b, d).count;
  pvz::guest::Write(b, sp + 0x50, 1);
  pvz::guest::Write(b, sp + 0x54, aCount);
  pvz::guest::Write(b, sp + 0x54, dCount);
  const uint32_t count = int32_t(aCount) > int32_t(dCount) ? aCount : dCount;
  for (uint32_t i = 0; int32_t(i) < int32_t(count); ++i) {
    const uint16_t x =
        int32_t(i) < int32_t(uint32_t(Big(b, a).count)) ? Read16(b, uint32_t(Big(b, a).limbs) + i * 2) : 0;
    const uint16_t y =
        int32_t(i) < int32_t(uint32_t(Big(b, d).count)) ? Read16(b, uint32_t(Big(b, d).limbs) + i * 2) : 0;
    Call(c, b, sub_82583AC8, And ? 0x82584FA4 : 0x82584E74,
         {self, i, And ? uint32_t(x & y) : uint32_t(x | y)});
  }
  if constexpr (And)
    NormalizeBig(b, self);
  c.r3.u64 = selfFull;
}
void Native_82584DA8(PPCContext& c, uint8_t* b) {
  BitwiseResult<false>(c, b);
}
void Native_82584ED8(PPCContext& c, uint8_t* b) {
  BitwiseResult<true>(c, b);
}
void Native_82585860(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64;
  const uint32_t self = c.r3.u32, string = c.r4.u32;
  Frame f(c, b, 0xC0);
  const uint32_t sp = c.r1.u32;
  // The Xbox string constructor does not initialize the sign byte here.
  InitBig(b, self, false);
  uint32_t length = pvz::guest::Read(b, string + 0x14);
  pvz::guest::Write(b, sp + 0x50, length);
  if (int32_t(length) > 0)
    for (uint32_t i = 0;; ++i) {
      if (i > length)
        ppc_trap(c, b, 22);
      const uint32_t chars =
          pvz::guest::Read(b, string + 0x18) >= 16 ? pvz::guest::Read(b, string + 4) : string + 4;
      const int32_t ch = int8_t(b[chars + i]);
      uint32_t digit = 0;
      if (ch >= 0x30 && ch <= 0x39)
        digit = uint32_t(ch - 0x30);
      else if (ch >= 0x61 && ch <= 0x66)
        digit = uint32_t(ch - 0x57);
      if (ch >= 0x41 && ch <= 0x46)
        digit = uint32_t(ch - 0x37);
      Call(c, b, sub_825853E0, 0x82585928, {self, 4});
      Call(c, b, sub_82584188, 0x82585934, {sp + 0x60, digit});
      Call(c, b, sub_825855A8, 0x82585940, {self, sp + 0x60});
      Big(b, sp + 0x60).vtable = 0x8207EB04;
      Call(c, b, sub_825F9538, 0x8258594C, {uint32_t(Big(b, sp + 0x60).limbs)});
      length = pvz::guest::Read(b, string + 0x14);
      if (int32_t(i + 1) >= int32_t(length))
        break;
    }
  c.r3.u64 = selfFull;
}
void Native_82585BA0(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64;
  const uint32_t self = c.r3.u32, a = c.r4.u32, d = c.r5.u32;
  Frame f(c, b, 0xE0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xF4, self);
  pvz::guest::Write(b, sp + 0x50, 0);
  InitBig(b, self);
  pvz::guest::Write(b, sp + 0x50, 1);
  uint32_t dCount = Big(b, d).count;
  pvz::guest::Write(b, sp + 0x54, dCount);
  for (uint32_t i = 0; int32_t(i) < int32_t(dCount); ++i) {
    const uint16_t y = int32_t(i) < int32_t(dCount) ? Read16(b, uint32_t(Big(b, d).limbs) + i * 2) : 0;
    InitBig(b, sp + 0x60);
    uint32_t aCount = Big(b, a).count, carry = 0;
    pvz::guest::Write(b, sp + 0x54, aCount);
    if (int32_t(aCount) > 0) {
      for (uint32_t j = 0;; ++j) {
        const uint16_t x = int32_t(j) < int32_t(aCount) ? Read16(b, uint32_t(Big(b, a).limbs) + j * 2) : 0;
        const uint32_t product = uint32_t(x) * uint32_t(y) + carry;
        Call(c, b, sub_82583AC8, 0x82585C84, {sp + 0x60, j + i, product & 0xFFFF});
        aCount = Big(b, a).count;
        carry = product >> 16;
        pvz::guest::Write(b, sp + 0x54, aCount);
        if (int32_t(j + 1) >= int32_t(aCount))
          break;
      }
      if (carry) {
        pvz::guest::Write(b, sp + 0x54, aCount);
        Call(c, b, sub_82583AC8, 0x82585CBC, {sp + 0x60, aCount + i, carry & 0xFFFF});
      }
    }
    Call(c, b, sub_82585998, 0x82585CC8, {self, sp + 0x60});
    Big(b, sp + 0x60).vtable = 0x8207EB04;
    Call(c, b, sub_825F9538, 0x82585CD4, {uint32_t(Big(b, sp + 0x60).limbs)});
    dCount = Big(b, d).count;
    pvz::guest::Write(b, sp + 0x54, dCount);
  }
  Big(b, self).negative = Big(b, a).negative ^ Big(b, d).negative;
  c.r3.u64 = selfFull;
}
void Native_82586790(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64;
  const uint32_t self = c.r3.u32;
  const int32_t bits = c.r4.s32;
  Frame f(c, b, 0xD0);
  const uint32_t sp = c.r1.u32;
  pvz::guest::Write(b, sp + 0xE4, self);
  pvz::guest::Write(b, sp + 0x50, 0);
  InitBig(b, self);
  pvz::guest::Write(b, sp + 0x50, 1);
  const int32_t groups = bits / 16;
  for (int32_t i = 0; i < groups; ++i) {
    Call(c, b, sub_825F9840, 0x825867EC, {});
    const uint32_t lo = c.r3.u8;
    Call(c, b, sub_825F9840, 0x825867F4, {});
    const uint32_t word = ((c.r3.u32 << 8) | lo) & 0xFFFF;
    Call(c, b, sub_825853E0, 0x8258680C, {self, 16});
    Call(c, b, sub_82584188, 0x82586818, {sp + 0x60, word});
    Call(c, b, sub_82585998, 0x82586824, {self, sp + 0x60});
    Big(b, sp + 0x60).vtable = 0x8207EB04;
    Call(c, b, sub_825F9538, 0x82586830, {uint32_t(Big(b, sp + 0x60).limbs)});
  }
  const uint32_t remainder = uint32_t(bits) - uint32_t(groups) * 16;
  Call(c, b, sub_825853E0, 0x82586854, {self, remainder});
  Call(c, b, sub_825F9840, 0x82586858, {});
  const uint32_t lo = c.r3.u8;
  Call(c, b, sub_825F9840, 0x82586860, {});
  const uint32_t word = ((c.r3.u32 << 8) | lo) & 0xFFFF, shift = uint16_t(16 - remainder) & 63;
  Call(c, b, sub_82584188, 0x82586880, {sp + 0x80, shift >= 32 ? 0 : word >> shift});
  Call(c, b, sub_82585998, 0x8258688C, {self, sp + 0x80});
  Big(b, sp + 0x80).vtable = 0x8207EB04;
  Call(c, b, sub_825F9538, 0x82586898, {uint32_t(Big(b, sp + 0x80).limbs)});
  c.r3.u64 = selfFull;
}
void Native_825874B0(PPCContext& c, uint8_t* b) {
  const uint64_t selfFull = c.r3.u64;
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  const uint32_t limbs = Big(b, self).limbs;
  Big(b, self).vtable = 0x8207EB04;
  Call(c, b, sub_825F9538, 0x825874E0, {limbs});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x825874F0, {self});
  c.r3.u64 = selfFull;
}
#endif
} // namespace

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#define LEAF(A, P, NATIVE, OUTPUTS, RANGES, INPUTS)                                                          \
  PVZ_NATIVE_BATCH_REPLACEMENT(b017, P, bigint_##A, sub_##A, NATIVE,                                         \
                               pvz::native::Safety::DeclaredOutputsOnly, OUTPUTS, RANGES)
#define SNAP(A, P, NATIVE)                                                                                   \
  PVZ_NATIVE_BATCH_REPLACEMENT(b017, P, bigint_##A, sub_##A, NATIVE, pvz::native::Safety::SnapshotsOnly,     \
                               noOutputs, NoRanges)
#endif

LEAF(825838F0, true, BigIntEmpty, noOutputs, EmptyRanges, Empty)
SNAP(82583918, false, Native_82583918)
SNAP(82584188, true, BigIntFromInt32)
SNAP(82584250, false, Native_82584250)
SNAP(82584330, false, Native_82584330)
SNAP(82584A78, false, Native_82584A78)
SNAP(82584B80, false, Native_82584B80)
SNAP(82584DA8, false, Native_82584DA8)
SNAP(82584ED8, false, Native_82584ED8)
SNAP(82585860, false, Native_82585860)
SNAP(82585BA0, false, Native_82585BA0)
SNAP(82586790, false, Native_82586790)
SNAP(825874B0, false, Native_825874B0)

#undef SNAP
#undef LEAF
