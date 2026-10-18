// Native-port T06. Logic transcribed from Xbox PPC 82149538..821495A8.
// No Android/PC implementation or offsets are used. See reports/T06.md.
#include "replacement.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include "xbox/Board.h"
#include "xbox/LawnApp.h"
#include <algorithm>
#include <array>

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
}
const std::array outputs{pvz::native::RegisterOutput{
    "r3", &PPCContext::r3, 64, pvz::native::Kind::Signed}};
auto Ranges(const PPCContext& input) {
  // 82291A00 uses four 8-byte conversion slots in the PPC red zone.
  // Declare them even outside Zen; the ordinary branch leaves them intact.
  return std::array{pvz::native::Range{input.r1.u32 - 32, 32}};
}
}
PVZ_NATIVE_REPLACEMENT(pixel_to_grid_x, sub_82149538, Native,
    pvz::native::Safety::DeclaredOutputsOnly, outputs, Ranges)
