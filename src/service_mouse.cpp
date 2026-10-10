#include "service_mouse.h"
#include "mouse_bridge.h"
#include "input_backend.h"
#include "guest_memory.h"
#include "board_mapping.h"
#include "game_settings.h"
#include "input_widget_inventory.h"
#include "guest_call.h"
#include "guest_layout.h"
#include "input_trace.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <bit>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>

REX_EXTERN(sub_822142E0); // Store::GetStoreItemType(slot)
REX_EXTERN(sub_82214010); // Store icon/position(slot,item,image*,x*,y*,frame*)
REX_EXTERN(sub_82214800); // Store::IsItemUnavailable
REX_EXTERN(sub_822160A0); // Store::UpdateMouse (native item speech)
REX_EXTERN(sub_82217888); // Store::GamepadButtonDown (prices and confirmation)
REX_EXTERN(__imp__sub_82217888);
REX_EXTERN(sub_8219ACD0); // HouseChooserDialog::GamepadButtonDown
REX_EXTERN(__imp__sub_8219ACD0);
REX_EXTERN(sub_82125150); // Almanac::MouseDown
REX_EXTERN(sub_82122BD8); // Almanac::SetPage
REX_EXTERN(sub_821252C0); // Almanac::MouseWheel (description scrollbar)
REX_EXTERN(__imp__sub_821249A8); // Almanac::Draw
REX_EXTERN(__imp__sub_82122DD0); // Almanac::Update
REX_EXTERN(__imp__sub_82123220); // Almanac::DrawPlants
REX_EXTERN(__imp__sub_82123CD8); // Almanac::DrawZombies
REX_EXTERN(__imp__sub_821A8CE8); // LawnApp::SeedTypeAvailable
REX_EXTERN(__imp__sub_82124D08); // Almanac::ZombieIsShown
REX_EXTERN(sub_82124A90); // Almanac::SeedHitTest
REX_EXTERN(sub_82124FC0); // Almanac::ZombieHitTest
REX_EXTERN(sub_8219A210); // HelpTextScreen page navigation
REX_EXTERN(sub_8219D480); // HouseMenu page navigation
REX_EXTERN(sub_821B9618); // Leaderboard row navigation
REX_EXTERN(sub_821BECE0); // MailScreen page navigation
REX_EXTERN(sub_8228B5C0); // VSSetupMenu side navigation
REX_EXTERN(sub_82191B10); // TreeOfWisdomControls::ButtonDown
REX_EXTERN(__imp__sub_82191DA8); // TreeOfWisdomControls::Update
REX_EXTERN(sub_82139808); // Board::PickUpTool(hit,player)
REX_EXTERN(__imp__sub_82139808);
REX_EXTERN(__imp__sub_8228CE18); // ZenGarden::InitLevel (also used by garden switches)
REX_EXTERN(sub_821388A0); // Board::MouseDownWithTool(x,y,type)
REX_EXTERN(sub_82139238); // Board::MouseHitTest
REX_EXTERN(sub_82130E50); // Board::GetToolRect base geometry
REX_EXTERN(sub_82131098); // Board::GetZenToolRect horizontal placement
REX_EXTERN(sub_8214C9F8); // Board::ToolVisible
REX_EXTERN(sub_82165788); // TreeOfWisdom::MouseHitTest
REX_EXTERN(sub_82165920); // TreeOfWisdom::ShouldShowTreeFood
REX_EXTERN(sub_82139AC0); // Board::ClearCursor
REX_EXTERN(sub_8216EF50); // Coin::Collect
REX_EXTERN(sub_8214CDC0); // Board::GamepadButtonDown (Dave/menu)
REX_EXTERN(sub_8217D798); // CutScene::AdvanceCrazyDaveDialog(skip)
REX_EXTERN(sub_82192490); // ZenGardenControls::GamepadButtonDown
REX_EXTERN(__imp__sub_82192080); // ZenGardenControls::Update
REX_EXTERN(__imp__sub_821923C0); // ZenGardenControls::Draw
REX_EXTERN(sub_821923C0);
REX_EXTERN(sub_821774C0); // CursorObject::Draw
REX_EXTERN(sub_8249C058); // Graphics::DrawImageStretch (native tool bank outline)
REX_EXTERN(__imp__sub_82189B40); // Controls::UpdateInput

using namespace pvz::layout;
using pvz::GuestCall;

