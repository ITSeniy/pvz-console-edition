#include "batch_synthetic.h"
#include "xbox/Buttons.h"
namespace {
const SyntheticInputs<PPCContext> DisabledInputs{
    "self=0x1000; r4.u64 boundaries and all 256 low bytes plus 1024 seeded values; only +72 byte written",
    265, 1024, 42, [](size_t i, PPCContext& c, uint8_t* b, uint64_t& seed) {
      c.r3.u64 = 0x1000;
      c.r4.u64 = i < 9 ? Words[i] : i < 265 ? i - 9 : Random(seed);
      b[0x1072] = uint8_t(~c.r4.u32);
    }};
const SyntheticInputs<PPCContext> CheckedInputs{
    "self=0x1000; checkbox checked byte +D8 all 256 values and 1024 xorshift64 seed=42; r3 zero extended",
    256, 1024, 42, [](size_t i, PPCContext& c, uint8_t* b, uint64_t& seed) {
      c.r3.u64 = 0x1000;
      b[0x1000 + offsetof(pvz::xbox::Checkbox, checked)] = uint8_t(i < 256 ? i : Random(seed));
    }};
const SyntheticInputs<PPCContext> PressedInputs{
    "self=0x1000; down/over/gamepad-focus/disabled Cartesian bytes 0,1,2,255 and 1024 seeded byte quadruples",
    256, 1024, 42, [](size_t i, PPCContext& c, uint8_t* b, uint64_t& seed) {
      constexpr uint8_t values[]{0, 1, 2, 255};
      constexpr uint32_t offsets[]{0x74, 0x75, 0xA2, 0x72};
      c.r3.u64 = 0x1000;
      for (size_t j = 0; j < 4; ++j)
        b[0x1000 + offsets[j]] = i < 256 ? values[(i >> (2 * j)) & 3] : uint8_t(Random(seed));
    }};
const SyntheticInputs<PPCContext> GamePressedInputs{
    "self=0x1000; down/over/disabled/no-draw Cartesian bytes 0,1,2,255 and 1024 seeded byte quadruples",
    256, 1024, 42, [](size_t i, PPCContext& c, uint8_t* b, uint64_t& seed) {
      constexpr uint8_t values[]{0, 1, 2, 255};
      constexpr uint32_t offsets[]{0x74, 0x75, 0x72, 0x149};
      c.r3.u64 = 0x1000;
      for (size_t j = 0; j < 4; ++j)
        b[0x1000 + offsets[j]] = i < 256 ? values[(i >> (2 * j)) & 3] : uint8_t(Random(seed));
    }};
const SyntheticInputs<PPCContext> GameCoordinatesInputs{
    "self=0x1000 SP=0x7000; Cartesian signed-word extrema/high GPR bits in r4-r7 and 1024 seeded; int->float stores and final eight-byte entry.SP-16 home slot",
    81, 1024, 42, [](size_t i, PPCContext& c, uint8_t*, uint64_t& seed) {
      c.r3.u64 = 0x1000;
      c.r1.u64 = 0x7000;
      c.r4.u64 = i < 81 ? Words[i % 9] : Random(seed);
      c.r5.u64 = i < 81 ? Words[i / 9] : Random(seed);
      c.r6.u64 = i < 81 ? Words[(i + 1) % 9] : Random(seed);
      c.r7.u64 = i < 81 ? Words[(i + 2) % 9] : Random(seed);
    }};
const SyntheticInputs<PPCContext> PairInputs{
    "destination=0x1000; eight-byte pair initially word boundaries and 1024 seeded values; both words zeroed",
    9, 1024, 42, [](size_t i, PPCContext& c, uint8_t* b, uint64_t& seed) {
      c.r3.u64 = 0x1000;
      pvz::guest::Write(b, 0x1000, uint32_t(i < 9 ? Words[i] : Random(seed)));
      pvz::guest::Write(b, 0x1004, uint32_t(i < 9 ? Words[8 - i] : Random(seed)));
    }};
const SyntheticInputs<PPCContext> CustomTrackInputs{
    "self=0x1000; child=null or 0x2000; horizontal=0/255; signed selector/dimension/child-width Cartesian word extrema and 1024 seeded, including 32-bit doubled width wrap and 64-bit subtraction underflow",
    9*9*9*2*2, 1024, 42, [](size_t i, PPCContext& c, uint8_t* b, uint64_t& seed) {
      const bool boundary = i < 9*9*9*2*2;
      const uint64_t index = boundary ? i : Random(seed);
      c.r3.u64 = 0x1000;
      const uint32_t dimension = uint32_t(boundary ? Words[index % 9] : Random(seed));
      pvz::guest::Write(b, 0x1048, dimension);
      pvz::guest::Write(b, 0x104C, ~dimension);
      pvz::guest::Write(b, 0x1104, uint32_t(boundary ? Words[(index/9)%9] : Random(seed)));
      pvz::guest::Write(b, 0x2048, uint32_t(boundary ? Words[(index/81)%9] : Random(seed)));
      b[0x1100] = (index/729)%2 ? 255 : 0;
      pvz::guest::Write(b, 0x10D8, (index/1458)%2 ? 0x2000 : 0);
    }};
} // namespace
#include "../src/native/b014_buttons.cpp"
int main(int argc, char** argv) {
  return Run(argc, argv, 10);
}
