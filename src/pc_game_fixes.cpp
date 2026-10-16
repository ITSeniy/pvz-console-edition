#include "mouse_bridge.h"
#include "pc_text.h"
#include "input_backend.h"
#include "pc_text_runtime.h"
#include "bonus_games.h"
#include "game_settings.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include "guest_call.h"
#include <rex/ppc/func.h>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <bit>
#include <array>
#include <algorithm>
#include <chrono>
#include <string>

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
  return {Binding("keybind_start")};
}
std::mutex advice_mutex;
std::unordered_map<std::u16string, std::u16string> advice_keys;
// LawnStrings of the running language, as the game loads them.
std::mutex strings_mutex;
std::unordered_map<std::u16string, std::u16string> game_strings;
// "[KEY]" lines open an entry; its value runs to the next one.
void ParseGameStrings(std::u16string_view text) {
  std::lock_guard lock(strings_mutex);
  std::u16string key, value;
  const auto flush = [&] {
    while (!value.empty() && (value.back() == u'\n' || value.back() == u' ')) value.pop_back();
    if (!key.empty()) game_strings[key] = value;
    value.clear();
  };
  for (size_t at = 0; at < text.size();) {
    auto end = text.find(u'\n', at);
    if (end == std::u16string_view::npos) end = text.size();
    auto line = text.substr(at, end - at);
    if (line.ends_with(u'\r')) line.remove_suffix(1);
    at = end + 1;
    if (line.size() > 2 && line.front() == u'[' && line.back() == u']') {
      flush(); key = line.substr(1, line.size() - 2); continue;
    }
    if (!key.empty() && (!value.empty() || !line.empty())) value.append(line).push_back(u'\n');
  }
  flush();
}
}

std::u16string pvz::GameString(std::u16string_view key) {
  std::lock_guard lock(strings_mutex);
  const auto found = game_strings.find(std::u16string(key));
  return found != game_strings.end() ? found->second : std::u16string{};
}
std::u16string pvz::PortText(const Localized& text, std::u16string_view key) {
  const auto language = RunningLanguage();
  if (const auto own = text.In(language); !own.empty()) return std::u16string(own);
  if (language != Language::En && !key.empty())
    if (auto game = GameString(key); !game.empty()) return game;
  return std::u16string(text.en);
}