namespace {
using pvz::guest::Float;
using pvz::guest::Read;
double ReadDouble(uint8_t* b, uint32_t a) { uint64_t v; std::memcpy(&v, b + a, 8); return std::bit_cast<double>(std::byteswap(v)); }
using pvz::guest::Write;

bool Pc() { return rex::cvar::Query<std::string>("pvz_cursor_mode") == "pc"; }
bool GardenScreen(uint8_t* b, uint32_t screen) {
  if (!screen || Read(b, screen) != kBoard) return false;
  const auto app = BoardApp(b, screen), controls = Read(b, screen + 0x1B0);
  return app && controls &&
      ((GameMode(b, app) == kModeZenGarden && Read(b, controls) == kZenControls) ||
       (GameMode(b, app) == kModeTreeOfWisdom && Read(b, controls) == kTreeControls));
}
bool Garden(uint8_t* b, uint32_t screen) {
  return GardenScreen(b, screen) && Read(b, Read(b, screen + 0x1B0) + 0xA0) == 0;
}
bool Tree(uint8_t* b, uint32_t board) { return Garden(b, board) && Read(b, Read(b, board + 0x1B0)) == kTreeControls; }
uint32_t ToolSlot(uint8_t* b, uint32_t board) { return Tree(b, board) ? 0xFC : 0xF8; }
struct Capture { uint32_t screen = 0; int page = -1, slot = -1; } capture;
uint32_t foreground = 0, garden = 0, shop = 0;
uint32_t garden_hover = 0;
uint64_t garden_hover_generation = 0;
std::optional<pvz::Point> position;
bool had_mouse = false, had_pc = false;
std::atomic_bool item_cursor{false};
thread_local bool drawing_cursor_overlay = false;
bool NavigationCursor(uint8_t* b, uint32_t board) {
  if (Pc()) return false;
  const auto cursor = Read(b, Read(b, board + 0x1B8) + 0x4C);
  return (garden_hover >= 15 && garden_hover <= 18) || (cursor >= 18 && cursor <= 21);
}
struct AlmanacGridSelection { uint32_t screen = 0, offset = 0, selected = 0; };
thread_local AlmanacGridSelection grid_selection;
void RestoreAlmanacSelection(uint8_t* b) {
  if (grid_selection.screen) Write(b, grid_selection.screen + grid_selection.offset, grid_selection.selected);
  grid_selection = {};
}
void ItemCursor(bool show) {
  if (item_cursor.exchange(show) != show) {
    pvz::RefreshHostSettings();
    PVZ_TRACE("PvZ garden item cursor {} mode {}", show, Pc() ? "pc" : "console");
  }
}
bool StoreReady(uint8_t* b, uint32_t s) {
  return int32_t(Read(b, s + 0x1AC)) >= 120 && !b[s + 0x1D0] &&
      int32_t(Read(b, s + 0x1E8)) <= 0 && !b[s + 0x205];
}
void HoverSound(PPCContext& ctx, uint8_t* b, uint32_t app) {
  const auto sound = Read(b, 0x82D0A4F8); // SOUND_BLEEP, same sample as native Store navigation.
  if (app && int32_t(sound) >= 0) {
    const auto instance = GuestCall(ctx, b, rex::runtime::ResolveIndirectFunction(Read(b, Read(b, app) + 0xE0)), app, sound, 1);
    PVZ_TRACE("PvZ service hover sound {} instance {:08X}", sound, instance);
  }
}
std::optional<pvz::Point> Local(uint8_t* b, uint32_t s, std::optional<pvz::Point> p) {
  auto local = p ? pvz::MapUiPointer(*p) : std::nullopt;
  if (local) { local->x -= int32_t(Read(b, s + 0x40)); local->y -= int32_t(Read(b, s + 0x44)); }
  return local;
}
int HouseHit(uint8_t* b, uint32_t s, std::optional<pvz::Point> p) {
  const auto local = Local(b, s, p);
  if (!local) return -1;
  const auto app = Read(b, s + 0x1A4), profile = app ? Read(b, app + 0x9FC) : 0;
  if (!profile) return -1;
  // HouseChooserDialog::Draw paints five icons at (75 + 90*i,125).
  // The default house is always available; others use the native purchases.
  constexpr std::array<uint32_t, 5> images{0x82D0A9F8, 0x82D0A210, 0x82D0A52C, 0x82D0A700, 0x82D0A7DC};
  for (int house = 0; house < 5; ++house) {
    if (house && !Read(b, profile + 0x430 + house * 4)) continue;
    const auto image = Read(b, images[house]);
    if (image && pvz::InRect(*local, 75 + house * 90, 125, Read(b, image + 0x24), Read(b, image + 0x28))) return house;
  }
  return -1;
}
int StoreHit(PPCContext& ctx, uint8_t* b, uint32_t s, std::optional<pvz::Point> p) {
  const auto local = Local(b, s, p);
  if (!local || !StoreReady(b, s)) return -1;
  const auto scratch = ctx.r1.u32 - 0x80;
  for (int slot = 0; slot < 8; ++slot) {
    const auto item = GuestCall(ctx, b, sub_822142E0, s, uint32_t(slot));
    if (int32_t(item) < 0 || GuestCall(ctx, b, sub_82214800, s, item)) continue;
    GuestCall(ctx, b, sub_82214010, uint32_t(slot), item, scratch, scratch + 4, scratch + 8, scratch + 12);
    // Native drawing anchors icons at their bottom centre, not top left.
    // Include the price tag without overlapping the 75px shelf slots.
    const int x = int32_t(Read(b, scratch + 4)) + int32_t(Read(b, s + 0x1F4));
    const int y = int32_t(Read(b, scratch + 8)) + int32_t(Read(b, s + 0x1F8));
    if (pvz::InRect(*local, x - 37, y - 85, 75, 110)) return slot;
  }
  return -1;
}
void GardenPointer(uint8_t* b, uint32_t controls) {
  const auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt;
  if (!local) return;
  Write(b, controls + 0x74, std::bit_cast<uint32_t>(local->x));
  Write(b, controls + 0x78, std::bit_cast<uint32_t>(local->y));
  Write(b, controls + 0x7C, 0); Write(b, controls + 0x80, 0);
  // Snap both current and target coordinates; Xbox normally interpolates
  // these for an analogue stick, which makes an absolute mouse trail behind.
  Write(b, controls + 0x58, std::bit_cast<uint32_t>(local->x));
  Write(b, controls + 0x5C, std::bit_cast<uint32_t>(local->y));
}
uint32_t GardenToolAt(PPCContext& ctx, uint8_t* b, uint32_t board, pvz::Point p) {
  const auto rect = ctx.r1.u32 - 0x70;
  for (uint32_t tool = 6; tool <= 18; ++tool) {
    if (!GuestCall(ctx, b, sub_8214C9F8, board, tool)) continue;
    if (tool == 14 && !GuestCall(ctx, b, sub_82165920, Read(b, board + 0x1D4))) continue;
    GuestCall(ctx, b, sub_82130E50, rect, board, tool);
    GuestCall(ctx, b, sub_82131098, rect, board, tool);
    if (pvz::InRect(p, int32_t(Read(b, rect)), int32_t(Read(b, rect + 4)),
        Read(b, rect + 8), Read(b, rect + 12))) return tool;
  }
  return 0;
}
void ClearGarden(PPCContext& ctx, uint8_t* b, uint32_t s) {
  GuestCall(ctx, b, sub_82139AC0, s, 0);
  // Zero means "choose the watering can on the next Update" in Xbox Zen.
  Write(b, Read(b, s + 0x1B0) + ToolSlot(b, s), uint32_t(-1));
}
void SwitchGarden(PPCContext& ctx, uint8_t* b, uint32_t board, pvz::Point p, uint32_t tool) {
  const auto app = BoardApp(b, board);
  GuestCall(ctx, b, sub_821388A0, board, uint32_t(int(p.x)), uint32_t(int(p.y)), tool);
  // A tree/garden transition can replace the Board. Clear the new board's
  // default tool instead of accessing the old cursor after its destruction.
  const auto live_board = Read(b, app + 0x900);
  if (Garden(b, live_board)) ClearGarden(ctx, b, live_board);
  garden_hover = 0; ItemCursor(false);
}
}

