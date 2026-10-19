// Guest calls preserve Xbox LR sites and keep child home slots in a guest frame.
#pragma once
#include "pvz_pch.h"
#include "guest_memory.h"
#include <array>
#include <initializer_list>

namespace pvz::native {
struct Frame {
  PPCContext& c;
  uint64_t sp, lr;
  Frame(PPCContext& ctx, uint8_t* base, uint32_t bytes) : c(ctx), sp(ctx.r1.u64), lr(ctx.lr) {
    c.r1.u32 -= bytes;
    pvz::guest::Write(base, c.r1.u32, uint32_t(sp));
  }
  ~Frame() {
    c.r1.u64 = sp;
    c.lr = lr;
  }
};

inline void Args(PPCContext& c, std::initializer_list<uint32_t> values) {
  const std::array<PPCRegister*, 8> regs{&c.r3, &c.r4, &c.r5, &c.r6, &c.r7, &c.r8, &c.r9, &c.r10};
  size_t i = 0;
  for (auto value : values)
    regs[i++]->u64 = value;
}

template <class Function>
void Call(PPCContext& ctx, uint8_t* base, Function function, uint32_t lr,
          std::initializer_list<uint32_t> args) {
  Args(ctx, args);
  ctx.lr = lr;
  function(ctx, base);
}

inline void Virtual(PPCContext& ctx, uint8_t* base, uint32_t self, uint32_t slot, uint32_t lr) {
  const uint32_t vtable = pvz::guest::Read(base, self);
  ctx.ctr.u64 = pvz::guest::Read(base, vtable + slot);
  ctx.lr = lr;
  REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
}
} // namespace pvz::native
