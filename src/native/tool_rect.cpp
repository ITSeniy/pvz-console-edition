// Xbox 82130E50..82130F4C, leaf: only stores Rect[0..15] at entry r3.
// Name and full ABI evidence: docs/native-port/reports/T10.md.
#include "replacement.h"
#include "guest_memory.h"
#include "xbox/Board.h"
#include "xbox/LawnApp.h"
#include "xbox/SeedBank.h"
#include <array>
namespace {
void NativeToolRect(PPCContext& ctx, uint8_t* base) {
  const auto out = ctx.r3.u32;
  const auto& board = *reinterpret_cast<const pvz::xbox::Board*>(base + ctx.r4.u32);
  const auto& bank = *reinterpret_cast<const pvz::xbox::SeedBank*>(base + uint32_t(board.seed_banks[0]));
  const auto& app = *reinterpret_cast<const pvz::xbox::LawnApp*>(base + uint32_t(board.app));
  const int count = bank.packet_count;
  uint32_t x = 456 + (count <= 6 ? 0 : count == 7 ? 60 : count == 8 ? 76 : count == 9 ? 112 : 153);
  // Xbox writes the first three words before reading app.board; preserve that
  // order even for an aliased output pointer (normal callers use stack Rect).
  pvz::guest::Write(base, out, x);
  pvz::guest::Write(base, out + 8, 70);
  pvz::guest::Write(base, out + 12, 72);
  const auto has_board = uint32_t(app.board);
  pvz::guest::Write(base, out + 4, 0);
  const bool slot = has_board && int32_t(app.game_mode) == 19;
  // The squirrel branch reloads app.board after the y store in the Xbox body.
  const bool squirrel = !slot && uint32_t(app.board) && int32_t(app.game_mode) == 50;
  if (slot || squirrel)
    pvz::guest::Write(base, out, 600);
  if (int32_t(app.game_mode) >= 77 && int32_t(app.game_mode) <= 89)
    pvz::guest::Write(base, out, 330);
}
const std::array<pvz::native::RegisterOutput, 0> outputs{};
auto Ranges(const PPCContext& input) { return std::array{pvz::native::Range{input.r3.u32, 16}}; }
}
PVZ_NATIVE_REPLACEMENT(tool_rect, sub_82130E50, NativeToolRect,
    pvz::native::Safety::DeclaredOutputsOnly, outputs, Ranges)