void pvz::CancelServiceMouse(PPCContext& ctx, uint8_t* b, uint32_t live_board) {
  capture = {}; position.reset(); garden_hover = 0; ItemCursor(false);
  if (had_pc && Garden(b, live_board)) ClearGarden(ctx, b, live_board);
}
bool pvz::ServiceItemCursor() { return item_cursor.load(); }
bool pvz::ServicePcGarden() { return garden && MouseOwnsInput() && Pc(); }
bool pvz::CancelCarriedGardenTool(PPCContext& ctx, uint8_t* b, uint32_t board) {
  if (!ServicePcGarden() || !Garden(b, board)) return false;
  const auto cursor = Read(b, board + 0x1B8);
  if (!cursor || !Read(b, cursor + 0x4C)) return false;
  ClearGarden(ctx, b, board); ItemCursor(false);
  return true;
}
void pvz::DrawServiceToolHover(PPCContext& ctx, uint8_t* b) {
  // Called just after the native bank background, before its tool icon.
  // Use draw parameters rather than altering the carried CursorType: the
  // latter also controls whether the glove icon is drawn while moving plants.
  if (!ServicePcGarden() || ctx.r20.u32 != foreground || ctx.r19.u32 != garden_hover || !garden_hover) return;
  const auto image = ctx.r4.u32;
  GuestCall(ctx, b, sub_8249C058, ctx.r3.u32, Read(b, 0x82D0A2F8), ctx.r5.u32, ctx.r6.u32,
      Read(b, image + 0x24), Read(b, image + 0x28));
  if (REXCVAR_GET(pvz_input_trace)) {
    static uint64_t last = uint64_t(-1);
    if (last != garden_hover_generation) REXLOG_INFO("PvZ PC garden toolbar hover painted tool {}", garden_hover);
    last = garden_hover_generation;
  }
}
void pvz::DrawServiceCursor(PPCContext& ctx, uint8_t* b, uint32_t board) {
  // Draw once after Board DrawAll, above buttons and garden labels.
  // Tree shares Zen's cursor layout but has an empty native Draw virtual.
  if (MouseOwnsInput() && board == foreground && Garden(b, board)) {
    drawing_cursor_overlay = true;
    GuestCall(ctx, b, sub_821923C0, Read(b, board + 0x1B0), ctx.r4.u32);
    drawing_cursor_overlay = false;
    if (REXCVAR_GET(pvz_input_trace) && position && Read(b, Read(b, board + 0x1B8) + 0x4C)) {
      const int area = pvz::InRect(*position, 160, 500, 80, 45) ? 1 :
          pvz::InRect(*position, 245, 500, 80, 45) ? 2 :
          pvz::InRect(*position, 370, 535, 60, 15) ? 3 : 0;
      static int previous = -1;
      const int state = area + (Pc() ? 0 : 4);
      if (area && state != previous)
        REXLOG_INFO("PvZ garden cursor overlay area {} mode {}", area, Pc() ? "pc" : "console");
      previous = state;
    }
  }
}
void pvz::UpdateServicePointer(PPCContext& ctx, uint8_t* b, uint32_t screen, std::optional<Point> p) {
  const bool mouse = MouseOwnsInput(), pc = mouse && Pc();
  if (screen && screen != foreground && REXCVAR_GET(pvz_input_trace))
    REXLOG_INFO("PvZ input screen {} vtable {:08X} down {:08X} wheel {:08X}",
        InputClassName(Read(b, screen)), Read(b, screen), Read(b, Read(b, screen) + 0xDC), Read(b, Read(b, screen) + 0xF0));
  if (screen != foreground || !mouse || !p) { capture = {}; garden_hover = 0; }
  if (pc != had_pc) garden_hover = 0;
  foreground = screen; position = mouse ? p : std::nullopt;
  const bool is_garden = Garden(b, screen);
  if (is_garden && pc && (garden != screen || !had_pc)) ClearGarden(ctx, b, screen);
  if (is_garden && !pc && (had_pc || (!mouse && had_mouse)) &&
      int32_t(Read(b, Read(b, screen + 0x1B0) + ToolSlot(b, screen))) < 0)
    Write(b, Read(b, screen + 0x1B0) + ToolSlot(b, screen), 0);
  garden = is_garden ? screen : 0;
  const auto local = mouse && p ? MapBoardPointer(*p) : std::nullopt;
  const bool garden_pointer = is_garden && mouse && local && !BoardPaused(b, screen) &&
      int32_t(Read(b, BoardApp(b, screen) + 0xA18)) < 3;
  const auto hovered = garden_pointer ? GardenToolAt(ctx, b, screen, *local) : 0;
  if (hovered != garden_hover) {
    if (pc && hovered) HoverSound(ctx, b, BoardApp(b, screen));
    PVZ_TRACE("PvZ garden toolbar hover {} -> {} mode {}",
        garden_hover, hovered, pc ? "pc" : "console");
    garden_hover = hovered;
    ++garden_hover_generation;
  }
  ItemCursor(garden_pointer && !NavigationCursor(b, screen) &&
      Read(b, Read(b, screen + 0x1B8) + 0x4C) != 0);
  if (screen && Read(b, screen) == kStoreScreen) {
    if (mouse) {
      const int slot = StoreHit(ctx, b, screen, p);
      const auto item = slot >= 0 ? GuestCall(ctx, b, sub_822142E0, screen, uint32_t(slot)) : uint32_t(-1);
      const auto previous = Read(b, screen + 0x1E4);
      if (slot >= 0) Write(b, screen + 0x1E0, uint32_t(slot));
      if (pc || slot >= 0) Write(b, screen + 0x1E4, item);
      if (slot >= 0 && item != previous) {
        Write(b, screen + 0x2A0, 0); GuestCall(ctx, b, sub_822160A0, screen);
        HoverSound(ctx, b, Read(b, screen + 0x1A4));
      }
      if (REXCVAR_GET(pvz_input_trace) && item != previous) REXLOG_INFO("PvZ Shop hover slot {} item {}", slot, int32_t(item));
    } else if (had_mouse && shop == screen) {
      Write(b, screen + 0x1E4, GuestCall(ctx, b, sub_822142E0, screen, Read(b, screen + 0x1E0)));
    }
    shop = screen;
  } else shop = 0;
  if (mouse && screen && Read(b, screen) == kHouseChooser) {
    const int house = HouseHit(b, screen, p);
    if (house >= 0 && Read(b, screen + 0x1C0) != uint32_t(house)) {
      Write(b, screen + 0x1C0, house); Write(b, screen + 0x1C4, 0);
      HoverSound(ctx, b, Read(b, screen + 0x1A4));
      PVZ_TRACE("PvZ Select House hover {}", house);
    }
  }
  if (screen && Read(b, screen) == kAlmanac) {
    const auto local = mouse ? Local(b, screen, p) : std::nullopt;
    for (int page = 1; page <= 2; ++page) {
      const auto button = Read(b, screen + 0x1C4 + page * 4);
      const bool over = local && !b[button + 0x72] && !b[button + 0x149] &&
          pvz::InRect(*local, int32_t(Read(b, button + 0x190)), int32_t(Read(b, button + 0x194)),
              Read(b, button + 0x1A4), Read(b, button + 0x1A8));
      if (over && !b[button + 0x75]) HoverSound(ctx, b, Read(b, screen + 0x1C4));
      b[button + 0x75] = over;
      if (mouse) b[button + 0xA2] = over;
    }
  }
  had_mouse = mouse; had_pc = pc;
}
void pvz::ScrollService(PPCContext& ctx, uint8_t* b, uint32_t screen, int steps) {
  if (!screen || !steps) return;
  const auto vt = Read(b, screen);
  if (vt == kHouseChooser) {
    capture = {};
    GuestCall(ctx, b, sub_8219ACD0, screen, steps > 0 ? 2u : 3u, 0, 0);
    return;
  }
  if (vt == kAlmanac) {
    const auto scrollbar = Read(b, screen + 0x1D0);
    const auto before = scrollbar ? ReadDouble(b, scrollbar + 0xE8) : 0.0;
    GuestCall(ctx, b, sub_821252C0, screen, uint32_t(steps));
    PVZ_TRACE("PvZ Almanac description wheel {} position {} -> {}", steps,
        before, scrollbar ? ReadDouble(b, scrollbar + 0xE8) : 0.0);
  } else if (vt == kHelpTextScreen || vt == kHouseMenu || vt == kLeaderboardMenu || vt == kMailScreen || vt == kVSSetupMenu) {
    // These Xbox screens have empty MouseWheel methods. Use only verified
    // directional commands, preserving their native animation/row guards.
    const bool rows = vt == kLeaderboardMenu;
    auto* fn = vt == kHelpTextScreen ? sub_8219A210 : vt == kHouseMenu ? sub_8219D480 :
        rows ? sub_821B9618 : vt == kMailScreen ? sub_821BECE0 : sub_8228B5C0;
    const auto user = vt == kVSSetupMenu ?
        (pvz::LocalPlayersActive() ? uint32_t(pvz::MouseUserIndex()) : Read(b, screen + 0x118)) : 0;
    if (vt != kVSSetupMenu || Read(b, screen + 0x114) == 1)
      GuestCall(ctx, b, fn, screen, steps > 0 ? (rows ? 0u : 2u) : (rows ? 1u : 3u), user, 0);
    PVZ_TRACE("PvZ service wheel vtable {:08X} steps {}", vt, steps);
  } else if (vt == kStoreScreen && StoreReady(b, screen)) {
    capture = {};
    const auto before = Read(b, screen + 0x1DC), user = Read(b, screen + 0x200);
    // Native page animation rejects further commands until it finishes.
    GuestCall(ctx, b, sub_82217888, screen, steps > 0 ? 10u : 11u, user, 0);
    PVZ_TRACE("PvZ Shop scroll page {} -> {}", before, Read(b, screen + 0x1DC));
  } else if (Garden(b, screen) && !BoardPaused(b, screen)) {
    const auto controls = Read(b, screen + 0x1B0), slot = ToolSlot(b, screen), before = Read(b, controls + slot);
    GuestCall(ctx, b, Tree(b, screen) ? sub_82191B10 : sub_82192490, controls, steps > 0 ? 10u : 11u, 0, 0);
    PVZ_TRACE("PvZ {} scroll tool {} -> {}", Tree(b, screen) ? "Tree" : "Zen", int32_t(before), int32_t(Read(b, controls + slot)));
  }
}
bool pvz::ServiceMouseButton(PPCContext& ctx, uint8_t* b, uint32_t screen, Point p, int button, bool down) {
  if (!screen) return false;
  const auto vt = Read(b, screen);
  if (vt == kHouseChooser) {
    if (button < 0) {
      capture = {};
      if (down) GuestCall(ctx, b, sub_8219ACD0, screen, 7, 0, 0);
    } else if (button > 0) {
      const int house = HouseHit(b, screen, p);
      if (down) capture = {screen, -1, house};
      else {
        const bool activate = capture.screen == screen && house >= 0 && house == capture.slot;
        capture = {};
        if (activate) {
          const auto profile = Read(b, Read(b, screen + 0x1A4) + 0x9FC);
          Write(b, screen + 0x1C0, house);
          GuestCall(ctx, b, sub_8219ACD0, screen, 6, 0, 0);
          PVZ_TRACE("PvZ Select House confirmed {} profile {}", house, Read(b, profile + 0x96C));
        }
      }
    }
    return true;
  }
  if (vt == kStoreScreen && button > 0) {
    const int slot = StoreHit(ctx, b, screen, p);
    if (down) {
      if (b[screen + 0x1D0]) {
        GuestCall(ctx, b, sub_82217888, screen, 6, Read(b, screen + 0x200), 0);
        PVZ_TRACE("PvZ Shop Dave advanced");
      } else capture = {screen, int(Read(b, screen + 0x1DC)), slot};
    } else {
      const bool activate = capture.screen == screen && capture.page == int(Read(b, screen + 0x1DC)) && slot >= 0 && slot == capture.slot;
      capture = {}; // Clear before the purchase enters a nested modal loop.
      if (activate) {
        Write(b, screen + 0x1E0, uint32_t(slot));
        const auto app = Read(b, screen + 0x1A4), profile = Read(b, app + 0x9FC);
        const auto coins = Read(b, profile + 0x220), item = GuestCall(ctx, b, sub_822142E0, screen, uint32_t(slot));
        GuestCall(ctx, b, sub_82217888, screen, 6, Read(b, screen + 0x200), 0);
        PVZ_TRACE("PvZ Shop purchase item {} coins {} -> {}", item, coins, Read(b, profile + 0x220));
      }
    }
    return true;
  }
  if (vt == kAlmanac && button > 0) {
    if (Read(b, screen + 0x1D4) == 0) {
      const auto local = Local(b, screen, p);
      const int page = !local ? -1 : pvz::InRect(*local, 24, 170, 370, 260) ? 1 :
          pvz::InRect(*local, 408, 170, 370, 260) ? 2 : -1;
      if (down) capture = {screen, 0, page};
      else {
        const bool activate = capture.screen == screen && capture.page == 0 && page > 0 && page == capture.slot;
        capture = {};
        if (activate) {
          GuestCall(ctx, b, sub_82122BD8, screen, uint32_t(page));
          PVZ_TRACE("PvZ Almanac opened page {}", page);
        }
      }
      return true;
    }
    if (down) if (auto local = Local(b, screen, p)) {
      const auto enabled = b[screen + 0x71]; b[screen + 0x71] = 1;
      GuestCall(ctx, b, sub_82125150, screen, uint32_t(int(local->x)), uint32_t(int(local->y)), 1);
      b[screen + 0x71] = enabled;
      PVZ_TRACE("PvZ Almanac selection page {} plant {} zombie {}", Read(b, screen + 0x1D4), Read(b, screen + 0x1D8), Read(b, screen + 0x1DC));
    }
    return true;
  }
  if (!GardenScreen(b, screen)) return false;
  if (!down) return true;
  const auto app = BoardApp(b, screen), controls = Read(b, screen + 0x1B0);
  if (int32_t(Read(b, app + 0xA24)) != -1) {
    if (button > 0) {
      const auto before = Read(b, app + 0xA24);
      // First-visit speech is a CutScene: it also grants the starting food.
      // Challenge's similarly named routine handles puzzle tutorials only.
      if (Read(b, controls) == kTreeControls && GameScene(b, app) == kSceneLevelIntro)
        GuestCall(ctx, b, sub_8217D798, Read(b, screen + 0x1D0), 0);
      else GuestCall(ctx, b, sub_8214CDC0, screen, 6, 0, 0);
      PVZ_TRACE("PvZ garden Dave click mode {} message {} -> {}", GameMode(b, app), int32_t(before), int32_t(Read(b, app + 0xA24)));
    }
    return true;
  }
  if (!Garden(b, screen)) return false;
  if (BoardPaused(b, screen)) return true;
  if (button < 0) {
    ClearGarden(ctx, b, screen);
    PVZ_TRACE("PvZ Zen tool cancelled");
    return true;
  }
  const auto local = MapBoardPointer(p);
  if (!local) return true;
  GardenPointer(b, controls);
  const auto hit = ctx.r1.u32 - 0x80;
  Write(b, hit, 0); Write(b, hit + 4, 0);
  GuestCall(ctx, b, sub_82139238, screen, uint32_t(int(local->x)), uint32_t(int(local->y)), hit, 0);
  const auto toolbar = GardenToolAt(ctx, b, screen, *local);
  const auto cursor = Read(b, screen + 0x1B8);
  if (toolbar) {
    const auto before = Read(b, cursor + 0x4C);
    if (Pc() && Read(b, controls + ToolSlot(b, screen)) == toolbar && before) ClearGarden(ctx, b, screen);
    else if (GuestCall(ctx, b, sub_82139808, screen, toolbar, 0)) {
      Write(b, controls + ToolSlot(b, screen), toolbar);
      const auto tool = Read(b, cursor + 0x4C);
      if (tool >= 18 && tool <= 21) {
        capture = {};
        SwitchGarden(ctx, b, screen, *local, tool);
        PVZ_TRACE("PvZ garden toolbar switched location {}", toolbar);
        return true; // Native switch can reconstruct the Board.
      }
    }
    PVZ_TRACE("PvZ garden toolbar tool {} cursor {} -> {}", toolbar, before, Read(b, cursor + 0x4C));
    return true;
  }
  if (Tree(b, screen) && Read(b, hit + 4) != 3)
    GuestCall(ctx, b, sub_82165788, Read(b, screen + 0x1D4), uint32_t(int(local->x)), uint32_t(int(local->y)), hit);
  const auto type = Read(b, hit + 4);
  if (Tree(b, screen) && type == 18) {
    if (!Read(b, cursor + 0x4C) && GuestCall(ctx, b, sub_82165920, Read(b, screen + 0x1D4)) &&
        GuestCall(ctx, b, sub_82139808, screen, 14, 0)) Write(b, controls + ToolSlot(b, screen), 14);
    if (Read(b, cursor + 0x4C) != 17) return true;
    // TreeHitTest uses 18 on Xbox (19 on Vita). The native A handler
    // validates dialog/growth state and consumes food only when allowed.
    const auto profile = Read(b, app + 0x9FC), before = Read(b, profile + 0x424);
    GuestCall(ctx, b, sub_82191B10, controls, 6, 0, 0);
    const auto after = Read(b, profile + 0x424);
    if (Pc() || after < before) {
      ClearGarden(ctx, b, screen); ItemCursor(false);
      // Console resumes its normal food selection when the projectile ends.
      // An empty hand after a garden switch keeps the -1 sentinel instead.
      if (!Pc()) Write(b, controls + 0xFC, 0);
    }
    if (after < before && REXCVAR_GET(pvz_input_trace))
      REXLOG_INFO("PvZ Tree feeding cursor cleared mode {}", Pc() ? "pc" : "console");
    PVZ_TRACE("PvZ Tree food click hit 18 balance {} -> {}", before, Read(b, profile + 0x424));
  } else if (type >= 6 && type <= 18) {
    if (Pc() && Read(b, controls + ToolSlot(b, screen)) == type && Read(b, cursor + 0x4C)) ClearGarden(ctx, b, screen);
    else if (GuestCall(ctx, b, sub_82139808, screen, type, 0)) Write(b, controls + ToolSlot(b, screen), type);
    PVZ_TRACE("PvZ Zen picked tool hit {} cursor {}", type, Read(b, cursor + 0x4C));
  } else if (type == 3) {
    GuestCall(ctx, b, sub_8216EF50, Read(b, hit), 0);
    PVZ_TRACE("PvZ Zen collected coin");
  } else if (Tree(b, screen)) {
    const auto tool = Read(b, cursor + 0x4C);
    if (tool >= 18 && tool <= 21) {
      SwitchGarden(ctx, b, screen, *local, tool);
    }
  } else if (Read(b, cursor + 0x4C) || type == 1 || type == 24) {
    // Console mode retains the Xbox implicit watering can. PC mode requires
    // a fresh explicit tool selection, like an empty cursor on the lawn.
    if (type == 1 && !Read(b, cursor + 0x4C) && !Pc()) {
      if (!GuestCall(ctx, b, sub_82139808, screen, 6, 0)) return true;
      Write(b, controls + ToolSlot(b, screen), 6);
    }
    const auto tool = Read(b, cursor + 0x4C);
    if (!tool && type == 1) {
      PVZ_TRACE("PvZ Zen empty hand requires tool selection");
      return true;
    }
    GuestCall(ctx, b, sub_821388A0, screen, uint32_t(int(local->x)), uint32_t(int(local->y)), tool);
    // Feeding tools are single-use selections in PC mode. A glove carrying
    // a plant (cursor 3) must survive until the plant is placed.
    if (Pc() && Read(b, cursor + 0x4C) != 3) ClearGarden(ctx, b, screen);
    PVZ_TRACE("PvZ Zen applied tool {} hit {} at {},{} cursor now {}", tool, type, local->x, local->y, Read(b, cursor + 0x4C));
  }
  return true;
}

