// B010. Every translation below follows the Xbox instruction stream.
#include "native/replacement.h"
#include "pvz_pch.h"
#include "guest_memory.h"
#include "xbox/DataObjects.h"
#include <array>
#include <bit>

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#include "native/guest_call.h"
PVZ_NATIVE_BATCH(b010)
REX_EXTERN(sub_82125760);
REX_EXTERN(sub_82125F60);
REX_EXTERN(sub_821266E8);
REX_EXTERN(sub_821818F8);
REX_EXTERN(sub_82181B60);
REX_EXTERN(sub_821BF768);
REX_EXTERN(sub_82552458);
REX_EXTERN(sub_82552538);
REX_EXTERN(sub_825535E0);
REX_EXTERN(sub_82555280);
REX_EXTERN(sub_825557B0);
REX_EXTERN(sub_82555DE8);
REX_EXTERN(sub_82555ED0);
REX_EXTERN(sub_82555F70);
REX_EXTERN(sub_825562F8);
REX_EXTERN(sub_825566E8);
REX_EXTERN(sub_82556910);
REX_EXTERN(sub_82556FA0);
REX_EXTERN(sub_82557000);
REX_EXTERN(sub_82557050);
REX_EXTERN(sub_82557230);
REX_EXTERN(sub_825572B0);
REX_EXTERN(sub_82557588);
REX_EXTERN(sub_82557710);
REX_EXTERN(sub_8255AB58);
REX_EXTERN(sub_8255ABE0);
REX_EXTERN(sub_82564B78);
REX_EXTERN(sub_8257B878);
REX_EXTERN(sub_825931C0);
REX_EXTERN(sub_82593700);
REX_EXTERN(sub_825DDB78);
REX_EXTERN(sub_825F8F50);
REX_EXTERN(sub_825FC8B0);
#endif

namespace {
using pvz::native::Range;
using pvz::native::RegisterOutput;
const std::array<RegisterOutput, 0> noOutputs{};
const auto NoRanges = [](const PPCContext&) { return std::array<Range, 0>{}; };
const auto RefCountRange = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32, sizeof(pvz::xbox::RefCount)}};
};
const auto DataElementRange = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32, offsetof(pvz::xbox::DataElement, field_4) + 1}};
};
const auto DataElementBaseRange = [](const PPCContext& c) { return std::array{Range{c.r3.u32, 4}}; };
const auto EncodingRanges = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32 + uint32_t(offsetof(pvz::xbox::EncodingParser, field_20)), 4},
                    Range{c.r3.u32 + uint32_t(offsetof(pvz::xbox::EncodingParser, field_24)), 1}};
};
const auto DataWriterRange = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32, sizeof(pvz::xbox::DataWriter)}};
};
const auto DataReaderRange = [](const PPCContext& c) {
  return std::array{Range{c.r3.u32, offsetof(pvz::xbox::DataReader, field_14) + 1}};
};

void RefCountConstruct(PPCContext& c, uint8_t* base) {
  pvz::guest::Write(base, c.r3.u32, 0x82074074);
}
void DataElementConstruct(PPCContext& c, uint8_t* base) {
  base[c.r3.u32 + offsetof(pvz::xbox::DataElement, field_4)] = 0;
  pvz::guest::Write(base, c.r3.u32, 0x82074E84);
}
void DataElementBaseConstruct(PPCContext& c, uint8_t* base) {
  pvz::guest::Write(base, c.r3.u32, 0x82074E84);
}
void SetEncodingParser(PPCContext& c, uint8_t* base) {
  uint32_t table = 0;
  switch (c.r4.u32) {
  case 0:
    table = 0x82556040;
    break;
  case 1:
    table = 0x825560B0;
    break;
  case 2:
    table = 0x825562F8;
    break;
  case 3:
    table = 0x82556428;
    break;
  case 4:
    table = 0x825564D0;
    break;
  default:
    return;
  }
  pvz::guest::Write(base, c.r3.u32 + offsetof(pvz::xbox::EncodingParser, field_20), table);
  base[c.r3.u32 + offsetof(pvz::xbox::EncodingParser, field_24)] = 1;
}
void DataWriterConstruct(PPCContext& c, uint8_t* base) {
  const uint32_t self = c.r3.u32;
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataWriter, field_4), 0);
  pvz::guest::Write(base, self, 0x82081F24);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataWriter, field_8), 0);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataWriter, field_C), 0);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataWriter, field_10), 0);
}
void DataReaderConstruct(PPCContext& c, uint8_t* base) {
  const uint32_t self = c.r3.u32;
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataReader, field_4), 0);
  pvz::guest::Write(base, self, 0x82081F40);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataReader, field_8), 0);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataReader, field_C), 0);
  pvz::guest::Write(base, self + offsetof(pvz::xbox::DataReader, field_10), 0);
  base[self + offsetof(pvz::xbox::DataReader, field_14)] = 0;
}

