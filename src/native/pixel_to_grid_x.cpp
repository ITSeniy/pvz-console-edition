// Native-port T06. Logic transcribed from Xbox PPC 82149538..821495A8.
// No Android/PC implementation or offsets are used. See reports/T06.md.
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include "xbox/Board.h"
#include "xbox/LawnApp.h"
#include <algorithm>
#include <atomic>

REXCVAR_DEFINE_BOOL(pvz_native_pixel_to_grid_x, false, "Native port",
    "Use the native Xbox Board::PixelToGridX replacement");
REXCVAR_DEFINE_BOOL(pvz_native_pixel_to_grid_x_compare, false, "Diagnostics",
    "Compare native and original PixelToGridX on each real call; log every result");
REXCVAR_DEFINE_BOOL(pvz_native_pixel_to_grid_x_inject_error, false, "Diagnostics",
    "Comparison sensitivity only: perturb native PixelToGridX by one");

REX_EXTERN(__imp__sub_82149538);
REX_EXTERN(sub_82291A00);

namespace {
// Xbox ABI: r3=Board*, r4=signed pixel x, r5=pixel y; signed integer in r3.
// Zen's special-coordinate helper is the same child call as original 82149568.
// It is deliberately NOT another native replacement in this pilot.
void Native(PPCContext& ctx, uint8_t* base) {
  const auto& board = *reinterpret_cast<const pvz::xbox::Board*>(base + ctx.r3.u32);
  const auto& app = *reinterpret_cast<const pvz::xbox::LawnApp*>(base + uint32_t(board.app));
  const int32_t terrain = board.background;
  if (int32_t(app.game_mode) == 44 && (terrain == 6 || terrain == 8 || terrain == 7)) {
    ctx.r3.u64 = uint32_t(app.field_9f0);
    sub_82291A00(ctx, base);
  } else {
    const int32_t x = ctx.r4.s32;
    // The subtraction is evaluated only for x >= 40, so it cannot overflow.
    ctx.r3.s64 = x < 40 ? -1 : std::clamp((x - 40) / 80, 0, 8);
  }
  static const bool inject = REXCVAR_GET(pvz_native_pixel_to_grid_x_inject_error);
  static const bool compare = REXCVAR_GET(pvz_native_pixel_to_grid_x_compare);
  // Explicit negative control in the native implementation. With compare
  // enabled but replacement disabled the real game still uses the original.
  if (inject && compare) ctx.r3.u64 ^= 1;
}
std::atomic<uint64_t> compared{0};
std::atomic<uint64_t> mismatches{0};
}

REX_HOOK_RAW(sub_82149538) {
  static const bool enabled = REXCVAR_GET(pvz_native_pixel_to_grid_x);
  static const bool compare = REXCVAR_GET(pvz_native_pixel_to_grid_x_compare);
  if (!enabled && !compare) {
    __imp__sub_82149538(ctx, base);
    return;
  }
  if (!compare) {
    Native(ctx, base);
    return;
  }
  const auto board = ctx.r3.u32;
  const auto x = ctx.r4.s32;
  const auto y = ctx.r5.s32;
  PPCContext native = ctx;
  Native(native, base);
  static const bool inject = REXCVAR_GET(pvz_native_pixel_to_grid_x_inject_error);
  const auto observed = native.r3.u64;
  __imp__sub_82149538(ctx, base);
  const auto original = ctx.r3.u64;
  const bool mismatch = observed != original;
  const auto n = ++compared;
  const auto errors = mismatch ? ++mismatches : mismatches.load();
  if (mismatch) {
    REXLOG_ERROR("PvZ native PixelToGridX mismatch call {} board {:08X} x {} y {} native {} original {} injected {}",
                 n, board, x, y, observed, original, inject);
  }
  // Every comparison is auditable; killed smoke processes need no exit hook
  // or destructor to reveal exact call counts and arguments.
  REXLOG_INFO("PvZ native PixelToGridX compare call {} errors {} board {:08X} x {} y {} native {} original {} selected {}",
              n, errors, board, x, y, observed, original, enabled ? "native" : "original");
  if (enabled) ctx.r3 = native.r3;
}