void DrawAlmanacGrid(PPCContext& ctx, uint8_t* b, uint32_t offset, PPCFunc* draw, PPCFunc* hit_test) {
  const auto screen = ctx.r3.u32;
  if (!pvz::MouseOwnsInput() || screen != foreground) { draw(ctx, b); return; }
  const auto local = Local(b, screen, position);
  const auto hovered = local ? GuestCall(ctx, b, hit_test, screen, uint32_t(int(local->x)), uint32_t(int(local->y))) : uint32_t(-1);
  const auto selected = Read(b, screen + offset);
  grid_selection = {screen, offset, selected};
  Write(b, screen + offset, hovered);
  // The grid caches its highlighted card before its first availability call.
  // That call restores the real selection before the detail panel is drawn.
  draw(ctx, b);
  RestoreAlmanacSelection(b);
  Write(b, screen + offset, selected);
  if (REXCVAR_GET(pvz_input_trace)) {
    static uint32_t last_screen = 0, last_hover = uint32_t(-2), last_offset = 0;
    if (screen != last_screen || hovered != last_hover || offset != last_offset)
      REXLOG_INFO("PvZ Almanac animated grid page {} hover {} selection {}", offset == 0x1D8 ? 1 : 2, int32_t(hovered), int32_t(selected));
    last_screen = screen; last_hover = hovered; last_offset = offset;
  }
}
REX_HOOK_RAW(sub_82123220) { DrawAlmanacGrid(ctx, base, 0x1D8, __imp__sub_82123220, sub_82124A90); }
REX_HOOK_RAW(sub_82123CD8) { DrawAlmanacGrid(ctx, base, 0x1DC, __imp__sub_82123CD8, sub_82124FC0); }
REX_HOOK_RAW(sub_821A8CE8) {
  if (ctx.lr == 0x82123474) RestoreAlmanacSelection(base);
  __imp__sub_821A8CE8(ctx, base);
}
REX_HOOK_RAW(sub_82124D08) {
  if (ctx.lr == 0x82123F28) RestoreAlmanacSelection(base);
  __imp__sub_82124D08(ctx, base);
}

