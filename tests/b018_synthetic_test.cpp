#include "batch_synthetic.h"
namespace {
const SyntheticInputs<PPCContext> WordCharacterInputs{
    "r4 low16 exhaustive 65536 values, and 1024 seed42 high-word noise; ASCII letters/digits/underscore and C0..FF",
    65536, 1024, 42, [](size_t i, PPCContext& c, uint8_t*, uint64_t& seed) {
      c.r4.u64 = i < 65536 ? i : Random(seed);
    }};
}
#include "../src/native/b018_edit_widget.cpp"
int main(int argc, char** argv) { return Run(argc, argv, 2); }
