#pragma once
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>
#include <cstdint>

namespace pvz {
// Context for calling a recompiled guest function from a hook. The callee
// gets its own frame Frame bytes below the hook's incoming stack pointer, so
// scratch memory between the two (e.g. ctx.r1 - 0x80) survives the call and
// the hook's own context (CR, FP registers, return value) stays intact.
// Arguments go to r3, r4, ... as sign-extended 32-bit values; other
// registers keep ctx's values (draw hooks rely on that for r4 = Graphics).
template <uint32_t Frame = 0x100, class... Args>
PPCContext GuestFrame(const PPCContext& ctx, Args... args) {
  static_assert(sizeof...(Args) <= 8, "r3..r10 only");
  PPCContext work = ctx;
  work.r1.u32 -= Frame;
  PPCRegister* registers[] = {&work.r3, &work.r4, &work.r5, &work.r6, &work.r7, &work.r8, &work.r9, &work.r10};
  size_t index = 0;
  ((registers[index++]->s64 = int64_t(int32_t(args))), ...);
  return work;
}

// Returns r3 (low 32 bits); cast to int32_t for signed results.
template <uint32_t Frame = 0x100, class... Args>
uint32_t GuestCall(const PPCContext& ctx, uint8_t* base, PPCFunc* fn, Args... args) {
  auto work = GuestFrame<Frame>(ctx, args...);
  fn(work, base);
  return work.r3.u32;
}
}
