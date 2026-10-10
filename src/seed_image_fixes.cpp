#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include "guest_memory.h"
#include <array>
#include <bit>

REXCVAR_DEFINE_BOOL(pvz_peashooter_direct_draw, true, "Diagnostics", "Draw peashooter seed art from native animation layers instead of the render cache");
REX_EXTERN(__imp__sub_821DEFC0);
REX_EXTERN(sub_821DE058); // ReanimatorCache::DrawReanimatorFrame
REX_EXTERN(__imp__sub_821E1560); // Reanimation::DrawRenderGroup
namespace {
thread_local bool drawing_seed_frame = false;
thread_local bool drawing_aquarium_packet = false;
}
REX_EXTERN(__imp__sub_8220D990); // DrawSeedPacket.
REX_HOOK_RAW(sub_8220D990) {
  const auto previous = drawing_aquarium_packet;
  drawing_aquarium_packet = ctx.r6.u32 == 59 || ctx.r6.u32 == 60;
  __imp__sub_8220D990(ctx, base);
  drawing_aquarium_packet = previous;
}
REX_EXTERN(__imp__sub_8227C240); // TodBltMatrix with an explicit source cell.
extern "C" bool DrawAquariumPacketCell(PPCContext& ctx, uint8_t* base) {
  using pvz::guest::Read; using pvz::guest::Write;
  const auto rect = ctx.r9.u32, matrix = ctx.r5.u32;
  if (!drawing_aquarium_packet || !rect || !matrix || ctx.r4.u32 != Read(base, 0x82D0A0B8)) {
    return false;
  }
  std::array<uint32_t, 4> source{};
  for (size_t i = 0; i < source.size(); ++i) source[i] = Read(base, rect + uint32_t(i) * 4);
  if (source[2] <= 4 || source[3] <= 4) return false;
  // Keep bilinear samples inside this atlas cell. Preserve its destination
  // size and center, so the neighboring sun/diamond cells cannot bleed into
  // the aquarium buttons during hover scaling or disabled-card drawing.
  std::array<uint32_t, 4> transform{};
  constexpr std::array<uint32_t, 4> offsets{0, 4, 12, 16};
  for (size_t i = 0; i < offsets.size(); ++i) {
    transform[i] = Read(base, matrix + offsets[i]);
    const auto dimension = i % 2 == 0 ? source[2] : source[3];
    Write(base, matrix + offsets[i], std::bit_cast<uint32_t>(
        std::bit_cast<float>(transform[i]) * float(dimension) / float(dimension - 2)));
  }
  Write(base, rect, source[0] + 1); Write(base, rect + 4, source[1] + 1);
  Write(base, rect + 8, source[2] - 2); Write(base, rect + 12, source[3] - 2);
  __imp__sub_8227C240(ctx, base);
  for (size_t i = 0; i < source.size(); ++i) Write(base, rect + uint32_t(i) * 4, source[i]);
  for (size_t i = 0; i < offsets.size(); ++i) Write(base, matrix + offsets[i], transform[i]);
  return true;
}
REX_HOOK_RAW(sub_821E1560) {
  const auto graphics = ctx.r4.u32;
  const auto tint = base[graphics + 0x48];
  // DrawReanimatorFrame already copied this tint into mColorOverride.
  // Reanimation would multiply it by Graphics tint again.
  if (drawing_seed_frame) base[graphics + 0x48] = 0;
  __imp__sub_821E1560(ctx, base);
  base[graphics + 0x48] = tint;
}
REX_HOOK_RAW(sub_821DEFC0) {
  if (ctx.r7.u32 != 0 || ctx.r8.u32 != 0 || !REXCVAR_GET(pvz_peashooter_direct_draw)) {
    __imp__sub_821DEFC0(ctx, base); return;
  }
  // The cached frame can contain unrelated pixels on its right edge. Use
  // the same idle body/head layers as MakeCachedPlantFrame, at their native
  // origin. Graphics retains the caller's scale, clip, color and opacity.
  const auto graphics = ctx.r4.u32;
  const auto type = __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + 0x82812B58));
  const auto previous = drawing_seed_frame; drawing_seed_frame = true;
  for (const auto track : {0x820B5C58u, 0x820BA55Cu}) {
    PPCContext work = ctx; work.r1.u32 -= 0x500;
    work.r3.u32 = graphics; work.r6.u32 = type; work.r7.u32 = track; work.r8.u32 = 0;
    sub_821DE058(work, base);
  }
  drawing_seed_frame = previous;
}