#ifdef PVZ_BATCH_SYNTHETIC_TEST
const pvz::native::SyntheticInputs<PPCContext> ObjectInputs{
    "nine aligned guest addresses including the committed-buffer boundary; varying prior object bytes; 1024 "
    "seeded cases",
    9, 1024, 42, [](size_t i, PPCContext& c, uint8_t* base, uint64_t& seed) {
      constexpr uint32_t addresses[]{0x1000, 0x1004, 0x1010, 0x1100, 0x1FF0, 0x3FF0, 0x7000, 0x7FD0, 0x7FE0};
      const uint32_t self = addresses[(i < 9 ? i : Random(seed)) % std::size(addresses)];
      c.r3.u64 = self;
      for (uint32_t j = 0; j < 32; ++j)
        base[self + j] = uint8_t(i < 9 ? Words[(i + j) % std::size(Words)] : Random(seed));
    }};
const pvz::native::SyntheticInputs<PPCContext> EncodingInputs{
    "self=0x1000; unsigned selector 0..8, 32-bit extrema and 1024 seeded selectors", 9, 1024, 42,
    [](size_t i, PPCContext& c, uint8_t*, uint64_t& seed) {
      c.r3.u64 = 0x1000;
      constexpr uint32_t boundaries[]{0, 1, 2, 3, 4, 5, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF};
      c.r4.u64 = i < std::size(boundaries) ? boundaries[i] : Random(seed);
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
void Write(uint8_t* b, uint32_t at, uint32_t value) {
  pvz::guest::Write(b, at, value);
}
uint16_t Read16(uint8_t* b, uint32_t at) {
  return uint16_t(uint16_t(b[at]) << 8 | b[at + 1]);
}
void Write16(uint8_t* b, uint32_t at, uint16_t v) {
  b[at] = uint8_t(v >> 8);
  b[at + 1] = uint8_t(v);
}
uint64_t Read64(uint8_t* b, uint32_t at) {
  return uint64_t(Read(b, at)) << 32 | Read(b, at + 4);
}
void Write64(uint8_t* b, uint32_t at, uint64_t v) {
  Write(b, at, uint32_t(v >> 32));
  Write(b, at + 4, uint32_t(v));
}
void Indirect(PPCContext& c, uint8_t* b, uint32_t target, uint32_t lr, std::initializer_list<uint32_t> args) {
  Call(
      c, b,
      [target](PPCContext& ctx, uint8_t* base) {
        ctx.ctr.u64 = target;
        REX_CALL_INDIRECT_FUNC(target);
      },
      lr, args);
}
uint32_t VectorCount(uint8_t* b, uint32_t vector, uint32_t shift) {
  return uint32_t(int32_t(Read(b, vector + 0x10) - Read(b, vector + 0xC)) >> shift);
}
void Native_82126CE8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x80);
  Write(b, self, 0x820B4828);
  Call(c, b, sub_825DDB78, 0x82126D14, {4});
  const uint32_t proxy = c.r3.u32;
  if (proxy)
    Write(b, proxy, self + 4);
  Write(b, self + 4, proxy);
  Write(b, self + 0x10, 0);
  Write(b, self + 0x14, 0);
  Write(b, self + 0x18, 0);
  c.r3.u64 = self;
}
void Native_821B4010(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x60);
  Write(b, self, 0x82074074);
  if (flags & 1) {
    Call(c, b, sub_825F8F50, 0x821B4040, {self});
    c.r3.u64 = self;
  }
}
template <bool Delete> void SyncDestruct(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, Delete ? 0x70 : 0x60);
  Write(b, self, 0x820B4828);
  const uint32_t begin = Read(b, self + 0x10);
  if (begin)
    Call(c, b, sub_825F8F50, Delete ? 0x821B6CF4 : 0x8220D380, {begin});
  const uint32_t proxy = Read(b, self + 4);
  Write(b, self + 0x10, 0);
  Write(b, self + 0x14, 0);
  Write(b, self + 0x18, 0);
  Call(c, b, sub_825F8F50, Delete ? 0x821B6D0C : 0x8220D398, {proxy});
  if constexpr (Delete) {
    c.r3.u64 = self;
    if (flags & 1)
      Call(c, b, sub_825F8F50, 0x821B6D20, {self});
    c.r3.u64 = self;
  }
}
void Native_821B6CC0(PPCContext& c, uint8_t* b) {
  SyncDestruct<true>(c, b);
}
void Native_8220D350(PPCContext& c, uint8_t* b) {
  SyncDestruct<false>(c, b);
}
void Native_824AA288(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_825FC8B0, 0x824AA2A4, {self, c.r4.u32, 0x24});
  b[self + 0x24] = 0;
  b[self + 0x25] = 1;
}
void Native_824B63B8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Write(b, self, 0x82065E0C);
  Call(c, b, sub_82564B78, uint32_t(c.lr), {self + 4});
}
void Native_824B6990(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Write(b, self, 0x82065E0C);
  Call(c, b, sub_825557B0, 0x824B69B8, {self + 4});
  Write(b, self + 0x1C, 0);
  Write(b, self + 0x20, 0);
  Write(b, self + 0x24, 0);
  c.r3.u64 = self;
}
void Native_824B7370(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Write(b, self, 0x82065E0C);
  Call(c, b, sub_82564B78, 0x824B73A0, {self + 4});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x824B73B0, {self});
  c.r3.u64 = self;
}
void Native_825523F0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  b[self + 4] = 0;
  Write(b, self, 0x82074E90);
  Call(c, b, sub_82125760, 0x8255242C, {self + 8, 0, 0});
  b[self + 4] = 0;
  Write(b, self + 0x24, 0);
  c.r3.u64 = self;
}
void Native_82552458(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Write(b, c.r1.u32 + 0x84, self);
  Write(b, self, 0x82074E90);
  const uint32_t child = Read(b, self + 0x24);
  if (child) {
    Write(b, c.r1.u32 + 0x50, child);
    Args(c, {child, 1});
    Virtual(c, b, child, 0, 0x825524A8);
  }
  Call(c, b, sub_82125760, 0x825524B8, {self + 8, 1, 0});
  Write(b, self, 0x82074E84);
}
void Native_82552538(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, vector = self + 8;
  Frame f(c, b, 0x90);
  Write(b, c.r1.u32 + 0xA4, self);
  Write(b, self, 0x82074E9C);
  Write(b, c.r1.u32 + 0x50, vector);
  if (VectorCount(b, vector, 2)) {
    uint32_t index = 0, offset = 0;
    do {
      const uint32_t begin = Read(b, vector + 0xC);
      Write(b, c.r1.u32 + 0x50, vector);
      if (index >= VectorCount(b, vector, 2))
        ppc_trap(c, b, 22);
      const uint32_t child = Read(b, begin + offset);
      if (child) {
        Args(c, {child, 1});
        Virtual(c, b, child, 0, 0x825525C0);
      }
      ++index;
      offset += 4;
      Write(b, c.r1.u32 + 0x50, vector);
      if (index >= VectorCount(b, vector, 2))
        break;
    } while (true);
  }
  Call(c, b, sub_82564B78, 0x825525EC, {vector});
  Write(b, self, 0x82074E84);
}
void Native_82552660(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, source = c.r4.u32;
  Frame f(c, b, 0x70);
  Write(b, c.r1.u32 + 0x84, self);
  b[self + 4] = 0;
  Write(b, self, 0x82074E90);
  Call(c, b, sub_82125F60, 0x82552694, {self + 8, source});
  b[self + 4] = 0;
  Write(b, self + 0x24, 0);
  c.r3.u64 = self;
}
void Native_825529F8(PPCContext& c, uint8_t* b) {
  const uint32_t source = c.r3.u32;
  Frame f(c, b, 0x80);
  Call(c, b, sub_825DDB78, 0x82552A14, {0x28});
  Write(b, c.r1.u32 + 0x50, c.r3.u32);
  uint32_t copy = 0;
  if (c.r3.u32) {
    Call(c, b, sub_82555280, 0x82552A28, {c.r3.u32, source});
    copy = c.r3.u32;
  }
  const uint32_t child = Read(b, source + 0x24);
  if (child) {
    Args(c, {child});
    Virtual(c, b, child, 4, 0x82552A54);
    Write(b, copy + 0x24, c.r3.u32);
  }
  c.r3.u64 = copy;
}
void Native_82553550(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Write(b, c.r1.u32 + 0x84, self);
  b[self + 4] = 0;
  Write(b, self, 0x82074E9C);
  Call(c, b, sub_825557B0, 0x8255358C, {self + 8});
  b[self + 4] = 1;
  c.r3.u64 = self;
}
void Native_825535E0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, source = c.r4.u32 + 8, vector = self + 8;
  Frame f(c, b, 0x90);
  Write(b, c.r1.u32 + 0xA4, self);
  b[self + 4] = 0;
  Write(b, self, 0x82074E9C);
  Call(c, b, sub_825557B0, 0x8255361C, {vector});
  Write(b, c.r1.u32 + 0x50, source);
  b[self + 4] = 1;
  uint32_t index = 0, offset = 0;
  if (VectorCount(b, source, 2))
    do {
      const uint32_t begin = Read(b, source + 0xC);
      Write(b, c.r1.u32 + 0x50, source);
      if (index >= VectorCount(b, source, 2))
        ppc_trap(c, b, 22);
      const uint32_t child = Read(b, begin + offset);
      Args(c, {child});
      Virtual(c, b, child, 4, 0x82553678);
      Write(b, c.r1.u32 + 0x50, c.r3.u32);
      Call(c, b, sub_8257B878, 0x82553688, {vector, c.r1.u32 + 0x50});
      ++index;
      offset += 4;
      Write(b, c.r1.u32 + 0x50, source);
      if (index >= VectorCount(b, source, 2))
        break;
    } while (true);
  c.r3.u64 = self;
}
void Native_82553840(PPCContext& c, uint8_t* b) {
  const uint32_t source = c.r3.u32;
  Frame f(c, b, 0x70);
  Call(c, b, sub_825DDB78, 0x82553864, {0x20});
  Write(b, c.r1.u32 + 0x50, c.r3.u32);
  if (c.r3.u32)
    Call(c, b, sub_825535E0, 0x82553878, {c.r3.u32, source});
  else
    c.r3.u64 = 0;
}
void Native_82555120(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x60);
  Write(b, self, 0x82074E84);
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x8255514C, {self});
  c.r3.u64 = self;
}
template <auto Destruct, uint32_t Lr1, uint32_t Lr2> void Deleting(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Call(c, b, Destruct, Lr1, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, Lr2, {self});
  c.r3.u64 = self;
}
void Native_82555168(PPCContext& c, uint8_t* b) {
  Deleting<sub_82552458, 0x82555188, 0x82555198>(c, b);
}
void Native_82555280(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, source = c.r4.u32;
  Frame f(c, b, 0x70);
  Write(b, c.r1.u32 + 0x84, self);
  Write(b, self, 0x82074E84);
  b[self + 4] = b[source + 4];
  Write(b, self, 0x82074E90);
  Call(c, b, sub_82125F60, 0x825552C8, {self + 8, source + 8});
  Write(b, self + 0x24, Read(b, source + 0x24));
  c.r3.u64 = self;
}
void Native_82555308(PPCContext& c, uint8_t* b) {
  Deleting<sub_82552538, 0x82555328, 0x82555338>(c, b);
}
void Native_825562F8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, out = c.r4.u32, error = c.r5.u32;
  Frame f(c, b, 0x80);
  const uint32_t sp = c.r1.u32;
  Write16(b, sp + 0x50, 0);
  Call(c, b, sub_82555ED0, 0x8255632C, {sp + 0x50, 2, 1, Read(b, self + 4)});
  if (c.r3.u32 != 1) {
    c.r3.u64 = 0;
    return;
  }
  uint16_t word = Read16(b, sp + 0x50);
  if (b[self + 0x25]) {
    b[self + 0x25] = 0;
    if (word == 0xFEFF || word == 0xFFFE) {
      b[self + 0x26] = word == 0xFFFE ? 1 : 0;
      Call(c, b, sub_825562F8, 0x82556370, {self, out, error});
      return;
    }
  }
  if (b[self + 0x26]) {
    word = uint16_t((word << 8) | (word >> 8));
    Write16(b, sp + 0x50, word);
  }
  if ((word & 0xFC00) == 0xD800) {
    Write16(b, sp + 0x52, 0);
    b[error] = 1;
    Call(c, b, sub_82555ED0, 0x825563D0, {sp + 0x52, 2, 1, Read(b, self + 4)});
    if (c.r3.u32 != 1) {
      c.r3.u64 = 0;
      return;
    }
    uint16_t next = Read16(b, sp + 0x52);
    if (b[self + 0x26])
      next = uint16_t((next << 8) | (next >> 8));
    if ((next & 0xFC00) != 0xDC00) {
      c.r3.u64 = 0;
      return;
    }
    const uint32_t rotated = std::rotl(uint32_t(Read16(b, sp + 0x50)), 10);
    // Xbox stores a halfword, including its original truncation of surrogate bits.
    Write16(b, out, uint16_t((uint32_t(next) & ~0x03FFFC00u) | (rotated & 0x03FFFC00u)));
  } else
    Write16(b, out, word);
  c.r3.u64 = 1;
  b[error] = 0;
}
void Native_82556598(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  const uint32_t stream = Read(b, self + 4);
  if (stream) {
    Call(c, b, sub_82555DE8, 0x825565BC, {stream});
    Write(b, self + 4, 0);
    c.r3.u64 = 1;
  } else
    c.r3.u64 = 0;
}
void Native_82556650(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  if (VectorCount(b, self + 8, 1)) {
    c.r3.u64 = 0;
    return;
  }
  const uint32_t stream = Read(b, self + 4);
  if (!stream) {
    c.r3.u64 = 1;
    return;
  }
  const uint32_t manager = Read(b, 0x828A3498);
  if (manager) {
    Args(c, {manager, stream});
    Virtual(c, b, manager, 0x2C, 0x825566A4);
    c.r3.u64 = c.r3.u32 ? 1 : 0;
  } else
    c.r3.u64 = Read(b, stream + 0x14) >= Read(b, stream + 0x10) ? 1 : 0;
}
void Native_825566E8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Write(b, c.r1.u32 + 0x84, self);
  Write(b, self, 0x82075088);
  const uint32_t stream = Read(b, self + 4);
  if (stream)
    Call(c, b, sub_82555DE8, 0x82556724, {stream});
  Call(c, b, sub_82564B78, 0x8255672C, {self + 8});
}
void Native_82556808(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, out = c.r4.u32, vector = self + 8;
  Frame f(c, b, 0x80);
  if (!out) {
    c.r3.u64 = 3;
    return;
  }
  if ((Read(b, self + 0x18) - Read(b, self + 0x14)) & 0xFFFFFFFEu) {
    Call(c, b, sub_82557230, 0x8255684C, {vector});
    Write16(b, out, Read16(b, c.r3.u32));
    if ((Read(b, vector + 0x10) - Read(b, vector + 0xC)) & 0xFFFFFFFEu)
      Write(b, vector + 0x10, Read(b, vector + 0x10) - 2);
    c.r3.u64 = 0;
    return;
  }
  const uint32_t stream = Read(b, self + 4);
  if (!stream) {
    c.r3.u64 = 2;
    return;
  }
  const uint32_t manager = Read(b, 0x828A3498);
  bool eof;
  if (manager) {
    Args(c, {manager, stream});
    Virtual(c, b, manager, 0x2C, 0x825568A4);
    eof = c.r3.s32 != 0;
  } else
    eof = Read(b, stream + 0x14) >= Read(b, stream + 0x10);
  if (eof) {
    c.r3.u64 = 2;
    return;
  }
  b[c.r1.u32 + 0x50] = 0;
  Indirect(c, b, Read(b, self + 0x20), 0x825568E4, {self, out, c.r1.u32 + 0x50});
  c.r3.u64 = c.r3.u8 ? 0 : (b[c.r1.u32 + 0x50] ? 1 : 2);
}
void Native_82556AE8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, path = c.r4.u32;
  Frame f(c, b, 0x80);
  const uint32_t chars = Read(b, path + 0x18) >= 16 ? Read(b, path + 4) : path + 4;
  Call(c, b, sub_82556910, 0x82556B1C, {chars, 0x82075004});
  Write(b, self + 4, c.r3.u32);
  if (!c.r3.u32) {
    c.r3.u64 = 0;
    return;
  }
  if (!b[self + 0x24]) {
    uint32_t manager = Read(b, 0x828A3498), stream = Read(b, self + 4);
    if (manager) {
      Args(c, {manager, stream, 0, 2});
      Virtual(c, b, manager, 0xC, 0x82556B64);
    } else
      Write(b, stream + 0x14, Read(b, stream + 0x10));
    manager = Read(b, 0x828A3498);
    stream = Read(b, self + 4);
    uint32_t length;
    if (manager) {
      Args(c, {manager, stream});
      Virtual(c, b, manager, 0x10, 0x82556B94);
      length = c.r3.u32;
    } else
      length = Read(b, stream + 0x14);
    manager = Read(b, 0x828A3498);
    stream = Read(b, self + 4);
    if (manager) {
      Args(c, {manager, stream, 0, 0});
      Virtual(c, b, manager, 0xC, 0x82556BCC);
    } else
      Write(b, stream + 0x14, 0);
    Write(b, self + 0x20, 0x82556040);
    auto readByte = [&](uint32_t lr) {
      Call(c, b, sub_82555F70, lr, {Read(b, self + 4)});
      return c.r3.u32;
    };
    auto unget = [&](uint32_t value, uint32_t lr) {
      const uint32_t m = Read(b, 0x828A3498), s = Read(b, self + 4);
      if (m) {
        Args(c, {m, value, s});
        Virtual(c, b, m, 0x20, lr);
      } else
        Write(b, s + 0x14, Read(b, s + 0x14) - 1);
    };
    if (int32_t(length) >= 2) {
      const uint32_t first = readByte(0x82556BF4), second = readByte(0x82556C00);
      if ((int32_t(first) == 255 && int32_t(second) == 254) ||
          (int32_t(first) == 254 && int32_t(second) == 255))
        Write(b, self + 0x20, 0x825562F8);
      unget(second, 0x82556C54);
      unget(first, 0x82556C88);
    }
    if (Read(b, self + 0x20) == 0x82556040 && int32_t(length) >= 3) {
      const uint32_t first = readByte(0x82556CBC), second = readByte(0x82556CC8),
                     third = readByte(0x82556CD4);
      if (int32_t(first) == 0xEF && int32_t(second) == 0xBB && int32_t(third) == 0xBF)
        Write(b, self + 0x20, 0x825560B0);
      unget(third, 0x82556D20);
      unget(second, 0x82556D54);
      unget(first, 0x82556D88);
    }
  }
  b[self + 0x25] = 1;
  c.r3.u64 = 1;
}
void Native_82556DB0(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Write(b, self, 0x82075088);
  Call(c, b, sub_825557B0, 0x82556DD8, {self + 8});
  Write(b, self + 4, 0);
  Write(b, self + 0x20, 0x825560B0);
  b[self + 0x24] = 0;
  b[self + 0x25] = 0;
  b[self + 0x26] = 0;
  c.r3.u64 = self;
}
void Native_82556E10(PPCContext& c, uint8_t* b) {
  const uint32_t vector = c.r3.u32 + 8, source = c.r4.u32, length = Read(b, source + 0x14);
  Frame f(c, b, 0x70);
  Call(c, b, sub_82557588, 0x82556E38, {vector, length, 0});
  if (int32_t(length) <= 0)
    return;
  for (uint32_t index = 0; index < length; ++index) {
    const uint32_t from = length - index - 1;
    if (from > Read(b, source + 0x14))
      ppc_trap(c, b, 22);
    const uint32_t chars = Read(b, source + 0x18) >= 8 ? Read(b, source + 4) : source + 4;
    const uint32_t begin = Read(b, vector + 0xC);
    if (index >= VectorCount(b, vector, 1))
      ppc_trap(c, b, 22);
    Write16(b, begin + index * 2, Read16(b, chars + from * 2));
  }
}
void Native_82556EC0(PPCContext& c, uint8_t* b) {
  Frame f(c, b, 0x60);
  Call(c, b, sub_82557710, 0x82556ED4, {c.r3.u32 + 8});
  c.r3.u64 = 1;
}
void Native_82556EE8(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, vector = self + 8, source = c.r4.u32;
  const uint32_t old = VectorCount(b, vector, 1);
  Frame f(c, b, 0x90);
  const uint32_t sp = c.r1.u32;
  Call(c, b, sub_82557588, 0x82556F24, {vector, Read(b, source + 0x14) + old, 0});
  const uint32_t begin = Read(b, self + 0x14);
  if (begin > Read(b, self + 0x18))
    ppc_trap(c, b, 22);
  Write(b, sp + 0x50, Read(b, vector));
  Write(b, sp + 0x54, begin);
  Write64(b, sp + 0x58, Read64(b, sp + 0x50));
  Call(c, b, sub_82556FA0, 0x82556F58, {sp + 0x58, old});
  Call(c, b, sub_82557000, 0x82556F64, {sp + 0x50, source});
  const uint64_t first = Read64(b, c.r3.u32);
  Call(c, b, sub_82557050, 0x82556F78, {sp + 0x60, source});
  const uint64_t last = Read64(b, c.r3.u32);
  c.r3.u64 = sp + 0x58;
  c.r4.u64 = last;
  c.r5.u64 = first;
  c.r6.u64 = Read64(b, sp + 0x58);
  Call(c, b, sub_825572B0, 0x82556F90, {});
  c.r3.u64 = 1;
}
void Native_825571E0(PPCContext& c, uint8_t* b) {
  Deleting<sub_825566E8, 0x82557200, 0x82557210>(c, b);
}
void Native_8255AB58(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x70);
  Write(b, c.r1.u32 + 0x84, self);
  Write(b, self, 0x82077798);
  Call(c, b, sub_82564B78, 0x8255AB8C, {self + 0x50});
  Call(c, b, sub_821818F8, 0x8255AB94, {self + 0x30});
  Call(c, b, sub_821818F8, 0x8255AB9C, {self + 0x10});
}
template <uint32_t Address> void SyncConstruct(PPCContext& c, uint8_t* b) {
  constexpr bool plain = Address == 0x8255B080;
  const uint32_t self = c.r3.u32, stream = c.r4.u32;
  Frame f(c, b, plain ? 0x70 : 0x80);
  const uint32_t sp = c.r1.u32;
  Write(b, sp + (plain ? 0x84 : 0x94), self);
  Write(b, self, 0x82077798);
  Call(c, b, sub_82181B60, plain ? 0x8255B0BC : (Address == 0x8255B190 ? 0x8255B1C8 : 0x8255B2C8),
       {self + 0x10, sp + 0x51, sp + 0x50});
  Call(c, b, sub_82181B60, plain ? 0x8255B0CC : (Address == 0x8255B190 ? 0x8255B1D8 : 0x8255B2D8),
       {self + 0x30, sp + 0x52, sp + 0x51});
  Call(c, b, sub_821BF768, plain ? 0x8255B0D4 : (Address == 0x8255B190 ? 0x8255B1E0 : 0x8255B2E0),
       {self + 0x50});
  Write(b, self + 4, 0);
  Write(b, self + 8, 0);
  Call(c, b, sub_8255ABE0, plain ? 0x8255B0E8 : (Address == 0x8255B190 ? 0x8255B1F4 : 0x8255B2F4), {self});
  if constexpr (!plain)
    Write(b, self + (Address == 0x8255B190 ? 4 : 8), stream);
  c.r3.u64 = self;
}
void Native_8255B080(PPCContext& c, uint8_t* b) {
  SyncConstruct<0x8255B080>(c, b);
}
void Native_8255B190(PPCContext& c, uint8_t* b) {
  SyncConstruct<0x8255B190>(c, b);
}
void Native_8255B290(PPCContext& c, uint8_t* b) {
  SyncConstruct<0x8255B290>(c, b);
}
void Native_8255B388(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32;
  Frame f(c, b, 0x60);
  Call(c, b, sub_821266E8, 0x8255B3A0, {});
  Write(b, self, 0x8207778C);
  c.r3.u64 = self;
}
void Native_8255B3C8(PPCContext& c, uint8_t* b) {
  Deleting<sub_8255AB58, 0x8255B3E8, 0x8255B3F8>(c, b);
}
void Native_825934B8(PPCContext& c, uint8_t* b) {
  Write(b, c.r3.u32, 0x82081F24);
  Call(c, b, sub_825931C0, uint32_t(c.lr), {});
}
void Native_82593680(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Write(b, self, 0x82081F24);
  Call(c, b, sub_825931C0, 0x825936AC, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x825936BC, {self});
  c.r3.u64 = self;
}
void Native_82593A08(PPCContext& c, uint8_t* b) {
  Write(b, c.r3.u32, 0x82081F40);
  Call(c, b, sub_82593700, uint32_t(c.lr), {});
}
void Native_82593B38(PPCContext& c, uint8_t* b) {
  const uint32_t self = c.r3.u32, flags = c.r4.u32;
  Frame f(c, b, 0x70);
  Write(b, self, 0x82081F40);
  Call(c, b, sub_82593700, 0x82593B64, {self, flags});
  if (flags & 1)
    Call(c, b, sub_825F8F50, 0x82593B74, {self});
  c.r3.u64 = self;
}
#endif
} // namespace