std::u16string pvz::AdaptMouseMessage(std::u16string text, bool pc) {
  std::u16string key;
  { std::lock_guard lock(advice_mutex);
    if (auto found = advice_keys.find(text); found != advice_keys.end()) key = found->second;
  }
  return PcGameText(key, std::move(text), true, pc, Bindings(), RunningLanguage());
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
  bool mouse = pvz::MouseOwnsMenus();
  // Select Second Player addresses player 1's device, whoever clicked last.
  if (key.starts_with(u"2P_") && pvz::LocalPlayersActive()) mouse = pvz::MouseUserIndex() == 0;
  if (mouse) bindings = Bindings();
  auto adapted = pvz::PcGameText(key, original, mouse,
      rex::cvar::Query<std::string>("pvz_cursor_mode") == "pc", bindings, pvz::RunningLanguage());
  if (key == u"PICK_CHALLENGE" && pvz::DrawingBonusPage()) adapted = pvz::PortText(pvz::kBonusGamesText);
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
  // The game's string table loader; r3 holds the whole UTF-16 LawnStrings.
  const auto text = ctx.r3.u32;
  uint32_t size = 0;
  while (size < 2 * 1024 * 1024 && (base[text + size] || base[text + size + 1])) size += 2;
  std::u16string strings(size / 2, char16_t(0));
  for (uint32_t i = 0; i < size / 2; ++i)
    strings[i] = char16_t((unsigned(base[text + i * 2]) << 8) | base[text + i * 2 + 1]);
  ParseGameStrings(strings);
  if (rex::cvar::Query<bool>("pvz_asset_audit")) {
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

REX_EXTERN(__imp__sub_821A5DB8); // LawnApp::Update
REX_EXTERN(sub_82139DE8);        // Board::Pause(bool)
using pvz::GuestCall;
REX_HOOK_RAW(sub_821A5DB8) {
  const auto app = ctx.r3.u32;
  __imp__sub_821A5DB8(ctx, base);
  // Exit Game saves the board (+AAE pending) behind the Saving dialog (59).
  // This update finishes the exit once the save is idle, but only while
  // dialog 59 still exists. Host saves complete instantly and the dialog
  // can already be gone, which left Exit Game waiting forever. Finish the
  // same way: clear +AAE and, without other dialogs, unpause the Board, whose
  // Update then returns to the menu.
  if (!base[app + 0xAAE]) return;
  const auto status = Read(base, app + 0xA54);
  if (!status || (base[status + 0xC59] && Read(base, status) != 0)) return; // still saving
  const auto error = Read(base, status + 0xC5C);
  if (error == 2 || error == 6) return; // native error path handles these
  auto dialog = [&](uint32_t id) {
    return GuestCall(ctx, base, rex::runtime::ResolveIndirectFunction(Read(base, Read(base, app) + 0x130)), app, id);
  };
  if (dialog(59)) return; // native path still owns it
  base[app + 0xAAE] = 0;
  const auto board = Read(base, app + 0x900);
  if (!board || dialog(55) || dialog(49) || dialog(48)) return;
  if (const auto pending = Read(base, status + 0xC14)) {
    GuestCall(ctx, base, rex::runtime::ResolveIndirectFunction(Read(base, Read(base, pending))), pending, 1);
    Write(base, status + 0xC14, 0);
  }
  GuestCall(ctx, base, sub_82139DE8, board, 0);
  REXLOG_INFO("PvZ board save finished without its dialog: exit continues");
}

// Looping foley across a pause. TodFoley::GamePause(true) (8227FCA0) calls
// the sound instance's Stop, which on Xbox also releases its voice (+10), so
// GamePause(false) cannot restart it: the rain of It's Raining Seeds and
// Dark Stormy Night stayed silent after any pause, including the automatic
// pause when the window is minimized. Start such a loop again on resume.
REX_EXTERN(__imp__sub_8227FCA0); // TodFoley::GamePause(bool)
REX_EXTERN(__imp__sub_8227FC10); // TodFoley::StopFoley(type)
REX_EXTERN(sub_821A8258);        // LawnApp::PlayFoley(type)
namespace {
constexpr uint32_t kFoleyParams = 0x82D0BCC8, kFoleyCount = 0x82D0BCCC;
constexpr uint32_t kFoleyTypeSize = 164, kFoleyInstanceSize = 20, kFoleyInstances = 8;
std::vector<uint32_t> paused_loops;
bool LoopingOnPause(uint8_t* base, uint32_t type) {
  const auto flags = Read(base, Read(base, kFoleyParams) + type * 52 + 48);
  return (flags & 1) && (flags & 4); // FOLEYFLAGS_LOOP, FOLEYFLAGS_MUTE_ON_PAUSE
}
bool FoleyActive(uint8_t* base, uint32_t foley, uint32_t type, bool voice) {
  for (uint32_t i = 0; i < kFoleyInstances; ++i) {
    const auto at = foley + type * kFoleyTypeSize + i * kFoleyInstanceSize;
    const auto instance = Read(base, at);
    if (Read(base, at + 4) && instance && (!voice || Read(base, instance + 16))) return true;
  }
  return false;
}
}
REX_HOOK_RAW(sub_8227FCA0) {
  const auto foley = ctx.r3.u32;
  const bool pausing = (ctx.r4.u32 & 0xFF) != 0;
  const auto types = std::min<uint32_t>(Read(base, kFoleyCount), 256);
  if (pausing) {
    paused_loops.clear();
    for (uint32_t type = 0; type < types; ++type)
      if (LoopingOnPause(base, type) && FoleyActive(base, foley, type, true)) paused_loops.push_back(type);
  }
  __imp__sub_8227FCA0(ctx, base);
  if (pausing) return;
  const auto app = Read(base, 0x82D0BC88);
  for (const auto type : std::exchange(paused_loops, {}))
    if (app && Read(base, app + 2360) == foley && !FoleyActive(base, foley, type, true)) {
      GuestCall(ctx, base, sub_821A8258, app, type);
      REXLOG_INFO("PvZ looping foley {} restarted after pause", type);
    }
}
REX_HOOK_RAW(sub_8227FC10) {
  // A loop stopped during the pause (level exit) must not come back.
  std::erase(paused_loops, ctx.r4.u32);
  __imp__sub_8227FC10(ctx, base);
}

