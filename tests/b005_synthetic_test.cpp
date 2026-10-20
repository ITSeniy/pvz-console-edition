#include "batch_synthetic.h"
#include "../src/native/b005_scrollbar.cpp"
namespace {
const SyntheticInputs<PPCContext> SetHorizontalInputs{
    "self=0x1000; up/down children=0x2000/0x3000; r4 byte boundaries and 1024 seeded values; snapshot all three axis bytes",
    9, 1024, 42, [](size_t i, PPCContext& ctx, uint8_t* base, uint64_t& seed) {
      ctx.r3.u64 = 0x1000;
      ctx.r4.u64 = i < std::size(Words) ? Words[i] : Random(seed);
      pvz::guest::Write(base, 0x1000 + offsetof(pvz::xbox::ScrollbarWidget, up_button), 0x2000);
      pvz::guest::Write(base, 0x1000 + offsetof(pvz::xbox::ScrollbarWidget, down_button), 0x3000);
      base[0x1000 + offsetof(pvz::xbox::ScrollbarWidget, horizontal)] = uint8_t(i * 3);
      base[0x2000 + offsetof(pvz::xbox::ScrollbuttonWidget, horizontal)] = uint8_t(i * 5);
      base[0x3000 + offsetof(pvz::xbox::ScrollbuttonWidget, horizontal)] = uint8_t(i * 7);
    }};
const auto SetHorizontalRanges = [](const PPCContext& ctx) {
  return std::array{
      Range{ctx.r3.u32 + uint32_t(offsetof(pvz::xbox::ScrollbarWidget, horizontal)), 1},
      Range{0x2000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, horizontal)), 1},
      Range{0x3000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, horizontal)), 1}};
};

const SyntheticInputs<PPCContext> SetButtonFieldsInputs{
    "self=0x1000; up/down children=0x2000/0x3000; r4/r5/r6 word boundaries and 1024 seeded triples; snapshot all six stores",
    9, 1024, 42, [](size_t i, PPCContext& ctx, uint8_t* base, uint64_t& seed) {
      ctx.r3.u64 = 0x1000;
      ctx.r4.u64 = i < std::size(Words) ? Words[i] : Random(seed);
      ctx.r5.u64 = i < std::size(Words) ? Words[(i + 1) % std::size(Words)] : Random(seed);
      ctx.r6.u64 = i < std::size(Words) ? Words[(i + 2) % std::size(Words)] : Random(seed);
      pvz::guest::Write(base, 0x1000 + offsetof(pvz::xbox::ScrollbarWidget, up_button), 0x2000);
      pvz::guest::Write(base, 0x1000 + offsetof(pvz::xbox::ScrollbarWidget, down_button), 0x3000);
    }};
const auto SetButtonFieldsRanges = [](const PPCContext&) {
  return std::array{
      Range{0x2000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, field_F8)), 4},
      Range{0x2000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, field_100)), 4},
      Range{0x2000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, field_104)), 4},
      Range{0x3000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, field_F8)), 4},
      Range{0x3000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, field_100)), 4},
      Range{0x3000 + uint32_t(offsetof(pvz::xbox::ScrollbuttonWidget, field_104)), 4}};
};

REX_EXTERN(__imp__sub_824B9BB8);
REX_EXTERN(__imp__sub_824B9BD8);
const AddTest test_824B9BB8{{"824B9BB8", SetHorizontalInputs.description, [] {
  return VerifySparseSynthetic(SetHorizontalInputs, noOutputs, SetHorizontalRanges,
                               SetHorizontal, __imp__sub_824B9BB8);
}}};
const AddTest test_824B9BD8{{"824B9BD8", SetButtonFieldsInputs.description, [] {
  return VerifySparseSynthetic(SetButtonFieldsInputs, noOutputs, SetButtonFieldsRanges,
                               SetButtonFields, __imp__sub_824B9BD8);
}}};
} // namespace
int main(int argc, char** argv) { return Run(argc, argv, 7); }
