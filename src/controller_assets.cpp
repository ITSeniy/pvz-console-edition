#include "controller_assets.h"
#include "mouse_bridge.h"
#include "input_backend.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <rex/system/kernel_state.h>
#include <atomic>
#include <array>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <chrono>

REXCVAR_DECLARE(std::string, pvz_cursor_mode);
REXCVAR_DEFINE_STRING(pvz_controller_glyphs, "auto", "PvZ", "Controller glyphs: auto, xbox or playstation")
    .allowed({"auto", "xbox", "playstation"});
REXCVAR_DEFINE_BOOL(pvz_asset_audit, false, "Diagnostics", "Dump native strings and seed textures for inspection");
REX_EXTERN(__imp__sub_824B2D20); // PakInterface::ReadFile(path, data**, size*)
REX_EXTERN(sub_82480430); // SexyAppBase::LoadPTXImage(path)
REX_EXTERN(sub_821264E0); // std::string constructor from ASCII
REX_EXTERN(sub_8219F4D0); // std::string destructor

namespace {
std::atomic_bool sony_controller{false};
uint32_t Read(uint8_t* base, uint32_t at) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + at)); }
void Write(uint8_t* base, uint32_t at, uint32_t value) { *reinterpret_cast<uint32_t*>(base + at) = __builtin_bswap32(value); }
struct Asset { uint32_t global; const char* name; uint32_t original = 0, sony = 0; bool attempted = false; };
std::array assets{
    Asset{0x82D0AA18, "help_buttons"}, Asset{0x82D0A6D0, "help_buttons_small"},
    Asset{0x82D0A808, "gamepad0"}, Asset{0x82D0A7C4, "gamepad1"},
    Asset{0x82D09E1C, "help_menu_image_controller"}, Asset{0x82D0A598, "help_menu_image_vs_controllers"}};
std::array<uint32_t, 2> empty_atlases{};
bool empty_attempted = false;
uint32_t mouse_rmb = 0;
bool mouse_rmb_attempted = false;
// How to Play "Controls": our mouse illustrations (resources/mouse, shipped
// next to the EXE) replace the controller while the mouse owns input.
constexpr uint32_t kHelpControllerImage = 0x82D09E1C;
std::array<uint32_t, 2> mouse_help{}; // PC, Console
bool mouse_help_attempted = false;
// VS Pick Sides: the mouse player's icon replaces its controller (gamepad0
// for player 1, gamepad1 for player 2) while a local match is set up.
std::array<uint32_t, 2> mouse_players{};
bool mouse_players_attempted = false;
bool MouseHelpImage(uint32_t image) {
  return image && (image == mouse_help[0] || image == mouse_help[1] || image == mouse_players[0] || image == mouse_players[1]);
}

uint32_t LoadImage(PPCContext& ctx, uint8_t* base, uint32_t app, std::string_view name) {
  const auto chars = ctx.r1.u32 - 0x180, string = ctx.r1.u32 - 0x80;
  const auto path = std::string("pc/dualshock/") + std::string(name);
  std::copy(path.begin(), path.end(), base + chars); base[chars + path.size()] = 0;
  PPCContext work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = string; work.r4.u32 = chars;
  sub_821264E0(work, base);
  work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = app; work.r4.u32 = string;
  sub_82480430(work, base); const auto result = work.r3.u32;
  work = ctx; work.r1.u32 -= 0x500; work.r3.u32 = string; sub_8219F4D0(work, base);
  return result;
}
}

void pvz::PublishSonyController(bool sony) { sony_controller.store(sony); }

