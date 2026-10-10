#include "mouse_bridge.h"
#include "pc_text.h"
#include "pc_text_runtime.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <mutex>

namespace {
uint32_t Read(uint8_t* base, uint32_t at) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + at)); }
void Write(uint8_t* base, uint32_t at, uint32_t value) { *reinterpret_cast<uint32_t*>(base + at) = __builtin_bswap32(value); }
std::u16string Wide(uint8_t* base, uint32_t at) {
  const auto count = Read(base, at + 0x14);
  if (count > 8192) return {};
  const auto chars = Read(base, at + 0x18) >= 8 ? Read(base, at + 4) : at + 4;
  std::u16string out;
  for (uint32_t i = 0; i < count; ++i) out += char16_t((unsigned(base[chars + i * 2]) << 8) | base[chars + i * 2 + 1]);
  return out;
}

std::u16string Binding(const char* name) {
  auto value = rex::cvar::Query<std::string>(name);
  if (const auto comma = value.find(','); comma != std::string::npos) value.resize(comma);
  return {value.begin(), value.end()};
}
pvz::TextBindings Bindings() {
  return {Binding("keybind_x"), Binding("keybind_y"), Binding("keybind_start"),
      Binding("keybind_left_shoulder"), Binding("keybind_right_shoulder"),
      Binding("keybind_left_trigger"), Binding("keybind_right_trigger")};
}
std::mutex advice_mutex;
std::unordered_map<std::u16string, std::u16string> advice_keys;
}

std::u16string pvz::AdaptMouseMessage(std::u16string text, bool pc) {
  std::u16string key;
  { std::lock_guard lock(advice_mutex);
    if (auto found = advice_keys.find(text); found != advice_keys.end()) key = found->second;
  }
  return PcGameText(key, std::move(text), true, pc, Bindings());
}

REX_EXTERN(__imp__sub_82284C60);
REX_EXTERN(sub_821258D8); // UTF-16 assign from pointer and length
REX_HOOK_RAW(sub_82284C60) {
  const auto result = ctx.r3.u32;
  const auto key = Wide(base, ctx.r4.u32);
  __imp__sub_82284C60(ctx, base);
  const auto original = Wide(base, result);
  if (key.starts_with(u"ADVICE_")) {
    // These labels are copied into MessageWidget's fixed buffer. Keep their
    // controller text there and adapt at drawing, so live mode switches work.
    std::lock_guard lock(advice_mutex); advice_keys[original] = key;
    return;
  }
  pvz::TextBindings bindings;
  const bool mouse = pvz::MouseOwnsInput();
  if (mouse) bindings = Bindings();
  const auto adapted = pvz::PcGameText(key, original, mouse,
      rex::cvar::Query<std::string>("pvz_cursor_mode") == "pc", bindings);
  if (adapted == original || adapted.size() > 8192) return;
  // Assign the returned copy, never the localization table entry. A new lookup
  // can therefore restore controller markup after an input-owner switch.
  const auto chars = (ctx.r1.u32 - uint32_t(adapted.size() * 2 + 0x200)) & ~15u;
  for (size_t i = 0; i <= adapted.size(); ++i) {
    const auto ch = i < adapted.size() ? adapted[i] : char16_t(0);
    base[chars + i * 2] = uint8_t(ch >> 8); base[chars + i * 2 + 1] = uint8_t(ch);
  }
  PPCContext work = ctx; work.r1.u32 = chars - 0x500;
  work.r3.u32 = result; work.r4.u32 = chars; work.r5.u32 = uint32_t(adapted.size());
  sub_821258D8(work, base);
}

REX_EXTERN(__imp__sub_82285B70);
REX_HOOK_RAW(sub_82285B70) {
  // DrawCrazyDave's text rectangle reserves a row for the console prompt.
  // With that row hidden, center the text 12 native pixels lower in the bubble.
  const bool lower = pvz::MouseOwnsInput() && ctx.lr == 0x821AB0B0;
  const auto rect = ctx.r5.u32, y = lower ? Read(base, rect + 4) : 0;
  if (lower) Write(base, rect + 4, y + 12);
  __imp__sub_82285B70(ctx, base);
  if (lower) Write(base, rect + 4, y);
}

REX_EXTERN(__imp__sub_82284848);
REX_HOOK_RAW(sub_82284848) {
  if (rex::cvar::Query<bool>("pvz_asset_audit")) {
    const auto text = ctx.r3.u32;
    uint32_t size = 0;
    while (size < 2 * 1024 * 1024 && (base[text + size] || base[text + size + 1])) size += 2;
    std::filesystem::create_directories("runtime-data/diagnostics");
    std::ofstream out("runtime-data/diagnostics/lawnstrings-native.bin", std::ios::binary);
    out.write(reinterpret_cast<const char*>(base + text), size);
    REXLOG_INFO("PvZ text audit dumped {} native string bytes", size);
  }
  __imp__sub_82284848(ctx, base);
}

REX_EXTERN(__imp__sub_82139DE8); // Board::Pause(bool)
REX_HOOK_RAW(sub_82139DE8) {
  // Pausing shows the coin bank for 1000 frames. Keep it to coin pickups:
  // a bank already fading in from a pickup survives the pause.
  const auto board = ctx.r3.u32;
  const bool pause = (ctx.r4.u32 & 0xFF) != 0;
  const auto coin_bank = Read(base, board + 0x5694);
  __imp__sub_82139DE8(ctx, base);
  if (pause) Write(base, board + 0x5694, coin_bank);
}
