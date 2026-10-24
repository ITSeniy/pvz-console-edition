#include "batch_synthetic.h"
#include "xbox/Dialog.h"
namespace {
const SyntheticInputs<PPCContext> ModalInputs{
    "self=0x1000; modal byte +18C all 256 values and 1024 xorshift64 seed=42; zero-extended r3.u64",
    256, 1024, 42, [](size_t i, PPCContext& c, uint8_t* b, uint64_t& seed) {
      c.r3.u64 = 0x1000;
      b[0x1000 + offsetof(pvz::xbox::Dialog, modal)] = uint8_t(i < 256 ? i : Random(seed));
    }};
} // namespace
#include "../src/native/b008_dialog.cpp"
int main(int argc, char** argv) {
  return Run(argc, argv, 4);
}