REX_HOOK_RAW(sub_824B2D20) {
  // Only our converted assets bypass the packed-file index. All original
  // resources keep the native packed reader and decompression behavior.
  std::string path;
  for (uint32_t i = 0; i < 256 && base[ctx.r4.u32 + i]; ++i) path += char(base[ctx.r4.u32 + i]);
  std::replace(path.begin(), path.end(), '\\', '/');
  constexpr std::string_view prefix = "pc/dualshock/";
  const auto name = path.starts_with(prefix) ? path.substr(prefix.size()) : std::string{};
  const bool known = std::any_of(assets.begin(), assets.end(), [&](const auto& a) { return name == std::string(a.name) + ".ptx"; }) ||
      name == "a_button.ptx" || name == "help_buttons_empty.ptx" || name == "help_buttons_small_empty.ptx" || name == "mouse_rmb.ptx" ||
      name == "help_menu_image_mouse_pc.ptx" || name == "help_menu_image_mouse_console.ptx" ||
      name == "vs_mouse_player1.ptx" || name == "vs_mouse_player2.ptx";
  if (!known) {
    const auto out = ctx.r5.u32, length = ctx.r6.u32;
    __imp__sub_824B2D20(ctx, base);
    if (REXCVAR_GET(pvz_asset_audit) && ctx.r3.u32 &&
        (path == "images/seeds.ptx" || path == "images/SeedPacket_Larger.ptx")) {
      std::filesystem::create_directories("runtime-data/diagnostics");
      std::ofstream file(std::filesystem::path("runtime-data/diagnostics") / std::filesystem::path(path).filename(), std::ios::binary);
      const auto data = Read(base, out), size = Read(base, length);
      const auto* pixels = rex::system::kernel_memory()->TranslateVirtual<const char*>(data);
      file.write(pixels, size);
      REXLOG_INFO("PvZ texture audit {} bytes {} data {:08X} footer {:08X} {:08X} {:08X} {:08X}", path, size, data,
          __builtin_bswap32(*reinterpret_cast<const uint32_t*>(pixels + size - 16)),
          __builtin_bswap32(*reinterpret_cast<const uint32_t*>(pixels + size - 12)),
          __builtin_bswap32(*reinterpret_cast<const uint32_t*>(pixels + size - 8)),
          __builtin_bswap32(*reinterpret_cast<const uint32_t*>(pixels + size - 4)));
    }
    return;
  }
  const auto out = ctx.r5.u32, length = ctx.r6.u32;
  // Our own art ships with the EXE; game-derived assets stay in game data.
  const auto source = name.starts_with("help_menu_image_mouse_") || name.starts_with("vs_mouse_player") ?
      rex::filesystem::GetExecutableFolder() / "mouse" / name :
      std::filesystem::path(rex::cvar::Query<std::string>("game_data_root")) / path;
  std::ifstream file(source, std::ios::binary | std::ios::ate);
  const auto size = file ? std::streamoff(file.tellg()) : 0;
  if (size <= 16 || size > 2 * 1024 * 1024) { Write(base, out, 0); Write(base, length, 0); ctx.r3.u64 = 0; return; }
  // XG texture headers reference physical pages. Packed PTX data normally
  // lives in the physical mapping of main.pak; ordinary guest malloc cannot
  // be used here, because the GPU would read unrelated physical memory.
  auto* memory = rex::system::kernel_memory();
  const auto data = memory->SystemHeapAlloc(uint32_t(size), 0x1000, rex::memory::kSystemHeapPhysical);
  if (!data) { Write(base, out, 0); Write(base, length, 0); ctx.r3.u64 = 0; return; }
  file.seekg(0); file.read(memory->TranslateVirtual<char*>(data), size);
  if (!file) {
    memory->SystemHeapFree(data);
    Write(base, out, 0); Write(base, length, 0); ctx.r3.u64 = 0; return;
  }
  Write(base, out, data); Write(base, length, uint32_t(size)); ctx.r3.u64 = 1;
}