#ifndef PVZ_BATCH_SYNTHETIC_TEST
#define LEAF(A, P, NATIVE, OUTPUTS, RANGES, INPUTS)                                                          \
  PVZ_NATIVE_BATCH_REPLACEMENT(b010, P, data_##A, sub_##A, NATIVE, pvz::native::Safety::DeclaredOutputsOnly, \
                               OUTPUTS, RANGES)
#define SNAP(A, P, NATIVE)                                                                                   \
  PVZ_NATIVE_BATCH_REPLACEMENT(b010, P, data_##A, sub_##A, NATIVE, pvz::native::Safety::SnapshotsOnly,       \
                               noOutputs, NoRanges)
#endif

LEAF(821B4000, true, RefCountConstruct, noOutputs, RefCountRange, Object)
LEAF(825522F8, true, DataElementConstruct, noOutputs, DataElementRange, Object)
LEAF(82552310, true, DataElementBaseConstruct, noOutputs, DataElementBaseRange, Object)
LEAF(825565E8, true, SetEncodingParser, noOutputs, EncodingRanges, Encoding)
LEAF(82593198, true, DataWriterConstruct, noOutputs, DataWriterRange, Object)
LEAF(825936D8, true, DataReaderConstruct, noOutputs, DataReaderRange, Object)
LEAF(82593BB0, true, RefCountConstruct, noOutputs, RefCountRange, Object)