REX_HOOK_RAW(sub_82192080) {
  const auto controls = ctx.r3.u32, board = Read(base, controls + 0x44);
  const bool mouse = pvz::MouseOwnsInput() && board == foreground && Garden(base, board) && !BoardPaused(base, board);
  if (mouse) GardenPointer(base, controls);
  __imp__sub_82192080(ctx, base);
  if (mouse) GardenPointer(base, controls);
  if (mouse && controls == Read(base, board + 0x1B0) && REXCVAR_GET(pvz_input_trace)) {
    const auto background = Read(base, board + 0x55E0), slot = Read(base, controls + 0xF8);
    const auto cursor = Read(base, Read(base, board + 0x1B8) + 0x4C);
    static std::array<uint32_t, 3> previous{uint32_t(-2), uint32_t(-2), uint32_t(-2)};
    const std::array state{background, slot, cursor};
    if (state != previous) REXLOG_INFO("PvZ garden tool state background {} slot {} cursor {} mode {}",
        background, int32_t(slot), cursor, Pc() ? "pc" : "console");
    previous = state;
  }
}

REX_HOOK_RAW(sub_821249A8) {
  const auto screen = ctx.r3.u32;
  const auto visible = base[screen + 0x71];
  base[screen + 0x71] = pvz::MouseOwnsInput() && screen == foreground && position.has_value();
  __imp__sub_821249A8(ctx, base);
  base[screen + 0x71] = visible;
}
REX_HOOK_RAW(sub_82122DD0) {
  const auto screen = ctx.r3.u32;
  const auto visible = base[screen + 0x71];
  base[screen + 0x71] = pvz::MouseOwnsInput() && screen == foreground && position.has_value();
  __imp__sub_82122DD0(ctx, base);
  if (base[screen + 0x71] && REXCVAR_GET(pvz_input_trace)) {
    const auto local = Local(base, screen, position);
    const auto seed = local ? GuestCall(ctx, base, sub_82124A90, screen, uint32_t(int(local->x)), uint32_t(int(local->y))) : uint32_t(-1);
    const auto zombie = local ? GuestCall(ctx, base, sub_82124FC0, screen, uint32_t(int(local->x)), uint32_t(int(local->y))) : uint32_t(-1);
    static uint32_t previous_screen = 0, previous_seed = uint32_t(-1), previous_zombie = uint32_t(-1);
    if (screen != previous_screen || seed != previous_seed || zombie != previous_zombie)
      REXLOG_INFO("PvZ Almanac hover plant {} zombie {}", int32_t(seed), int32_t(zombie));
    previous_screen = screen; previous_seed = seed; previous_zombie = zombie;
  }
  base[screen + 0x71] = visible;
}