uint32_t pvz::ResolveControllerImage(uint32_t image, uint8_t* base) {
  if (!image) return 0;
  // Leaderboard tombstones use a reanimation image, outside the font atlas.
  if (pvz::MouseOwnsInput()) {
    const auto size = Read(base, image + 0x1C), capacity = Read(base, image + 0x20);
    const auto data = capacity < 16 ? image + 0xC : Read(base, image + 0xC);
    if (data && size > 0 && size < 128) {
      std::string name(reinterpret_cast<const char*>(base + data), size);
      std::replace(name.begin(), name.end(), '\\', '/');
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
      if (name == "reanim/a_button" || name == "reanim/a_button.ptx" ||
          name == "reanim/x_button" || name == "reanim/x_button.ptx" ||
          name == "images/survival_button" || name == "images/survival_button.ptx") return 0;
    }
  }
  const auto width = Read(base, image + 0x24), height = Read(base, image + 0x28);
  if (!((width == 546 && height == 42) || (width == 273 && height == 21) ||
        (width == 170 && height == 122) || (width == 384 && height == 293) ||
        (width == 119 && height == 119))) return image;
  std::string image_name;
  const auto size = Read(base, image + 0x1C), capacity = Read(base, image + 0x20);
  if (size && size <= 256) {
    const auto data = capacity < 16 ? image + 0xC : Read(base, image + 0xC);
    if (data) image_name.assign(reinterpret_cast<const char*>(base + data), size);
    std::replace(image_name.begin(), image_name.end(), '\\', '/');
    std::transform(image_name.begin(), image_name.end(), image_name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  }
  if (MouseHelpImage(image)) return image;
  for (size_t i = 0; i < assets.size(); ++i) {
    const auto& asset = assets[i];
    const bool named = image_name == std::string("images/") + asset.name ||
        image_name == std::string("images/") + asset.name + ".ptx";
    if (!named && image != asset.original && image != asset.sony &&
        !(i < empty_atlases.size() && image == empty_atlases[i])) continue;
    // Font render commands cache their atlas pointer. Resolve it at drawing
    // time too, so a previously cached B or A changes with the input owner.
    if (i < empty_atlases.size() && Read(base, i ? 0x828923D0 : 0x828923CC) == empty_atlases[i] &&
        pvz::MouseOwnsInput() && !pvz::SecondPlayerPromptOpen()) return 0;
    return Read(base, asset.global);
  }
  return image;
}

void pvz::UpdateControllerAssets(PPCContext& ctx, uint8_t* base) {
  const auto mode = REXCVAR_GET(pvz_controller_glyphs);
  const bool sony_mode = mode == "playstation" || (mode == "auto" && sony_controller.load());
  const auto app = Read(base, 0x82D0BC88);
  if (!app || !Read(base, 0x82D0AA18)) return;
  if (pvz::MouseOwnsInput() && !mouse_rmb_attempted) {
    mouse_rmb_attempted = true; mouse_rmb = LoadImage(ctx, base, app, "mouse_rmb");
    REXLOG_INFO("PvZ mouse RMB glyph loaded {}", bool(mouse_rmb));
  }
  if (pvz::MouseOwnsInput() && !mouse_help_attempted) {
    mouse_help_attempted = true;
    const auto original = Read(base, kHelpControllerImage);
    for (size_t i = 0; i < mouse_help.size(); ++i) {
      mouse_help[i] = LoadImage(ctx, base, app, i ? "help_menu_image_mouse_console" : "help_menu_image_mouse_pc");
      if (mouse_help[i] && original) {
        Write(base, mouse_help[i] + 0x2C, Read(base, original + 0x2C));
        Write(base, mouse_help[i] + 0x30, Read(base, original + 0x30));
      }
    }
    REXLOG_INFO("PvZ mouse help images loaded {} {}", bool(mouse_help[0]), bool(mouse_help[1]));
  }
  // Select Second Player asks for A on any controller: its inline glyphs stay
  // visible with the mouse and alternate Xbox / PlayStation once a second.
  const bool prompt = pvz::SecondPlayerPromptOpen();
  const bool cross = prompt && (std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count() & 1);
  const int mouse_player = pvz::LocalMouseAssigned() ? pvz::MouseUserIndex() : -1;
  if (mouse_player >= 0 && !mouse_players_attempted) {
    mouse_players_attempted = true;
    for (size_t i = 0; i < mouse_players.size(); ++i) {
      mouse_players[i] = LoadImage(ctx, base, app, i ? "vs_mouse_player2" : "vs_mouse_player1");
      if (mouse_players[i] && Read(base, assets[2 + i].global)) {
        Write(base, mouse_players[i] + 0x2C, Read(base, Read(base, assets[2 + i].global) + 0x2C));
        Write(base, mouse_players[i] + 0x30, Read(base, Read(base, assets[2 + i].global) + 0x30));
      }
    }
    REXLOG_INFO("PvZ VS mouse player icons loaded {} {}", bool(mouse_players[0]), bool(mouse_players[1]));
  }
  for (size_t index = 0; index < assets.size(); ++index) {
    auto& asset = assets[index];
    const auto current = Read(base, asset.global);
    if (!current) continue;
    if (current != asset.sony && !MouseHelpImage(current)) asset.original = current;
    const bool glyphs = index < empty_atlases.size();
    // VS player icons show each local player's own pad.
    bool own = sony_mode;
    if ((index == 2 || index == 3) && mode == "auto") {
      const auto pad = pvz::LocalPlayerPad(int(index - 2));
      if (pad == PadKind::Xbox || pad == PadKind::PlayStation) own = pad == PadKind::PlayStation;
    }
    const bool sony = prompt && glyphs ? cross : own;
    if ((sony || (prompt && glyphs)) && !asset.attempted) {
      asset.attempted = true;
      asset.sony = LoadImage(ctx, base, app, asset.name);
      if (asset.sony) {
        Write(base, asset.sony + 0x2C, Read(base, asset.original + 0x2C));
        Write(base, asset.sony + 0x30, Read(base, asset.original + 0x30));
        REXLOG_INFO("PvZ PlayStation asset {} loaded: {}x{} (Xbox {}x{}), pointers {:08X}/{:08X}", asset.name,
            Read(base, asset.sony + 0x24), Read(base, asset.sony + 0x28),
            Read(base, asset.original + 0x24), Read(base, asset.original + 0x28), asset.original, asset.sony);
      } else REXLOG_ERROR("PvZ could not load PlayStation asset {}", asset.name);
    }
    const bool mouse_icon = (index == 2 || index == 3) && mouse_player == int(index - 2) && mouse_players[index - 2];
    Write(base, asset.global, mouse_icon ? mouse_players[index - 2] : sony && asset.sony ? asset.sony : asset.original);
  }
  // Inline font glyphs also draw the B badge on the shovel. Transparent
  // atlases preserve layout while mouse owns input; gamepad restores them.
  if (pvz::MouseOwnsInput() && !empty_attempted) {
    empty_attempted = true;
    for (size_t i = 0; i < empty_atlases.size(); ++i) {
      empty_atlases[i] = LoadImage(ctx, base, app, i ? "help_buttons_small_empty" : "help_buttons_empty");
      if (empty_atlases[i]) {
        Write(base, empty_atlases[i] + 0x2C, Read(base, assets[i].original + 0x2C));
        Write(base, empty_atlases[i] + 0x30, Read(base, assets[i].original + 0x30));
      }
    }
  }
  const auto mouse_image = mouse_help[REXCVAR_GET(pvz_cursor_mode) == "pc" ? 0 : 1];
  if (pvz::MouseOwnsMenus() && mouse_image) Write(base, kHelpControllerImage, mouse_image);
  // Local multiplayer with a pad player: its glyphs (menu hints, the tool
  // badges on the lawn) stay visible.
  const bool pad_player = pvz::LocalPlayersActive() &&
      (pvz::LocalPlayerPad(0) == PadKind::Xbox || pvz::LocalPlayerPad(0) == PadKind::PlayStation ||
       pvz::LocalPlayerPad(1) == PadKind::Xbox || pvz::LocalPlayerPad(1) == PadKind::PlayStation);
  const bool hide = pvz::MouseOwnsMenus() && !prompt && !pad_player;
  Write(base, 0x828923CC, hide && empty_atlases[0] ? empty_atlases[0] : Read(base, 0x82D0AA18));
  Write(base, 0x828923D0, hide && empty_atlases[1] ? empty_atlases[1] : Read(base, 0x82D0A6D0));
}
uint32_t pvz::MouseRmbImage() { return mouse_rmb; }

// Select Second Player draws its inline glyphs one by one from the button
// atlas (cel 0 A, cel 1 B). Only a glyph whose controller is not known yet
// alternates Xbox / PlayStation: A until player 2 has joined. B (cancel)
// belongs to player 1, and after the join every glyph shows the pads in use.
uint32_t pvz::ResolvePromptGlyph(uint32_t image, uint32_t rect, uint8_t* base) {
  if (!image || !rect || !pvz::SecondPlayerPromptOpen()) return image;
  int atlas = -1;
  for (int i = 0; i < 2; ++i)
    if (image == assets[i].original || image == assets[i].sony) atlas = i;
  if (atlas < 0 || !assets[atlas].sony) return image;
  const int32_t width = int32_t(Read(base, rect + 8));
  const int cel = width > 0 ? int32_t(Read(base, rect)) / width : -1;
  const auto mode = REXCVAR_GET(pvz_controller_glyphs);
  auto kind = [&](int user) {
    const auto pad = pvz::LocalPlayerPad(user);
    if (pad != PadKind::Xbox && pad != PadKind::PlayStation) return -1;
    return mode == "playstation" ? 1 : mode == "xbox" ? 0 : pad == PadKind::PlayStation ? 1 : 0;
  };
  const int first = kind(0), second = kind(1);
  // LawnApp +AB4: the signed-in second user, -1 until player 2 has joined.
  const auto app = Read(base, 0x82D0BC88);
  const bool joined = app && int32_t(Read(base, app + 0xAB4)) != -1;
  int sony = cel == 1 ? (first >= 0 ? first : second) : joined ? (second >= 0 ? second : first) : -1;
  if (sony < 0) sony = int(std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count() & 1);
  return sony ? assets[atlas].sony : assets[atlas].original;
}

// The atlas renderer bypasses Graphics image hooks. Adapt only the temporary
// transform used by DrawTrack; animation definitions and hit testing are intact.
REX_EXTERN(__imp__sub_821E0068);
REX_HOOK_RAW(sub_821E0068) {
  const auto reanimation = ctx.r3.u32, transform = ctx.r5.u32;
  const bool drawing = ctx.lr == 0x821E08B0 && pvz::MouseOwnsMenus();
  __imp__sub_821E0068(ctx, base);
  if (!drawing) return;
  auto image = Read(base, transform + 0x20);
  const auto definition = Read(base, reanimation + 0x28);
  const auto atlas = definition ? Read(base, definition + 0xC) : 0;
  if (image && image <= 1000 && atlas) image = Read(base, atlas + (image - 1) * 20 + 0x10);
  if (image > 1000 && !pvz::ResolveControllerImage(image, base))
    Write(base, transform + 0x18, 0xBF800000); // invisible frame, render-only
}