SNAP(82126CE8, true, Native_82126CE8)
SNAP(821B4010, true, Native_821B4010)
SNAP(821B6CC0, true, Native_821B6CC0)
SNAP(8220D350, true, Native_8220D350)
SNAP(824AA288, false, Native_824AA288)
SNAP(824B63B8, true, Native_824B63B8)
SNAP(824B6990, true, Native_824B6990)
SNAP(824B7370, false, Native_824B7370)
SNAP(825523F0, true, Native_825523F0)
SNAP(82552458, true, Native_82552458)
SNAP(82552538, true, Native_82552538)
SNAP(82552660, true, Native_82552660)
SNAP(825529F8, true, Native_825529F8)
SNAP(82553550, true, Native_82553550)
SNAP(825535E0, true, Native_825535E0)
SNAP(82553840, true, Native_82553840)
SNAP(82555120, false, Native_82555120)
SNAP(82555168, true, Native_82555168)
SNAP(82555280, true, Native_82555280)
SNAP(82555308, true, Native_82555308)
SNAP(825562F8, false, Native_825562F8)
SNAP(82556598, true, Native_82556598)
SNAP(82556650, true, Native_82556650)
SNAP(825566E8, true, Native_825566E8)
SNAP(82556808, true, Native_82556808)
SNAP(82556AE8, true, Native_82556AE8)
SNAP(82556DB0, true, Native_82556DB0)
SNAP(82556E10, true, Native_82556E10)
SNAP(82556EC0, true, Native_82556EC0)
SNAP(82556EE8, true, Native_82556EE8)
SNAP(825571E0, false, Native_825571E0)
SNAP(8255AB58, true, Native_8255AB58)
SNAP(8255B080, false, Native_8255B080)
SNAP(8255B190, true, Native_8255B190)
SNAP(8255B290, true, Native_8255B290)
SNAP(8255B388, false, Native_8255B388)
SNAP(8255B3C8, false, Native_8255B3C8)
SNAP(825934B8, false, Native_825934B8)
SNAP(82593680, false, Native_82593680)
SNAP(82593A08, true, Native_82593A08)
SNAP(82593B38, false, Native_82593B38)

#undef LEAF
#undef SNAP