REX_HOOK_RAW(sub_821923C0) {
  const auto controls = ctx.r3.u32, board = Read(base, controls + 0x44);
  if (!pvz::MouseOwnsInput() || board != foreground || !Garden(base, board)) {
    __imp__sub_821923C0(ctx, base); return;
  }
  if (!drawing_cursor_overlay) return;
  if (!Pc()) {
    if (NavigationCursor(base, board)) return; // Location icons use the host arrow, including wheel selection.
    const auto cursor = Read(base, board + 0x1B8);
    const auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt;
    if (!local || BoardPaused(base, board)) { __imp__sub_821923C0(ctx, base); return; }
    // CursorObject::Update copies the controls to these integer draw anchors
    // earlier in the frame. Snap at Draw as well, preserving Xbox's overlay.
    const auto x = Read(base, cursor + 0x24), y = Read(base, cursor + 0x28);
    Write(base, cursor + 0x24, uint32_t(int(local->x)));
    Write(base, cursor + 0x28, uint32_t(int(local->y)));
    __imp__sub_821923C0(ctx, base);
    Write(base, cursor + 0x24, x); Write(base, cursor + 0x28, y);
    return;
  }
  const auto app = BoardApp(base, board), cursor = Read(base, board + 0x1B8);
  // Empty hands use the host arrow. Draw a selected tool with its native
  // offsets/area, but omit Xbox's permanent cell-selection overlay.
  if (!position || !pvz::MapBoardPointer(*position) || BoardPaused(base, board)) return;
  if (int32_t(Read(base, app + 0xA18)) >= 3 || !Read(base, cursor + 0x4C)) return;
  const auto local = *pvz::MapBoardPointer(*position);
  const auto graphics = ctx.r4.u32;
  const auto x = Read(base, graphics + 8), y = Read(base, graphics + 12);
  Write(base, graphics + 8, std::bit_cast<uint32_t>(std::bit_cast<float>(x) + local.x));
  Write(base, graphics + 12, std::bit_cast<uint32_t>(std::bit_cast<float>(y) + local.y));
  GuestCall(ctx, base, sub_821774C0, cursor, graphics);
  Write(base, graphics + 8, x); Write(base, graphics + 12, y);
}

REX_HOOK_RAW(sub_8228CE18) {
  const auto zen = ctx.r3.u32, old_board = Read(base, zen + 4);
  const bool mouse = pvz::MouseOwnsInput() && Garden(base, old_board);
  const auto old_cursor = mouse ? Read(base, Read(base, old_board + 0x1B8) + 0x4C) : 0;
  const bool empty = mouse && (old_cursor == 0 || (old_cursor >= 18 && old_cursor <= 21));
  __imp__sub_8228CE18(ctx, base);
  const auto board = Read(base, zen + 4);
  // InitLevel resets F8 to zero even when the Board itself hasn't changed.
  // PC returns with an empty hand; Console retains an explicitly empty hand.
  if (mouse && (Pc() || empty) && Garden(base, board)) {
    ClearGarden(ctx, base, board);
    PVZ_TRACE("PvZ garden initialized empty hand background {}", Read(base, board + 0x55E0));
  }
}

REX_HOOK_RAW(sub_82189B40) {
  const auto controls = ctx.r3.u32;
  if (pvz::MouseOwnsInput() && (Read(base, controls) == kZenControls || Read(base, controls) == kTreeControls) &&
      Read(base, controls + 0x44) == foreground) {
    GardenPointer(base, controls); return;
  }
  __imp__sub_82189B40(ctx, base);
}

REX_HOOK_RAW(sub_82191DA8) {
  const auto controls = ctx.r3.u32, board = Read(base, controls + 0x44);
  const bool mouse = pvz::MouseOwnsInput() && board == foreground && Tree(base, board) && !BoardPaused(base, board);
  const bool empty = mouse && !Read(base, Read(base, board + 0x1B8) + 0x4C) &&
      (Pc() || int32_t(Read(base, controls + 0xFC)) == -1);
  if (mouse) GardenPointer(base, controls);
  __imp__sub_82191DA8(ctx, base);
  if (mouse) GardenPointer(base, controls);
  if (mouse && REXCVAR_GET(pvz_input_trace)) {
    const bool ready = GuestCall(ctx, base, sub_82165920, Read(base, board + 0x1D4)) != 0;
    const auto cursor = Read(base, Read(base, board + 0x1B8) + 0x4C);
    static std::array<uint32_t, 3> previous{uint32_t(-1), uint32_t(-1), uint32_t(-1)};
    const std::array state{uint32_t(ready), cursor, uint32_t(Pc())};
    if (state != previous) REXLOG_INFO("PvZ Tree food cursor ready {} cursor {} mode {}", ready, cursor, Pc() ? "pc" : "console");
    previous = state;
  }
  // Tree's Update selects food whenever the hand is empty (unlike Zen's
  // -1 sentinel). Preserve PC hands and Console hands after a garden switch.
  if (empty) Write(base, controls + 0xFC, uint32_t(-1));
}

REX_HOOK_RAW(sub_82139808) {
  if (ctx.lr == 0x82191E5C && pvz::MouseOwnsInput() &&
      ctx.r3.u32 == foreground && Tree(base, foreground) &&
      (Pc() || int32_t(Read(base, Read(base, foreground + 0x1B0) + 0xFC)) == -1) &&
      !Read(base, Read(base, foreground + 0x1B8) + 0x4C)) {
    ctx.r3.u64 = 0; return; // Suppress automatic food pickup and its repeated sound.
  }
  __imp__sub_82139808(ctx, base);
}

REX_HOOK_RAW(sub_82217888) {
  const auto screen = ctx.r3.u32, action = ctx.r4.u32;
  const bool navigation = action <= 3 || action == 10 || action == 11;
  const auto slot = navigation ? Read(base, screen + 0x1E0) : 0;
  const auto page = navigation ? Read(base, screen + 0x1DC) : 0;
  __imp__sub_82217888(ctx, base);
  if (navigation && REXCVAR_GET(pvz_input_trace))
    REXLOG_INFO("PvZ Shop native navigation {} slot {} -> {} page {} -> {}", action, slot,
        Read(base, screen + 0x1E0), page, Read(base, screen + 0x1DC));
}

REX_HOOK_RAW(sub_8219ACD0) {
  const auto screen = ctx.r3.u32, action = ctx.r4.u32;
  const auto profile = Read(base, Read(base, screen + 0x1A4) + 0x9FC);
  const auto selected = Read(base, screen + 0x1C0), saved = Read(base, profile + 0x96C);
  __imp__sub_8219ACD0(ctx, base);
  if (REXCVAR_GET(pvz_input_trace) && (action == 2 || action == 3 || action == 6 || action == 7))
    REXLOG_INFO("PvZ Select House native action {} selected {} saved {} -> {}", action, selected,
        saved, Read(base, profile + 0x96C));
}
