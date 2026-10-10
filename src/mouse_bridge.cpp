#include "mouse_bridge.h"
#include "mouse_input.h"
#include "widget_tree.h"
#include "menu_mapping.h"
#include "board_mapping.h"
#include "mouse_prompt.h"
#include "pc_text_runtime.h"
#include "game_settings.h"
#include "pc_menus.h"
#include "controller_assets.h"
#include "challenge_mouse.h"
#include "service_mouse.h"
#include "heavy_weapon_pointer.h"
#include "input_backend.h"
#include "replay_fixtures.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <bit>
#include <mutex>
#include <vector>
#include <atomic>
#include <algorithm>
#include <tuple>
#include <array>
#include <rex/logging.h>
#include <rex/ppc/func.h>

REXCVAR_DEFINE_BOOL(pvz_mouse_cursor, false, "PvZ", "Experimental absolute mouse control for player 1");
REXCVAR_DEFINE_STRING(pvz_cursor_mode, "pc", "PvZ", "Mouse cursor behavior: pc or console")
    .allowed({"pc", "console"});
REXCVAR_DEFINE_DOUBLE(pvz_mouse_canvas_width, 800, "PvZ", "Mouse coordinate canvas width");
REXCVAR_DEFINE_DOUBLE(pvz_mouse_canvas_height, 600, "PvZ", "Mouse coordinate canvas height");
REXCVAR_DEFINE_BOOL(pvz_input_trace, false, "Diagnostics", "Log mouse dispatch and WidgetManager dimensions");
REXCVAR_DEFINE_BOOL(pvz_input_auto_switch, true, "PvZ", "Switch mouse/controller behavior on physical player 1 input");

REX_EXTERN(sub_824A6208); // WidgetManager::MousePosition
REX_EXTERN(sub_824A6428); // WidgetManager::MouseDown
REX_EXTERN(sub_824A6358); // WidgetManager::MouseUp
REX_EXTERN(sub_824A65A0); // WidgetManager::MouseDrag
REX_EXTERN(sub_8214CDC0); // Board::GamepadButtonDown
REX_EXTERN(sub_8214D1B8); // Board::GamepadButtonUp
REX_EXTERN(sub_821C0630); // MainMenu ButtonListener::ButtonDepress (this = menu + D4)
REX_EXTERN(sub_824A5F48); // WidgetManager::SetGamepadFocus
REX_EXTERN(__imp__sub_824A5F48);
REX_EXTERN(sub_8220F370); // SeedPacket::CanPickUp
REX_EXTERN(sub_82190D80); // GamepadControls::SetState
REX_EXTERN(sub_82139238); // Board::MouseHitTest (HitResult*, player); coin hit type = 3
REX_EXTERN(sub_8216EF50); // Coin::Collect (player)
REX_EXTERN(sub_824AE388); // GameButton::GamepadButtonDown
REX_EXTERN(sub_824AE488); // GameButton::GamepadButtonUp
REX_EXTERN(sub_82219690); // TitleScreen::GamepadButtonDown (checks loading completion)
REX_EXTERN(__imp__sub_824AE518); // ButtonWidget::MouseEnter
REX_EXTERN(sub_824AE610); // ButtonWidget::MouseLeave
REX_EXTERN(sub_821388A0); // Board::MouseDownWithTool(x, y, cursor type)
REX_EXTERN(sub_82134748); // Board::CountPlantByType(seed type)
REX_EXTERN(sub_8213D0F0); // Board::SetTutorialState(state)
REX_EXTERN(sub_8217D798); // CutScene::AdvanceCrazyDaveDialog(skip = false)
REX_EXTERN(sub_82139808); // Board::PickUpTool(hit type, player)
REX_EXTERN(sub_82139AC0); // Board::ClearCursor(player)
REX_EXTERN(sub_82134340); // Board::GetPlantsOnLawn(col, row, result)
REX_EXTERN(sub_82149538); // Board::PixelToGridX
REX_EXTERN(sub_821495B0); // Board::PixelToGridY without terrain height
REX_EXTERN(sub_82149728); // Board::PixelToGridY
REX_EXTERN(sub_8218F9A8); // GamepadControls::DrawSeedPreview
REX_EXTERN(sub_82135C48); // Board::CanPlantAt
REX_EXTERN(sub_82149848); // Board::GridToPixelY
REX_EXTERN(sub_821774C0); // CursorObject::Draw
REX_EXTERN(sub_821A8258); // LawnApp::PlayFoley
REX_EXTERN(sub_8220AFA8); // SeedChooser::SeedHitTest
REX_EXTERN(__imp__sub_8220AFA8);
REX_EXTERN(sub_82208A80); // SeedChooser::SeedNotAllowedToPick
REX_EXTERN(sub_8220B1C0); // SeedChooser: return a bank card to the chooser
REX_EXTERN(sub_8220B440); // SeedChooser: add a chooser card to the bank
REX_EXTERN(sub_8220C280); // SeedChooser::GamepadButtonDown, including Dave introduction
REX_EXTERN(sub_8212C4D0); // AwardScreen::GamepadButtonDown (native readiness guard)
REX_EXTERN(__imp__sub_821ABDE8); // LawnApp::PlaySample
REX_EXTERN(__imp__sub_824A85F0); // Dialog::MouseDrag
REX_EXTERN(__imp__sub_82165B58); // Challenge::UpdateHeavyWeapon
REX_EXTERN(__imp__sub_82580C58); // Input: right stick X
REX_EXTERN(__imp__sub_82580C80); // Input: right stick Y
REX_EXTERN(__imp__sub_821667C0); // Challenge::FireHeavyWeapon
REX_EXTERN(sub_8220F510); // SeedPacket::PickUp; Heavy Weapon purchases its upgrade directly.
REX_EXTERN(sub_82153300); // Challenge::MouseDown(x,y,HitResult*,player)
REX_EXTERN(sub_821538D8); // Challenge::GamepadButtonDown
REX_EXTERN(sub_82153018); // Challenge::MouseDownWhackAZombie(x,y,player)
REX_EXTERN(sub_821A8770); // LawnApp::IsScaryPotterLevel
REX_EXTERN(sub_82152F20); // Challenge::MouseMove(x,y)
REX_EXTERN(sub_821516D8); // Challenge::BeghouledDragUpdate(x,y)
REX_EXTERN(sub_82208550); // SeedChooser::GetSeedPosition(index,x*,y*)
REX_EXTERN(sub_821A8CE8); // LawnApp::IsSeedAvailable(index,zombies)
REX_EXTERN(sub_82206970); // ImitaterDialog::SeedHitTest
REX_EXTERN(sub_822074F0); // ImitaterDialog::GamepadButtonDown
REX_EXTERN(__imp__sub_8216DC40);
REX_EXTERN(__imp__sub_821516D8);
REX_EXTERN(sub_82150F60); // Twist hit test returns the top-left of its 2x2 block
REX_EXTERN(sub_8214F5E8); // LoadBeghouledBoardState (scratch copy)
REX_EXTERN(sub_82151600); // BeghouledMoveCausesMatch (scratch copy)
REX_EXTERN(sub_82150EB8); // BeghouledTwistMoveCausesMatch (scratch copy)

namespace {
std::mutex pointer_mutex;
std::optional<pvz::Point> pointer;
struct ButtonEvent { pvz::Point point; int click; bool down; };
pvz::MouseInputStream mouse_events;
pvz::MouseButtons mouse_buttons;
pvz::MouseWheel mouse_wheel;
std::atomic_bool mouse_owner{true};
std::atomic_bool mouse_item_cursor{false};
std::atomic_bool menu_ready{false}, board_ready{false};
std::atomic_bool title_ready{false};
std::atomic<uint32_t> replay_screen{0};
std::atomic_bool replay_input_idle{false};
// Guest-thread state. These addresses are identity keys only; each use reads
// and validates the current Board, primary controls and seed bank again.
struct MouseSelection {
  uint32_t board = 0, controls = 0, bank = 0;
  int packet = -1;
  bool shovel = false;
  uint32_t shovel_plant = 0;
  bool usable_seed = false;
  // Native conveyor slot numbers survive a shift. Remember the prefix's
  // identities, not its animated positions; appending a card remains safe.
  std::vector<std::array<uint32_t, 4>> packets;
} selection;
struct ItemDrag {
  uint32_t board = 0, controls = 0, bank = 0;
  int packet = -1;
  bool shovel = false, moved = false;
  pvz::Point origin{};
  void Observe(pvz::Point point) {
    const auto dx = point.x - origin.x, dy = point.y - origin.y;
    moved |= dx * dx + dy * dy >= 36;
  }
} item_drag;
bool discard_item_release = false;
struct BeghouledMouse {
  uint32_t board = 0;
  std::optional<pvz::Point> selected;
  bool held = false;
} beghouled_mouse;
std::vector<uint32_t> puzzle_highlights;
uint32_t captured_dialog = 0;
int captured_dialog_id = -1;
uint32_t captured_board = 0;
uint32_t captured_menu = 0;
int captured_button = -1;
struct ConsolePointer {
  uint32_t controls = 0;
  pvz::Point raw{};
} console_pointer;
struct ConsoleShovel {
  uint32_t board = 0, controls = 0;
  bool held = false;
} console_shovel;
thread_local std::optional<pvz::Point> console_draw_offset;
thread_local std::optional<pvz::Point> heavy_weapon_aim;
struct HeavyWeaponMouse { uint32_t board = 0; bool firing = false; } heavy_weapon_mouse;
thread_local bool explicit_mouse_collect = false;
std::vector<uint32_t> slot_auto_attempted;
struct LateCursor { uint32_t board = 0, controls = 0; std::array<uint32_t, 17> graphics{}; };
thread_local LateCursor late_cursor;
thread_local bool drawing_late_cursor = false;
uint32_t mouse_hovered_button = 0;
struct NativePadButtonPress { uint32_t manager = 0, widget = 0, user = 0; } native_pad_button_press;

bool MouseActive() { return REXCVAR_GET(pvz_mouse_cursor) && mouse_owner.load(std::memory_order_relaxed); }
uint32_t MouseControls(uint32_t board, uint8_t* base);
uint32_t MouseCursor(uint32_t board, uint8_t* base);
bool PcMouseMode() { return MouseActive() && REXCVAR_GET(pvz_cursor_mode) == "pc"; }

bool IsGameButton(uint32_t vtable) {
  return vtable == 0x820BA0AC || vtable == 0x820BA21C || vtable == 0x820BA38C;
}

uint32_t FindMenuButton(uint32_t menu, int id, uint8_t* base, bool enabled_only = true) {
  const auto head = __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + menu + 0x18));
  const auto read = [&](uint32_t a) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + a)); };
  if (!head) return 0;
  auto node = read(head);
  for (int i = 0; node && node != head && i < 128; ++i, node = read(node)) {
    const auto child = read(node + 8);
    if (child && IsGameButton(read(child)) && read(child + 0x6C) == uint32_t(id) &&
        (!enabled_only || (base[child + 0x70] && !base[child + 0x72] && !base[child + 0x149]))) return child;
  }
  return 0;
}

using pvz::guest::Read;
using pvz::guest::Write;

bool LiveWidget(uint32_t manager, uint32_t widget, uint8_t* base) {
  return pvz::guest::LiveWidget(base, manager, widget);
}

std::array<uint32_t, 4> PacketIdentity(uint8_t* base, uint32_t packet) {
  return {Read(base, packet + 0x48), Read(base, packet + 0x50),
      Read(base, packet + 0x54), Read(base, packet + 0x68)};
}
void RememberPackets(uint8_t* base) {
  selection.packets.clear();
  for (int i = 0; i <= selection.packet; ++i)
    selection.packets.push_back(PacketIdentity(base, selection.bank + 0x50 + uint32_t(i) * 0x7C));
}
bool SamePackets(uint8_t* base) {
  const auto count = Read(base, selection.bank + 0x40);
  if (count > 10 || count < selection.packets.size() || selection.packets.empty()) return false;
  for (size_t i = 0; i < selection.packets.size(); ++i)
    if (PacketIdentity(base, selection.bank + 0x50 + uint32_t(i) * 0x7C) != selection.packets[i]) return false;
  return true;
}


uint32_t FindRootWidget(uint32_t manager, uint32_t vtable, uint8_t* base, bool include_disabled = false) {
  const auto head = Read(base, manager + 0x18);
  if (!head) return 0;
  auto node = Read(base, head);
  for (int i = 0; node && node != head && i < 128; ++i, node = Read(base, node)) {
    const auto child = Read(base, node + 8);
    if (child && Read(base, child) == vtable && base[child + 0x70] && (include_disabled || !base[child + 0x72]))
      return child;
  }
  return 0;
}

bool InactiveSeedChooser(uint32_t widget, uint8_t* base) {
  if (!widget || Read(base, widget) != 0x820CA94C) return false;
  const auto app = Read(base, widget + 0xE60);
  if (!app) return false;
  // Intro keeps this widget attached after selection ends. Slot Machine,
  // Zen and Tree never choose seeds; their native tutorials own Board input.
  const auto mode = Read(base, app + 0x9CC);
  if (mode == 19 || mode == 44 || mode == 51 || Read(base, app + 0x9D0) == 3) return true;
  const auto board = Read(base, app + 0x900);
  return mode == 0 && board && Read(base, board) == 0x820B728C &&
      Read(base, board + 0x5620) >= 15 && Read(base, board + 0x5620) <= 17;
}

uint32_t TopRootWidget(uint32_t manager, uint8_t* base) {
  const auto head = Read(base, manager + 0x18);
  if (!head) return 0;
  uint32_t top = 0;
  auto node = Read(base, head);
  for (int i = 0; node && node != head && i < 64; ++i, node = Read(base, node)) {
    const auto child = Read(base, node + 8);
    if (child && base[child + 0x70] && Read(base, child) != 0x820BA804 && !InactiveSeedChooser(child, base)) top = child;
  }
  return top;
}

uint32_t VisibleDescendant(uint32_t container, uint32_t vtable, uint8_t* base) {
  uint32_t found = 0;
  pvz::guest::VisitWidgets(base, container, [&](uint32_t widget) {
    if (Read(base, widget) == vtable && !base[widget + 0x72]) found = widget;
    return !found;
  }, true);
  return found;
}

uint32_t MouseMenu(uint32_t manager, uint8_t* base) {
  auto over = Read(base, manager + 0xB0);
  if (over && IsGameButton(Read(base, over))) over = Read(base, over + 0x24);
  return over && TopRootWidget(manager, base) == over && Read(base, over) == 0x820C2784 && base[over + 0x70] && !base[over + 0x72] ? over : 0;
}

bool StableMenu(uint32_t menu, uint8_t* base) {
  return Read(base, menu + 0x118) < 3 &&
      Read(base, menu + 0x118) == Read(base, menu + 0x11C);
}

int AdventureButtonId(uint32_t menu, uint8_t* base) {
  // The native menu swaps Start Adventure (0) for Adventure (1) once the
  // profile has progress. Resolve the live widget, not the saved level.
  if (FindMenuButton(menu, 1, base)) return 1;
  return FindMenuButton(menu, 0, base) ? 0 : -1;
}

int MenuButtonAt(uint32_t menu, pvz::Point position, uint8_t* base) {
  if (!StableMenu(menu, base)) return -1;
  const auto panel = Read(base, menu + 0x118);
  if (panel == 1) {
    const auto id = pvz::MainMenuButtonAt(position);
    return id == 0 ? AdventureButtonId(menu, base) : id;
  }
  // Reanimation updates these draw positions as the panorama moves. They
  // already use the full guest framebuffer, unlike Board/dialog coordinates.
  const pvz::Point pixel{position.x * 1.6f, position.y * 1.2f};
  const int first = panel == 0 ? 9 : 15, last = panel == 0 ? 14 : 19;
  for (int id = first; id <= last; ++id) {
    const auto child = FindMenuButton(menu, id, base);
    if (!child) continue;
    const double x = base[child + 0x198] ?
        std::bit_cast<float>(Read(base, child + 0x19C)) : int32_t(Read(base, child + 0x190));
    const double y = base[child + 0x198] ?
        std::bit_cast<float>(Read(base, child + 0x1A0)) : int32_t(Read(base, child + 0x194));
    const double width = Read(base, child + 0x1A4), height = Read(base, child + 0x1A8);
    const double sx = std::bit_cast<float>(Read(base, child + 0x1C4));
    const double sy = std::bit_cast<float>(Read(base, child + 0x1C8));
    if (sx > 0 && sy > 0 && pixel.x >= x + width * (1 - sx) * 0.5 &&
        pixel.y >= y + height * (1 - sy) * 0.5 &&
        pixel.x < x + width * (1 + sx) * 0.5 &&
        pixel.y < y + height * (1 + sy) * 0.5) return id;
  }
  return -1;
}

uint32_t DialogParent(uint32_t over, uint8_t* base) {
  if (over && IsGameButton(Read(base, over))) over = Read(base, over + 0x24);
  if (!over || Read(base, over) == 0x820C2784 || Read(base, over) == 0x820B728C)
    return 0;
  return over;
}

double StoneButtonWidth(double requested, uint8_t* base) {
  // DrawStoneButton (82187970) rounds the middle tile count. Its painted
  // width can be smaller than the layout width (328 -> 300 in Help/Options).
  const auto left = Read(base, 0x82D09F40);
  const auto middle = Read(base, 0x82D0A738);
  const auto right = Read(base, 0x82D09EEC);
  if (!left || !middle || !right) return requested;
  const auto lw = Read(base, left + 0x24), mw = Read(base, middle + 0x24),
      rw = Read(base, right + 0x24);
  if (!lw || !mw || !rw || lw > 4096 || mw > 4096 || rw > 4096) return requested;
  const auto tiles = std::max(0L, std::lround((requested - lw - rw) / mw));
  return lw + tiles * mw + rw;
}

uint32_t DrawnDialogButtonAt(uint32_t parent, pvz::Point local, uint8_t* base) {
  if (!parent) return 0;
  double x = 0, y = 0;
  for (auto ancestor = parent, depth = uint32_t(0); ancestor && depth < 16; ++depth) {
    x += int32_t(Read(base, ancestor + 0x40));
    y += int32_t(Read(base, ancestor + 0x44));
    ancestor = Read(base, ancestor + 0x24);
  }
  const auto head = Read(base, parent + 0x18);
  uint32_t painted = 0;
  if (!head) return 0;
  auto node = Read(base, head);
  for (int i = 0; node && node != head && i < 128; ++i, node = Read(base, node)) {
    const auto child = Read(base, node + 8);
    if (!child || !IsGameButton(Read(base, child)) ||
        !base[child + 0x70] || base[child + 0x72] || base[child + 0x149]) continue;
    double bx = x + (base[child + 0x198] ?
        std::bit_cast<float>(Read(base, child + 0x19C)) : int32_t(Read(base, child + 0x190)));
    double by = y + (base[child + 0x198] ?
        std::bit_cast<float>(Read(base, child + 0x1A0)) : int32_t(Read(base, child + 0x194)));
    double width = Read(base, child + 0x1A4);
    double height = Read(base, child + 0x1A8);
    // GameButton::Draw scales about the image center. Use its base scale,
    // not the animated hover pulse, so entering/leaving has stable bounds.
    const double sx = std::bit_cast<float>(Read(base, child + 0x1C4));
    const double sy = std::bit_cast<float>(Read(base, child + 0x1C8));
    if (!std::isfinite(sx) || !std::isfinite(sy) || sx <= 0 || sy <= 0) continue;
    bx += width * (1 - sx) * 0.5;
    by += height * (1 - sy) * 0.5;
    width = (base[child + 0x1B0] ? StoneButtonWidth(width, base) : width) * sx;
    height *= sy;
    if (width > 0 && height > 0 && local.x >= bx && local.y >= by &&
        local.x < bx + width && local.y < by + height) painted = child;
  }
  return painted;
}

bool AwardButtonAt(uint32_t award, pvz::Point local, uint8_t* base) {
  // AwardScreen owns this LawnButton directly and calls Draw itself. It is
  // not in the widget child list, and its base Widget rect is empty.
  const auto button = Read(base, award + 0xD8);
  return button && pvz::InRect(local, int32_t(Read(base, button + 0x190)),
      int32_t(Read(base, button + 0x194)), int(StoneButtonWidth(Read(base, button + 0x1A4), base)),
      Read(base, button + 0x1A8));
}

uint32_t HoveredButton(uint32_t manager, std::optional<pvz::Point> position, uint8_t* base) {
  if (!position) return 0;
  auto over = Read(base, manager + 0xB0);
  if (over && IsGameButton(Read(base, over))) over = Read(base, over + 0x24);
  if (over && !base[over + 0x70]) over = 0;
  if (over && Read(base, over) == 0x820C2784) {
    return FindMenuButton(over, MenuButtonAt(over, *position, base), base);
  }
  const auto local = pvz::MapUiPointer(*position);
  const auto foreground = TopRootWidget(manager, base);
  // The chooser is a child of the full-screen intro widget, whose own
  // rectangle otherwise masks the chooser's painted buttons.
  if (local && foreground && Read(base, foreground) == 0x820BA804)
    if (auto chooser = VisibleDescendant(manager, 0x820CA94C, base))
      if (!VisibleDescendant(manager, 0x820CAAC4, base))
        if (auto button = DrawnDialogButtonAt(chooser, *local, base)) return button;
  // Native modal rectangles can leave the underlying screen in mOverWidget.
  // Hit the top screen's painted buttons before any background screen.
  if (foreground && Read(base, foreground) != 0x820C2784 && Read(base, foreground) != 0x820B728C)
    return local ? DrawnDialogButtonAt(foreground, *local, base) : 0;
  if (local) if (auto award = VisibleDescendant(manager, 0x820B4F44, base))
    if (auto button = DrawnDialogButtonAt(award, *local, base)) return button;
  if (local) if (auto chooser = VisibleDescendant(manager, 0x820CA94C, base))
    if (auto button = DrawnDialogButtonAt(chooser, *local, base)) return button;
  if (local) for (const auto vt : {0x820BB22Cu, 0x820C1D24u, 0x820BBF94u})
    if (auto help = VisibleDescendant(manager, vt, base))
      if (auto button = DrawnDialogButtonAt(help, *local, base)) return button;
  if (local) {
    if (auto button = DrawnDialogButtonAt(DialogParent(over, base), *local, base)) return button;
    return DrawnDialogButtonAt(TopRootWidget(manager, base), *local, base);
  }
  return 0;
}

void SyncMouseHover(PPCContext& ctx, uint32_t manager, std::optional<pvz::Point> position,
    uint8_t* base) {
  if (!MouseActive() ||
      Read(base, manager) != 0x8206404C) return;
  if (pvz::LocalPlayersActive() && native_pad_button_press.manager == manager &&
      native_pad_button_press.widget == Read(base, manager + 0xA8) &&
      native_pad_button_press.widget && base[native_pad_button_press.widget + 0x74]) return;
  auto hovered = HoveredButton(manager, position, base);
  const auto focused = Read(base, manager + 0xA8);
  const bool pc = PcMouseMode();
  const auto root = TopRootWidget(manager, base);
  const auto app = root && Read(base, root) == 0x820B728C ? Read(base, root + 0xF4) : 0;
  const bool garden = app && (Read(base, app + 0x9CC) == 44 || Read(base, app + 0x9CC) == 51);
  const auto cursor = app ? MouseCursor(root, base) : 0;
  if (pc && app && ((garden && cursor && Read(base, cursor + 0x4C)) ||
      (selection.board == root && (selection.packet >= 0 || selection.shovel || selection.usable_seed)))) hovered = 0;
  if ((hovered || (focused && IsGameButton(Read(base, focused)))) ||
      (garden && !hovered && focused != root)) {
    PPCContext work = ctx;
    work.r1.u32 -= 0x100;
    work.r3.u32 = manager;
    // Zen's native Update requires Board focus, including while a tool is
    // selected. Leaving a Menu/Shop button must return focus to the Board.
    work.r4.u32 = !hovered && root && Read(base, root) == 0x820B728C ? root : hovered;
    // Use this event's position; the published snapshot can already contain
    // the next queued event. The hook below filters guest-initiated focus.
    __imp__sub_824A5F48(work, base);
  }
  // Native Widget rects at 0,0 can set mIsOver independently of focus.
  // Clear those false positives, including default flags on newly opened UI.
  bool previous_live = false;
  pvz::guest::VisitWidgets(base, manager, [&](uint32_t child) {
    if (IsGameButton(Read(base, child))) {
      if (child == mouse_hovered_button) previous_live = true;
      const bool active = child == hovered;
      base[child + 0x75] = active; base[child + 0xA2] = active;
    }
    return true;
  });
  if (hovered != mouse_hovered_button) {
    PPCContext work = ctx;
    work.r1.u32 -= 0x100;
    if (previous_live) {
      work.r3.u32 = mouse_hovered_button;
      sub_824AE610(work, base);
    }
    mouse_hovered_button = hovered;
    if (hovered) {
      work = ctx;
      work.r1.u32 -= 0x100;
      work.r3.u32 = hovered;
      work.r4.u32 = work.r5.u32 = 0;
      __imp__sub_824AE518(work, base);
      const auto listener = Read(base, hovered + 0x14C);
      const auto enter = listener ? Read(base, Read(base, listener) + 0x10) : 0;
      // Xbox MainMenu/standard dialog listeners have an empty MouseEnter.
      // Reuse the game's loaded SOUND_BLEEP through its normal audio method.
      if (enter == 0x821770E8 || enter == 0x82120000) {
        const auto app = Read(base, hovered + 0x188);
        const auto sound = Read(base, 0x82D0A4F8);
        if (app && int32_t(sound) >= 0) {
          const auto play = Read(base, Read(base, app) + 0xE0);
          work = ctx;
          work.r1.u32 -= 0x100;
          work.r3.u32 = app;
          work.r4.u32 = sound;
          work.r5.u32 = 1;
          rex::runtime::ResolveIndirectFunction(play)(work, base);
          if (REXCVAR_GET(pvz_input_trace))
            REXLOG_INFO("PvZ mouse hover sound button {} sample {} playback {:08X} instance {:08X}",
                Read(base, hovered + 0x6C), sound, play, work.r3.u32);
        }
      }
    }
  }
  static uint32_t logged_hover = uint32_t(-1);
  if (REXCVAR_GET(pvz_input_trace) && hovered != logged_hover) {
    REXLOG_INFO("PvZ mouse hover button {}", hovered ? int32_t(Read(base, hovered + 0x6C)) : -1);
    logged_hover = hovered;
  }
}

uint32_t MouseSeedBank(uint32_t board, uint32_t controls, uint8_t* base) {
  // GamepadControls::GetSeedBank (8218F930): zombie controls use the
  // alternate bank when present. I, Zombie must not select plant packets.
  if (board && controls && base[controls + 0xC4])
    if (const auto bank = Read(base, board + 0x1A8)) return bank;
  const auto app = board ? Read(base, board + 0xF4) : 0;
  const auto mode = app ? Read(base, app + 0x9CC) : 0;
  return board ? Read(base, board + 0x1A4 + (controls && mode >= 77 && mode <= 89 ? Read(base, controls + 0x9C) * 4 : 0)) : 0;
}
uint32_t MouseControls(uint32_t board, uint8_t* base) {
  const auto user = pvz::MouseUserIndex();
  if (board && user >= 0 && pvz::LocalPlayersActive()) {
    // VS sides can swap player indices. Input ownership is the native
    // controls' signed-in user, rather than the plant/zombie side number.
    for (uint32_t i = 0; i < 2; ++i) {
      const auto controls = Read(base, board + 0x1B0 + i * 4);
      if (controls && int32_t(Read(base, controls + 0xA0)) == user) return controls;
    }
  }
  return board && user >= 0 ? Read(base, board + 0x1B0 + uint32_t(user * 4)) : 0;
}
uint32_t MouseCursor(uint32_t board, uint8_t* base) {
  const auto controls = MouseControls(board, base);
  return controls ? Read(base, board + 0x1B8 + Read(base, controls + 0x9C) * 4) : 0;
}
// CursorObject type of the mouse player's hand; 0 (empty) without a cursor.
uint32_t MouseCursorType(uint32_t board, uint8_t* base) {
  const auto cursor = MouseCursor(board, base);
  return cursor ? Read(base, cursor + 0x4C) : 0;
}
void RouteMouseGuestCall(PPCFunc* fn, PPCContext& work, uint8_t* base) {
  const auto user = pvz::MouseUserIndex();
  if (user < 0) return;
  if (fn == sub_8214CDC0 || fn == sub_8214D1B8 || fn == sub_821538D8) work.r5.u32 = uint32_t(user);
  if (fn == sub_824AE388 || fn == sub_824AE488) work.r5.u32 = uint32_t(user);
  if (pvz::LocalPlayersActive() && (fn == sub_8220B1C0 || fn == sub_8220B440 || fn == sub_8220C280))
    work.r5.u32 = uint32_t(user);
  if (fn == sub_82139AC0 || fn == sub_8216EF50 || fn == sub_8220F370 || fn == sub_8220F510) work.r4.u32 = uint32_t(user);
  if (fn == sub_82153300 || fn == sub_821388A0 || fn == sub_82139238) work.r7.u32 = uint32_t(user);
  if (fn == sub_82139808) work.r5.u32 = uint32_t(user);
}

bool PlantMouse(uint32_t controls, uint8_t* base) {
  if (!MouseActive() || !controls ||
      Read(base, controls) != 0x820BA5E8 || int32_t(Read(base, controls + 0xA0)) != pvz::MouseUserIndex())
    return false;
  const auto board = Read(base, controls + 0x44);
  const auto app = Read(base, controls + 0x20);
  const auto bank = MouseSeedBank(board, controls, base);
  const auto mode = app ? Read(base, app + 0x9CC) : uint32_t(-1);
  // Xbox/Android's shared challenge table includes puzzles at 52-71 and
  // bonus single-player boards at 37-50/90. They use the same native bank
  // and primary controls. Keep cinematic/service/multiplayer modes outside
  // this route, along with Heavy Weapon's separate input model.
  const bool single_player = mode == 0 || mode == 90 ||
      (mode >= 2 && mode <= 71 && mode != 20 && mode != 44 && mode != 51);
  const bool izombie = (mode >= 53 && mode <= 69 && (mode & 1)) || mode == 71;
  const bool multiplayer = pvz::LocalPlayersActive() && (mode >= 73 && mode <= 89);
  if (!board || Read(base, board) != 0x820B728C || !app ||
      (!single_player && !multiplayer) ||
      !bank || Read(base, bank) != 0x820CAED8 ||
      (base[controls + 0xC4] && !izombie && !multiplayer)) return false;
  const auto secondary = Read(base, board + 0x1B4);
  if (!multiplayer && secondary && Read(base, secondary + 0xA0) != uint32_t(-1)) return false;
  if (selection.board != board || selection.controls != controls || selection.bank != bank)
    selection = {board, controls, bank, -1};
  return true;
}

int PacketAt(uint32_t bank, pvz::Point local, uint8_t* base) {
  const auto count = Read(base, bank + 0x40);
  if (count > 10) return -1; // The constructor allocates ten packets.
  for (uint32_t i = 0; i < count; ++i) {
    const auto packet = bank + 0x50 + i * 0x7C;
    if (Read(base, packet + 0x50) == uint32_t(-1)) continue;
    const int x = int32_t(Read(base, bank + 0x24)) + int32_t(Read(base, packet + 0x24)) +
        int32_t(Read(base, packet + 0x4C));
    const int y = int32_t(Read(base, bank + 0x28)) + int32_t(Read(base, packet + 0x28));
    if (pvz::InRect(local, x, y, int32_t(Read(base, packet + 0x2C)),
        int32_t(Read(base, packet + 0x30)))) return int(i);
  }
  return -1;
}

bool OverPlantingField(uint32_t bank, pvz::Point local, uint8_t* base) {
  // Native grid conversion clamps coordinates near the board edge. The
  // mouse must be over the lawn before using that conversion for a preview
  // or a release action, including the bank's last few overlapping pixels.
  return local.x >= 40 && local.x < 760 && local.y >= 80 && local.y < 580 &&
      !pvz::InRect(local, int32_t(Read(base, bank + 0x24)), int32_t(Read(base, bank + 0x28)),
          int32_t(Read(base, bank + 0x2C)), int32_t(Read(base, bank + 0x30)));
}

void HideUnselectedCursor(uint32_t controls, uint8_t* base) {
  if (!PcMouseMode() || !PlantMouse(controls, base) || selection.packet >= 0) return;
  const auto cursor = MouseCursor(selection.board, base);
  if (selection.usable_seed && cursor && Read(base, cursor + 0x4C) == 2) {
    Write(base, controls + 0xC0, Read(base, cursor + 0x44));
    return;
  }
  selection.usable_seed = false;
  // The console Update rebuilds this preview every frame. Mouse mode needs
  // an empty hand until a card is picked. Costs/cooldowns stay in guest code.
  const auto app = Read(base, selection.board + 0xF4);
  if (cursor) { Write(base, cursor + 0x44, uint32_t(-1)); Write(base, cursor + 0x4C, selection.shovel ? 6 : Read(base, app + 0x9CC) == 31 ? 7 : 0); }
  Write(base, controls + 0xC0, uint32_t(-1));
  base[controls + 0xC5] = 0;
}

void ApplyPointer(PPCContext& ctx, uint32_t controls, uint8_t* base,
    std::optional<pvz::Point> position = std::nullopt);

void CancelMouseGestures(PPCContext& ctx, uint8_t* base, uint32_t manager) {
  const auto old_selection = selection;
  const auto old_shovel = console_shovel;
  const auto old_beghouled = beghouled_mouse;
  const auto old_menu = captured_menu, old_dialog = captured_dialog;
  const int old_button = captured_button, old_dialog_id = captured_dialog_id;
  if (item_drag.board) {
    discard_item_release = true;
    if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ item drag cancelled input or screen changed");
  }
  // Withdraw all custom capture before callbacks can enter a nested modal loop.
  selection = {}; item_drag = {}; console_shovel = {}; beghouled_mouse = {};
  heavy_weapon_mouse = {}; captured_board = 0;
  captured_menu = captured_dialog = 0; captured_button = captured_dialog_id = -1;
  mouse_buttons.Cancel(); mouse_wheel.Cancel();
  auto call = [&](PPCFunc* fn, uint32_t self, int a = 0, int b = 0, int c = 0) {
    PPCContext work = ctx; work.r1.u32 -= 0x100;
    work.r3.u32 = self; work.r4.s64 = a; work.r5.s64 = b; work.r6.s64 = c;
    work.r7.u32 = 0; RouteMouseGuestCall(fn, work, base); fn(work, base);
  };
  if (LiveWidget(manager, old_menu, base))
    if (auto button = FindMenuButton(old_menu, old_button, base, false)) base[button + 0x74] = 0;
  if (LiveWidget(manager, old_dialog, base))
    if (auto button = FindMenuButton(old_dialog, old_dialog_id, base, false)) base[button + 0x74] = 0;
  auto board = FindRootWidget(manager, 0x820B728C, base, true);
  if (board && board == old_beghouled.board)
    if (const auto challenge = Read(base, board + 0x1D4)) base[challenge + 0x28] = 0;
  if (board && board == old_shovel.board && MouseControls(board, base) == old_shovel.controls && old_shovel.held) {
    // StateExit must not turn an interrupted timed hold into a completed dig.
    Write(base, old_shovel.controls + 0x70, 0);
    call(sub_82190D80, old_shovel.controls, 1);
  }
  board = FindRootWidget(manager, 0x820B728C, base, true);
  if (board && board == old_selection.board && MouseControls(board, base) == old_selection.controls &&
      MouseSeedBank(board, old_selection.controls, base) == old_selection.bank)
    call(sub_82139AC0, board);
  pvz::CancelServiceMouse(ctx, base, board);
  pvz::CancelChallengeMouse(base, TopRootWidget(manager, base));
  if (mouse_item_cursor.exchange(false)) pvz::RefreshHostSettings();
  const auto held = Read(base, manager + 0x114);
  call(sub_824A6208, manager, -1, -1);
  if (held & 1) call(sub_824A6358, manager, -1, -1, 1);
  if (held & 2) call(sub_824A6358, manager, -1, -1, -1);
  base[manager + 0x108] = 0;
}

void DispatchWidgetEvent(PPCContext& ctx, uint8_t* base,
    const pvz::MouseInputEvent* input, bool lost_focus) {
  const uint32_t manager = ctx.r3.u32;
  static bool logged = false;
  if (REXCVAR_GET(pvz_input_trace) && !logged) {
    REXLOG_INFO("PvZ WidgetManager {:08X} vtable {:08X}: {}x{}", manager,
        Read(base, manager), Read(base, manager + 0x48), Read(base, manager + 0x4C));
    logged = true;
  }
  if (Read(base, manager) != 0x8206404C) return;
  static bool previous_owner = true;
  const bool active_mouse = MouseActive();
  const bool changed_owner = previous_owner != active_mouse;
  previous_owner = active_mouse;
  static std::string active_mode;
  const auto mode = REXCVAR_GET(pvz_cursor_mode);
  if (active_mode != mode) {
    active_mode = mode;
    selection.packet = -1;
    REXLOG_INFO("PvZ cursor mode: {}", mode);
  }
  std::optional<pvz::Point> snapshot;
  ButtonEvent button_event{};
  std::span<const ButtonEvent> pending;
  const bool cancel = input && input->kind == pvz::MouseEventKind::Cancel;
  {
    std::lock_guard lock(pointer_mutex);
    snapshot = pointer; // Only the last consumed event, never future UI input.
  }
  if (input && input->kind == pvz::MouseEventKind::Button && input->point) {
    button_event = {*input->point, input->click, input->down};
    pending = {&button_event, 1};
  }
  if (cancel) CancelMouseGestures(ctx, base, manager);
  // Give nested guest calls their own caller frame and leave this hook's
  // incoming context intact (including CR, FP registers and the return value).
  auto call = [&](auto fn, uint32_t self, int x, int y, int click) {
    PPCContext work = ctx;
    work.r1.u32 -= 0x100;
    work.r3.u32 = self;
    work.r4.s64 = x;
    work.r5.s64 = y;
    work.r6.s64 = click;
    work.r7.u32 = 0;
    RouteMouseGuestCall(fn, work, base);
    const bool previous_collect = explicit_mouse_collect;
    if (fn == sub_8216EF50) explicit_mouse_collect = true;
    fn(work, base);
    explicit_mouse_collect = previous_collect;
    return work.r3.u32;
  };
  const auto focused_root = TopRootWidget(manager, base);
  if (pvz::ApplyReplayFixtures(ctx, base, focused_root, FindRootWidget(manager, 0x820C2784, base))) return;
  auto live_selection = [&]() {
    const auto board = FindRootWidget(manager, 0x820B728C, base, true);
    return board && board == selection.board && MouseControls(board, base) == selection.controls &&
        MouseSeedBank(board, selection.controls, base) == selection.bank;
  };
  auto item_board = [&]() -> uint32_t {
    if (!live_selection()) return 0;
    const auto top = TopRootWidget(manager, base), board = selection.board;
    if (top == board) return board;
    // During the initial shovel lesson Intro keeps its SeedChooser root
    // attached above Board even though card selection has already ended.
    // Allow that specific native cutscene; actual modal dialogs still cancel.
    const auto app = Read(base, board + 0xF4);
    const auto tutorial = Read(base, board + 0x5620);
    if (top && Read(base, top) == 0x820CA94C && app && Read(base, app + 0x9CC) == 0 &&
        Read(base, app + 0x9D0) == 2 && tutorial >= 15 && tutorial <= 17) return board;
    return 0;
  };
  auto clear_item = [&]() {
    const bool live = live_selection();
    selection.packet = -1;
    selection.packets.clear();
    selection.shovel = selection.usable_seed = false;
    if (live) {
      call(sub_82139AC0, selection.board, 0, 0, 0);
      HideUnselectedCursor(selection.controls, base);
    }
  };
  auto cancel_item_drag = [&](const char* reason) {
    if (item_drag.board) {
      discard_item_release = true;
      if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ item drag cancelled {}", reason);
    }
    item_drag = {};
    clear_item();
  };
  if (item_drag.board || (PcMouseMode() && selection.packet >= 0)) {
    if (cancel || !PcMouseMode() || !item_board() ||
        base[selection.board + 0x1D9] || !PlantMouse(selection.controls, base) ||
        (item_drag.board && (item_drag.board != selection.board || item_drag.controls != selection.controls ||
                            item_drag.bank != selection.bank))) {
      if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ item validity cancel {} pc {} live {} top {:08X} vt {:08X} board {:08X} paused {} controls blocked {} intro {:08X}",
          cancel, PcMouseMode(), live_selection(), focused_root,
          focused_root ? Read(base, focused_root) : 0, selection.board,
          live_selection() ? base[selection.board + 0x1D9] : 0, live_selection() ? base[selection.controls + 0xC4] : 0,
          live_selection() ? Read(base, selection.board + 0x1D0) : 0);
      cancel_item_drag("input or screen changed");
    }
    else if (selection.packet >= 0 && !SamePackets(base)) cancel_item_drag("bank changed");
    else if (item_drag.board && snapshot) item_drag.Observe(*snapshot);
  }
  // Both click-click and drag-release use the existing Xbox plant action.
  // Conveyor Use can clear selection inside the native call before returning.
  auto plant_packet = [&](uint32_t board, uint32_t controls) {
    if (!live_selection() || selection.packet < 0 || !SamePackets(base)) return false;
    const auto packet = selection.bank + 0x50 + uint32_t(selection.packet) * 0x7C;
    const auto uses = Read(base, packet + 0x68);
    Write(base, controls + 0xBC, uint32_t(selection.packet));
    const auto app = Read(base, board + 0xF4);
    const auto whack_cursor = Read(base, app + 0x9CC) == 31 ? MouseCursor(board, base) : 0;
    if (whack_cursor) {
      Write(base, whack_cursor + 0x44, Read(base, packet + 0x50));
      Write(base, whack_cursor + 0x4C, 1);
    }
    call(sub_8214CDC0, board, 6, 0, 0);
    if (const auto cursor = whack_cursor ? MouseCursor(board, base) : 0) Write(base, cursor + 0x4C, 7);
    const bool planted = selection.packet < 0 || Read(base, packet + 0x68) != uses;
    if (planted) {
      selection.packet = -1;
      selection.packets.clear();
      HideUnselectedCursor(controls, base);
      if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse planted seed");
    }
    return planted;
  };
  if (beghouled_mouse.board && (cancel || !MouseActive() || focused_root != beghouled_mouse.board ||
      base[focused_root + 0x1D9])) {
    const auto live = FindRootWidget(manager, 0x820B728C, base, true);
    if (live == beghouled_mouse.board)
      if (const auto challenge = Read(base, live + 0x1D4)) base[challenge + 0x28] = 0;
    beghouled_mouse = {};
  }
  if (cancel || !active_mouse || !heavy_weapon_mouse.board ||
      focused_root != heavy_weapon_mouse.board || Read(base, focused_root) != 0x820B728C || base[focused_root + 0x1D9] ||
      Read(base, Read(base, focused_root + 0xF4) + 0x9CC) != 20)
    heavy_weapon_mouse = {};
  if (console_shovel.board) {
    const auto board = FindRootWidget(manager, 0x820B728C, base, true);
    if (cancel || !active_mouse || PcMouseMode() || !snapshot || !pvz::MapBoardPointer(*snapshot) || board != console_shovel.board ||
        !board || base[board + 0x1D9] || MouseControls(board, base) != console_shovel.controls) {
      // Cancelling must not trigger StateExit's completed-hold dig.
      if (console_shovel.held && board && board == console_shovel.board && MouseControls(board, base) == console_shovel.controls) {
        Write(base, console_shovel.controls + 0x70, 0);
        call(sub_82190D80, console_shovel.controls, 1, 0, 0);
        call(sub_82139AC0, board, 0, 0, 0);
      }
      console_shovel = {};
    }
  }
  if (lost_focus && rex::cvar::Query<bool>("pvz_pause_on_focus_loss") &&
      focused_root && Read(base, focused_root) == 0x820B728C) {
    const auto app = Read(base, focused_root + 0xF4);
    if (app && Read(base, app + 0x9D0) == 3 && !base[focused_root + 0x1D9])
      call(sub_8214CDC0, focused_root, 5, 0, 0);
  }
  if (REXCVAR_GET(pvz_mouse_cursor)) pvz::UpdateMouseNavigation(ctx, manager, base);
  pvz::UpdatePcMenus(ctx, base, manager);
  if (pvz::HostMenuOpen()) {
    CancelMouseGestures(ctx, base, manager);
    return;
  }
  if ((!focused_root || Read(base, focused_root) != 0x820B728C ||
      (!selection.shovel && !selection.usable_seed && selection.packet < 0)) && mouse_item_cursor.exchange(false))
    pvz::RefreshHostSettings();
  auto play_sample = [&](uint32_t app, uint32_t global) {
    const auto sound = Read(base, global);
    if (app && int32_t(sound) >= 0) {
      const auto instance = call(rex::runtime::ResolveIndirectFunction(Read(base, Read(base, app) + 0xE0)), app, int(sound), 1, 0);
      if (REXCVAR_GET(pvz_input_trace))
        REXLOG_INFO("PvZ mouse sound {} id {} instance {:08X}", global == 0x82D0A630 ? "seed lift" : "buzzer", sound, instance);
    }
  };
  if (changed_owner) {
    selection.packet = -1;
    console_pointer = {};
    REXLOG_INFO("PvZ input owner: {}", active_mouse ? "mouse" : "controller");
    pvz::RefreshHostSettings();
  }
  auto move = [&](pvz::Point p) {
    // MouseMove is inlined in this build; use its two verified paths.
    const auto top = TopRootWidget(manager, base);
    const auto local = top && Read(base, top) == 0x820C2784 ?
        std::optional<pvz::Point>(p) : pvz::MapUiPointer(p);
    base[manager + 0x108] = local.has_value();
    if (Read(base, manager + 0x114))
      call(sub_824A65A0, manager, local ? int(local->x) : -1, local ? int(local->y) : -1, 0);
    else
      call(sub_824A6208, manager, local ? int(local->x) : -1, local ? int(local->y) : -1, 0);
    if (!PcMouseMode()) {
      if (auto menu = MouseMenu(manager, base); menu && StableMenu(menu, base)) {
        if (auto child = FindMenuButton(menu, MenuButtonAt(menu, p, base), base))
          call(sub_824A5F48, manager, int(child), 0, 0);
      }
      if (local) {
        const auto dialog = DialogParent(Read(base, manager + 0xB0), base);
        if (auto child = DrawnDialogButtonAt(dialog, *local, base))
          call(sub_824A5F48, manager, int(child), 0, 0);
      }
    }
    SyncMouseHover(ctx, manager, p, base);
    pvz::UpdateChallengePointer(ctx, base, top, p);
    pvz::UpdateServicePointer(ctx, base, top, p);
    if (top == beghouled_mouse.board && beghouled_mouse.held && !base[top + 0x1D9]) {
      if (auto lawn = pvz::MapBoardPointer(p)) {
        const auto challenge = Read(base, top + 0x1D4);
        call(sub_82152F20, challenge, int(lawn->x), int(lawn->y), 0);
        if (!base[challenge + 0x28]) beghouled_mouse.selected.reset();
      }
    }
    static int logged_panel = -1;
    auto over = MouseMenu(manager, base);
    if (REXCVAR_GET(pvz_input_trace) && over && Read(base, over) == 0x820C2784 &&
        Read(base, over + 0x118) == Read(base, over + 0x11C) &&
        int(Read(base, over + 0x118)) != logged_panel) {
      logged_panel = int(Read(base, over + 0x118));
      REXLOG_INFO("PvZ menu panel {} offset {}", logged_panel,
          std::bit_cast<float>(Read(base, over + 0x108)));
      const auto head = Read(base, over + 0x18);
      auto node = Read(base, head);
      for (int i = 0; node != head && i < 64; ++i, node = Read(base, node)) {
        const auto child = Read(base, node + 8);
        REXLOG_INFO("PvZ menu child {:08X} vt {:08X} rect {},{},{},{} flags {:08X}/{:08X} visible {}/{}/{}",
            child, Read(base, child), int32_t(Read(base, child + 0x40)), int32_t(Read(base, child + 0x44)),
            Read(base, child + 0x48), Read(base, child + 0x4C), Read(base, child + 0x54),
            Read(base, child + 0x58), base[child + 0x70], base[child + 0x71], base[child + 0x72]);
        if (Read(base, child) == 0x820BA0AC)
          REXLOG_INFO("PvZ GameButton id {} draw {},{} float {} {},{} scale {},{} inset {},{},{},{} size {},{}",
              Read(base, child + 0x6C), int32_t(Read(base, child + 0x190)), int32_t(Read(base, child + 0x194)),
              base[child + 0x198], std::bit_cast<float>(Read(base, child + 0x19C)),
              std::bit_cast<float>(Read(base, child + 0x1A0)),
              std::bit_cast<float>(Read(base, child + 0x1CC)), std::bit_cast<float>(Read(base, child + 0x1D0)),
              int32_t(Read(base, child + 0x90)), int32_t(Read(base, child + 0x94)),
              int32_t(Read(base, child + 0x98)), int32_t(Read(base, child + 0x9C)),
              Read(base, child + 0x1A4), Read(base, child + 0x1A8));
      }
    }
  };
  if (!active_mouse) {
    pvz::UpdateServicePointer(ctx, base, TopRootWidget(manager, base), std::nullopt);
    // End mouse capture first, then restore a live focus for native navigation.
    // Never dereference the previously hovered widget after a screen change.
    if (changed_owner) {
      const auto root = TopRootWidget(manager, base);
      uint32_t first = 0;
      int budget = 256;
      auto clear = [&](auto&& self, uint32_t widget, int depth) -> void {
        if (!widget || depth > 16 || budget-- <= 0) return;
        if (IsGameButton(Read(base, widget))) {
          base[widget + 0x75] = 0;
          if (!first && base[widget + 0x70] && !base[widget + 0x72] && !base[widget + 0x149]) first = widget;
        }
        const auto head = Read(base, widget + 0x18);
        if (head) for (auto node = Read(base, head); node && node != head && budget > 0; node = Read(base, node))
          self(self, Read(base, node + 8), depth + 1);
      };
      clear(clear, root, 0);
      // MainMenu keeps off-screen panel buttons visible in its widget tree.
      // Pick a button from the current panel when PC hover left no focus.
      if (root && Read(base, root) == 0x820C2784 && StableMenu(root, base)) {
        const auto panel = Read(base, root + 0x118);
        const auto id = panel == 0 ? 9 : panel == 2 ? 15 : AdventureButtonId(root, base);
        first = FindMenuButton(root, id, base);
        if (mouse_hovered_button && Read(base, mouse_hovered_button + 0x24) == root) {
          const int hovered = int(Read(base, mouse_hovered_button + 0x6C));
          const bool current_panel = panel == 0 ? hovered >= 9 && hovered <= 14 :
              panel == 2 ? hovered >= 15 && hovered <= 19 : hovered >= 0 && hovered <= 8;
          if (current_panel && FindMenuButton(root, hovered, base) == mouse_hovered_button)
            first = mouse_hovered_button;
        }
      }
      if (root && (Read(base, root) == 0x820B728C || Read(base, root) == 0x820B899C ||
          Read(base, root) == 0x820CB23C || Read(base, root) == 0x820BB58C)) first = root;
      if (first) call(sub_824A5F48, manager, int(first), 0, 0);
      mouse_hovered_button = 0;
      captured_dialog = 0;
      captured_dialog_id = -1;
    }
    return;
  }
  if (snapshot && pending.empty()) move(*snapshot);
  else if (!snapshot) {
    SyncMouseHover(ctx, manager, std::nullopt, base);
    pvz::UpdateChallengePointer(ctx, base, TopRootWidget(manager, base), std::nullopt);
    pvz::UpdateServicePointer(ctx, base, TopRootWidget(manager, base), std::nullopt);
  }
  const auto title = FindRootWidget(manager, 0x820CB6BC, base);
  title_ready.store(title && base[title + 0xED] &&
      Read(base, title + 0xDC) == Read(base, title + 0xE0), std::memory_order_relaxed);
  const auto over = MouseMenu(manager, base);
  const bool over_menu = over != 0;
  menu_ready.store(over_menu && StableMenu(over, base), std::memory_order_relaxed);
  if (over_menu) {
    board_ready.store(false, std::memory_order_relaxed);
    selection = {};
    console_pointer = {};
  }
  const auto root = TopRootWidget(manager, base);
  const int wheel = input && input->kind == pvz::MouseEventKind::Wheel ?
      mouse_wheel.Take(input->delta, root) : 0;
  if (input && input->kind == pvz::MouseEventKind::Wheel && REXCVAR_GET(pvz_input_trace))
    REXLOG_INFO("PvZ mouse wheel event {} delta {} steps {} target {:08X}",
        input->sequence, input->delta, wheel, root ? Read(base, root) : 0);
  pvz::ScrollChallenge(ctx, base, root, wheel);
  pvz::ScrollService(ctx, base, root, wheel);
  if (root && Read(base, root) == 0x820B728C) {
    const auto app = Read(base, root + 0xF4);
    const auto controls = MouseControls(root, base);
    if (!PcMouseMode() && app && controls && (PlantMouse(controls, base) || Read(base, app + 0x9CC) == 20) &&
        Read(base, app + 0x9D0) == 3 && !base[root + 0x1D9]) {
      for (int i = 0; i < std::min(10, std::abs(wheel)); ++i)
        if (Read(base, app + 0x9CC) == 19)
          call(sub_821538D8, Read(base, root + 0x1D4), 8, 0, 0);
        else call(sub_8214CDC0, root, wheel > 0 ? 10 : 11, 0, 0);
    }
  }
  for (const auto& event : pending) {
    if (event.click == 2) {
      if (event.down && !PcMouseMode() && root && Read(base, root) == 0x820B728C &&
          Read(base, Read(base, root + 0xF4) + 0x9CC) == 24 && !base[root + 0x1D9])
        call(sub_821538D8, Read(base, root + 0x1D4), 9, 0, 0);
      continue;
    }
    if (event.click > 0 && !event.down) heavy_weapon_mouse = {};
    move(event.point);
    if (event.click == 1 && event.down && pvz::LocalPlayersActive()) {
      if (const auto waiting = VisibleDescendant(manager, 0x820C0A8C, base)) {
        const auto user = pvz::MouseUserIndex();
        if (user >= 0) call(rex::runtime::ResolveIndirectFunction(Read(base, Read(base, waiting) + 0xC0)),
            waiting, 6, user, 0);
        continue;
      }
    }
    auto help = VisibleDescendant(manager, 0x820BB22C, base);
    if (!help) help = VisibleDescendant(manager, 0x820C1D24, base);
    if (!help) help = VisibleDescendant(manager, 0x820BBF94, base);
    if (!help) help = VisibleDescendant(manager, 0x820C21A4, base);
    if (!help) help = VisibleDescendant(manager, 0x820B96EC, base);
    if (!help) help = VisibleDescendant(manager, 0x820CC2EC, base);
    if (!help) help = VisibleDescendant(manager, 0x820CC0AC, base);
    if (!help) help = VisibleDescendant(manager, 0x820CB23C, base);
    if (!help) help = VisibleDescendant(manager, 0x820B466C, base);
    if (help && TopRootWidget(manager, base) != help) help = 0;
    if (event.click < 0 && help) {
      if (event.down) pvz::CloseMouseHelp(ctx, help, base);
      continue;
    }
    if (event.click < 0 && !event.down && console_shovel.board) {
      const auto board = FindRootWidget(manager, 0x820B728C, base, true);
      if (board && board == console_shovel.board && MouseControls(board, base) == console_shovel.controls) {
        const auto controls = console_shovel.controls, count = Read(base, board + 0x583C);
        const auto before = Read(base, board + 0x5620);
        const auto elapsed = console_shovel.held ? std::bit_cast<float>(Read(base, controls + 0x70)) : 0.0f;
        if (console_shovel.held) {
          call(sub_82190D80, controls, 1, 0, 0);
          call(sub_82139AC0, board, 0, 0, 0);
        }
        if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ console shovel released after {} dug {} tutorial {} -> {}",
            elapsed, Read(base, board + 0x583C) != count, before, Read(base, board + 0x5620));
      }
      console_shovel = {};
      continue;
    }
    const auto active_title = FindRootWidget(manager, 0x820CB6BC, base);
    if (active_title && event.click > 0) {
      if (event.down) {
        call(sub_82219690, active_title, 6, 0, 0);
        if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse title start");
      }
      continue;
    }
    // The framework tracks capture/lifetime. Do not retain guest widget
    // pointers across frames or menu transitions.
    uint32_t widget = Read(base, manager + (event.down ? 0xB0 : 0xAC));
    // Release framework capture before any custom button consumes MouseUp.
    // In a modal stack the release may be over a different widget than the
    // title bar that started the drag; WidgetManager owns that capture.
    if (!event.down) {
      const auto release_root = TopRootWidget(manager, base);
      const auto local = pvz::MapUiPointer(event.point);
      call(sub_824A6358, manager, local ? int(local->x) : -1, local ? int(local->y) : -1, event.click);
      if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ framework release held {}", Read(base, manager + 0x114));
      if (TopRootWidget(manager, base) != release_root) {
        CancelMouseGestures(ctx, base, manager);
        return;
      }
      const auto live_board = FindRootWidget(manager, 0x820B728C, base, true);
      if (captured_board && captured_board == live_board) widget = live_board;
      else widget = Read(base, manager + 0xB0);
      captured_board = 0;
    }
    if (input) {
      std::lock_guard lock(pointer_mutex);
      if (!mouse_events.Current(*input)) return;
    }
    if (event.click > 0 && event.down) discard_item_release = false;
    if (event.click > 0 && !event.down && discard_item_release) {
      discard_item_release = false;
      continue;
    }
    if (item_drag.board && event.click > 0 && !event.down) {
      item_drag.Observe(event.point);
      const auto drag = item_drag;
      item_drag = {};
      const auto board = item_board();
      if (!PcMouseMode() || !live_selection() || board != drag.board ||
          base[board + 0x1D9] || drag.controls != selection.controls || drag.bank != selection.bank ||
          (drag.shovel ? !selection.shovel : selection.packet != drag.packet || !SamePackets(base))) {
        clear_item();
        continue;
      }
      const auto local = pvz::MapBoardPointer(event.point);
      if (!local) { clear_item(); continue; }
      // A short card click keeps the item in hand. The bank/tool bar cannot
      // become a lawn cell through the console's clamped grid conversion.
      if (!drag.moved || !OverPlantingField(drag.bank, *local, base)) continue;
      ApplyPointer(ctx, drag.controls, base, event.point);
      if (drag.shovel) {
        const auto count = Read(base, board + 0x583C);
        call(sub_821388A0, board, int(local->x), int(local->y), 6);
        clear_item();
        if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ item drag shovel dug {}", Read(base, board + 0x583C) != count);
      } else {
        const auto packet = drag.bank + 0x50 + uint32_t(drag.packet) * 0x7C;
        const bool allowed = call(sub_8220F370, packet, 0, 0, 0) != 0;
        const bool planted = allowed && plant_packet(board, drag.controls);
        if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ item drag seed release planted {} at {},{}", planted, local->x, local->y);
      }
      continue;
    }
    // After the shovel lesson, capture can still point at Board while hover
    // is empty. Route both halves of LMB to the live cutscene, independently
    // of those widget hit fields and GamepadControls' gameplay A handling.
    if (event.click > 0) {
      const auto board = FindRootWidget(manager, 0x820B728C, base, true);
      const auto app = board ? Read(base, board + 0xF4) : 0;
      if (app && Read(base, board + 0x5620) == 18 && Read(base, app + 0x9D0) == 2 && int32_t(Read(base, app + 0xA24)) != -1 &&
          !base[board + 0x1D9]) {
        if (event.down) {
          const auto message = Read(base, app + 0xA24);
          if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse Dave cutscene click message {}", message);
          call(sub_8217D798, Read(base, board + 0x1D0), 0, 0, 0);
        }
        continue;
      }
    }
    const uint32_t widget_vtable = widget ? Read(base, widget) : 0;
    auto menu = MouseMenu(manager, base);
    if (menu && Read(base, menu) == 0x820C2784) selection = {};
    if (menu && event.click > 0) {
      const auto id = MenuButtonAt(menu, event.point, base);
      if (event.down) {
        captured_menu = menu;
        captured_button = id;
        if (auto child = FindMenuButton(menu, id, base)) call(sub_824AE388, child, 6, 0, 0);
      }
      else {
        const bool activate = captured_menu == menu && captured_button >= 0 &&
            id == captured_button && FindMenuButton(menu, id, base);
        const auto pressed = captured_menu == menu ? FindMenuButton(menu, captured_button, base) : 0;
        captured_menu = 0;
        captured_button = -1;
        if (activate) {
          if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ menu activate button {}", id);
          call(sub_824AE488, pressed, 6, 0, 0);
        } else if (pressed) base[pressed + 0x74] = 0;
      }
      continue;
    }
    auto chooser = TopRootWidget(manager, base);
    if (const auto imitater = VisibleDescendant(manager, 0x820CAAC4, base)) {
      if (event.down) {
        if (event.click < 0) call(sub_822074F0, imitater, 7, int(Read(base, imitater + 0x1CC)), 0);
        else if (auto local = pvz::MapBoardPointer(event.point)) {
          const auto parent = Read(base, imitater + 0x24);
          const int x = int(local->x) - int32_t(Read(base, imitater + 0x40)) - (parent ? int32_t(Read(base, parent + 0x40)) : 0);
          const int y = int(local->y) - int32_t(Read(base, imitater + 0x44)) - (parent ? int32_t(Read(base, parent + 0x44)) : 0);
          const int seed = int(call(sub_82206970, imitater, x, y, 0));
          if (seed >= 0) {
            Write(base, imitater + 0x1C8, uint32_t(seed));
            call(sub_822074F0, imitater, 6, int(Read(base, imitater + 0x1CC)), 0);
            if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ imitater copy seed {}", seed);
          }
        }
      }
      continue;
    }
    if (auto award = VisibleDescendant(manager, 0x820B4F44, base)) chooser = award;
    else if (auto choices = VisibleDescendant(manager, 0x820CA94C, base)) chooser = choices;
    for (auto ancestor = widget, depth = 0u; ancestor && depth < 16; ++depth) {
      const auto vt = Read(base, ancestor);
      if (vt == 0x820CA94C || vt == 0x820B4F44) { chooser = ancestor; break; }
      ancestor = Read(base, ancestor + 0x24);
    }
    const auto chooser_top = TopRootWidget(manager, base);
    if (InactiveSeedChooser(chooser, base)) chooser = 0;
    if (VisibleDescendant(manager, 0x820CAAC4, base) || (chooser_top &&
        Read(base, chooser_top) != 0x820BA804 && Read(base, chooser_top) != 0x820CA94C &&
        Read(base, chooser_top) != 0x820B728C && Read(base, chooser_top) != 0x820B4F44)) chooser = 0;
    if (chooser && Read(base, chooser) == 0x820B4F44 && event.click > 0) {
      const auto button = Read(base, chooser + 0xD8);
      const auto local = pvz::MapBoardPointer(event.point);
      const bool over = button && local && AwardButtonAt(chooser, *local, base);
      if (event.down) { captured_dialog = over ? chooser : 0; captured_dialog_id = -2; }
      else {
        const bool activate = captured_dialog == chooser && captured_dialog_id == -2 && over;
        captured_dialog = 0; captured_dialog_id = -1;
        if (activate) {
          if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse NEXT LEVEL");
          call(sub_8212C4D0, chooser, 6, 0, 0);
        }
      }
      continue;
    }
    const auto chooser_ui = pvz::MapUiPointer(event.point);
    const bool chooser_button = chooser && chooser_ui && DrawnDialogButtonAt(chooser, *chooser_ui, base);
    if (chooser && Read(base, chooser) == 0x820CA94C && event.click != 2 && event.down && !chooser_button) {
      if (Read(base, chooser + 0xE9C) != 0 && event.click == 1) {
        call(sub_8220C280, chooser, 6, 0, 0); continue;
      }
      if (auto local = pvz::MapBoardPointer(event.point)) {
        const int seed = int(call(sub_8220AFA8, chooser,
            int(local->x) - int32_t(Read(base, chooser + 0x40)),
            int(local->y) - int32_t(Read(base, chooser + 0x44)), 0));
        if (seed >= 0) {
          const auto app = Read(base, chooser + 0xE60);
          const int index = seed - (base[chooser + 0xEB8] ? 61 : 0);
          if (index >= 0 && index < 49) {
            const auto chosen = chooser + 0xE0 + uint32_t(index) * 0x40;
            const auto state = Read(base, chosen + 0x28);
            if (event.click < 0 && state != 1) continue;
            if (call(sub_82208A80, chooser, seed, 0, 0) ||
                (state == 1 && base[chosen + 0x3C])) play_sample(app, 0x82D0AA24);
            else if (state == 1) call(sub_8220B1C0, chooser, int(chosen), 0, 0);
            else if (index == 48 && state != 1) {
              Write(base, chooser + 0xEAC + (pvz::LocalPlayersActive() ? uint32_t(pvz::MouseUserIndex()) * 4 : 0), 48);
              call(sub_8220C280, chooser, 6, int(Read(base, chooser + 0xEA8)), 0);
            }
            else if (state == 3) call(sub_8220B440, chooser, int(chosen), 0, 0);
            if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ chooser click seed {} state {}", seed, state);
            Write(base, chooser + 0xE84, uint32_t(-1));
          }
          continue;
        }
      }
    }
    auto dialog = DialogParent(Read(base, manager + 0xB0), base);
    const auto active_screen = TopRootWidget(manager, base);
    if (active_screen && Read(base, active_screen) != 0x820C2784 && Read(base, active_screen) != 0x820B728C)
      dialog = active_screen;
    const auto dialog_local = pvz::MapUiPointer(event.point);
    if (help && dialog_local && DrawnDialogButtonAt(help, *dialog_local, base)) dialog = help;
    if (dialog_local && DrawnDialogButtonAt(TopRootWidget(manager, base), *dialog_local, base))
      dialog = TopRootWidget(manager, base);
    if (chooser && Read(base, chooser) == 0x820CA94C && dialog_local &&
        DrawnDialogButtonAt(chooser, *dialog_local, base)) dialog = chooser;
    const auto dialog_button = dialog_local ? DrawnDialogButtonAt(dialog, *dialog_local, base) : 0;
    if (PcMouseMode() && event.click == 1 && event.down && dialog_button && Read(base, dialog) == 0x820B728C) {
      const bool carried = selection.board == dialog && (selection.packet >= 0 || selection.shovel || selection.usable_seed);
      if (pvz::CancelCarriedGardenTool(ctx, base, dialog) || carried) {
        if (carried) cancel_item_drag("button click");
        discard_item_release = true;
        play_sample(Read(base, dialog + 0xF4), 0x82D0A43C); // SOUND_GRAVEBUTTON.
        continue;
      }
    }
    if (event.click > 0 && ((event.down && dialog_button) || (!event.down && captured_dialog))) {
      if (event.down) {
        captured_dialog = dialog;
        captured_dialog_id = int(Read(base, dialog_button + 0x6C));
        call(sub_824AE388, dialog_button, 6, 0, 0);
      } else {
        const auto pressed_dialog = captured_dialog;
        const auto pressed_id = captured_dialog_id;
        captured_dialog = 0;
        captured_dialog_id = -1;
        if (dialog == pressed_dialog) {
          if (auto child = FindMenuButton(dialog, pressed_id, base)) {
            if (child == dialog_button) {
              if (REXCVAR_GET(pvz_input_trace))
                REXLOG_INFO("PvZ dialog activate button {}", pressed_id);
              call(sub_824AE488, child, 6, 0, 0);
            } else base[child + 0x74] = 0;
          }
        }
      }
      continue;
    }
    // The lawn's draw offset is absent from Board's Widget rectangle. Route
    if (pvz::ChallengeMouseButton(ctx, base, TopRootWidget(manager, base), event.point, event.click, event.down)) continue;
    if (pvz::ServiceMouseButton(ctx, base, TopRootWidget(manager, base), event.point, event.click, event.down)) continue;
    // visible lawn clicks after painted buttons and modal screens were hit.
    const auto live_field = TopRootWidget(manager, base);
    if (live_field && Read(base, live_field) == 0x820B728C && !base[live_field + 0x1D9] &&
        pvz::MapBoardPointer(event.point)) widget = live_field;
    if (widget && Read(base, widget) == 0x820B728C) {
      // Board's mouse methods mostly serve Zen Garden in the console build.
      // Its gamepad button methods own planting/cancelling and player routing.
      const uint32_t primary = MouseControls(widget, base);
      const uint32_t app = Read(base, widget + 0xF4);
      if (pvz::LocalPlayersActive() && event.down && primary &&
          Read(base, primary + 0xA0) == uint32_t(-1)) {
        // Start joins/reconnects through Board's own two-player controller
        // binding; the following click belongs to that player's controls.
        call(sub_8214CDC0, widget, 5, pvz::MouseUserIndex(), 0);
        if (Read(base, primary + 0xA0) == uint32_t(-1)) {
          call(sub_8214CDC0, widget, 6, pvz::MouseUserIndex(), 0);
          continue;
        }
      }
      // Zen Garden has its own native mouse actions; avoid dispatching twice.
      if (app && Read(base, app + 0x9CC) != 44 && primary &&
          Read(base, primary) == 0x820BA5E8 && int32_t(Read(base, primary + 0xA0)) == pvz::MouseUserIndex()) {
        ApplyPointer(ctx, primary, base, event.point);
        // Dave and scripted introductions own A. Card picking must not eat
        // that input while the native cutscene is waiting for a click.
        const int tutorial = int32_t(Read(base, widget + 0x5620));
        if (event.click > 0 && ((Read(base, app + 0x9CC) == 0 && Read(base, app + 0x9D0) == 2 && (tutorial < 15 || tutorial > 17)) ||
            int32_t(Read(base, app + 0xA24)) != -1)) {
          if (event.down) call(sub_8214CDC0, widget, 6, 0, 0);
          continue;
        }
        const int mini_mode = int(Read(base, app + 0x9CC));
        if (!PcMouseMode() && mini_mode == 24 && event.click == 1) {
          const auto local = pvz::MapBoardPointer(event.point);
          const auto bank = Read(base, widget + 0x1A4);
          if (local && bank && event.down && !base[widget + 0x1D9]) {
            const int packet = PacketAt(bank, *local, base);
            if (packet >= 0) Write(base, primary + 0xBC, packet);
            else {
              const auto hit = ctx.r1.u32 - 0x80;
              Write(base, hit, 0); Write(base, hit + 4, 0);
              call(sub_82139238, widget, int(local->x), int(local->y), int(hit));
              call(sub_82153300, Read(base, widget + 0x1D4), int(local->x), int(local->y), int(hit));
            }
          }
          continue;
        }
        if (mini_mode == 24 && event.click < 0) {
          if (event.down && !base[widget + 0x1D9]) {
            const auto challenge = Read(base, widget + 0x1D4);
            if (PcMouseMode()) call(sub_821538D8, challenge, 9, 0, 0);
            else {
              const auto bank = Read(base, widget + 0x1A4);
              const auto selected = Read(base, primary + 0xBC);
              if (bank && selected < Read(base, bank + 0x40) && selected < 10)
                call(sub_8220F510, bank + 0x50 + selected * 0x7C, 0, 0, 0);
            }
          }
          continue;
        }
        if (!PcMouseMode() && (mini_mode == 21 || mini_mode == 25) && PlantMouse(primary, base)) {
          const auto challenge = Read(base, widget + 0x1D4);
          const auto local = pvz::MapBoardPointer(event.point);
          if (!event.down) {
            if (mini_mode == 21) { base[challenge + 0x28] = 0; beghouled_mouse.held = false; }
            continue;
          }
          if (event.click < 0) {
            const auto selected = Read(base, primary + 0xBC);
            if (selected < Read(base, selection.bank + 0x40) && selected < 10)
              call(sub_8220F510, selection.bank + 0x50 + selected * 0x7C, 0, 0, 0);
          } else if (local && PacketAt(selection.bank, *local, base) < 0) {
            const auto hit = ctx.r1.u32 - 0x80;
            Write(base, hit, 0); Write(base, hit + 4, 0);
            call(sub_82139238, widget, int(local->x), int(local->y), int(hit));
            call(sub_82153300, challenge, int(local->x), int(local->y), int(hit));
            if (mini_mode == 21 && base[challenge + 0x28]) beghouled_mouse = {widget, *local, true};
          } else if (local) {
            const int packet = PacketAt(selection.bank, *local, base);
            if (packet >= 0) Write(base, primary + 0xBC, packet);
          }
          continue;
        }
        if (!PcMouseMode() && mini_mode == 31 && PlantMouse(primary, base)) {
          const auto local = pvz::MapBoardPointer(event.point);
          if (!event.down || !local || base[widget + 0x1D9]) continue;
          const int packet = PacketAt(selection.bank, *local, base);
          if (packet >= 0 && event.click == 1) Write(base, primary + 0xBC, packet);
          else if (event.click == 1) call(sub_82153018, Read(base, widget + 0x1D4), int(local->x), int(local->y), 0);
          else {
            const auto cursor = MouseCursor(widget, base);
            const auto selected = Read(base, primary + 0xBC);
            if (cursor && selected < Read(base, selection.bank + 0x40) && selected < 10) {
              call(sub_82190D80, primary, 1, 0, 0);
              Write(base, cursor + 0x44, Read(base, selection.bank + 0x50 + selected * 0x7C + 0x50));
              Write(base, cursor + 0x4C, 1);
              call(sub_8214CDC0, widget, 6, 0, 0);
              Write(base, cursor + 0x4C, 7);
            }
          }
          continue;
        }
        if (Read(base, app + 0x9CC) == 20) {
          if (event.down) captured_board = widget;
          // A packet buys an upgrade immediately in this mode; it must never
          // also become a lawn A press or begin a firing gesture.
          if (event.down && (event.click > 0 || !PcMouseMode()) && !base[widget + 0x1D9] && Read(base, app + 0x9D0) == 3) {
            const auto local = pvz::MapBoardPointer(event.point);
            const auto bank = MouseSeedBank(widget, primary, base);
            if (local && bank && Read(base, bank) == 0x820CAED8) {
              const int packet = PacketAt(bank, *local, base);
              const bool in_bank = pvz::InRect(*local, int32_t(Read(base, bank + 0x24)),
                  int32_t(Read(base, bank + 0x28)), Read(base, bank + 0x2C), Read(base, bank + 0x30));
              heavy_weapon_mouse = {widget, event.click == 1 && !in_bank};
              const int upgrade = !PcMouseMode() && event.click < 0 ? int(Read(base, primary + 0xBC)) : packet;
              if (upgrade >= 0 && upgrade < int(Read(base, bank + 0x40)) && (PcMouseMode() || event.click < 0)) {
                heavy_weapon_mouse.firing = false;
                const auto sun = Read(base, widget + 0x55F4);
                const auto challenge = Read(base, widget + 0x1D4);
                Write(base, primary + 0xBC, uint32_t(upgrade));
                call(sub_8220F510, bank + 0x50 + uint32_t(upgrade) * 0x7C, 0, 0, 0);
                if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ Heavy Weapon buy packet {} sun {} -> {} weapon {} missiles {}",
                    upgrade, sun, Read(base, widget + 0x55F4), Read(base, challenge + 0x130), Read(base, challenge + 0x12C));
              }
              else if (!PcMouseMode() && packet >= 0) Write(base, primary + 0xBC, packet);
              if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ Heavy Weapon LMB firing {}", heavy_weapon_mouse.firing);
            }
          } else if (!event.down && event.click > 0 && REXCVAR_GET(pvz_input_trace))
            REXLOG_INFO("PvZ Heavy Weapon LMB released");
          continue;
        }
        if (PcMouseMode() && PlantMouse(primary, base)) {
          const int game_mode = int(Read(base, app + 0x9CC));
          const auto challenge = Read(base, widget + 0x1D4);
          if (!event.down && game_mode == 21 && beghouled_mouse.board == widget) {
            base[challenge + 0x28] = 0;
            beghouled_mouse.held = false;
          }
          if (event.down) {
            if (event.click < 0) {
              const bool carried = selection.packet >= 0 || selection.usable_seed || selection.shovel;
              cancel_item_drag("right click");
              play_sample(app, 0x82D0A43C);
              if (!carried && call(sub_821A8770, app, 0, 0, 0)) {
                if (auto local = pvz::MapBoardPointer(event.point)) {
                  const auto hit = ctx.r1.u32 - 0x80;
                  Write(base, hit, 0); Write(base, hit + 4, 0);
                  call(sub_82139238, widget, int(local->x), int(local->y), int(hit));
                  call(sub_82153300, challenge, int(local->x), int(local->y), int(hit));
                }
              }
              if (game_mode == 21) { base[challenge + 0x28] = 0; beghouled_mouse = {}; }
              if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse cancel seed");
            } else if (const auto local = pvz::MapBoardPointer(event.point)) {
              // HitResult lives in this nested call's reserved caller frame.
              const uint32_t hit = ctx.r1.u32 - 0x80;
              Write(base, hit, 0);
              Write(base, hit + 4, 0);
              call(sub_82139238, widget, int(local->x), int(local->y), int(hit));
              if (Read(base, hit + 4) == 5) {
                selection.packet = -1;
                selection.packets.clear();
                selection.usable_seed = false;
                selection.shovel = !selection.shovel && call(sub_82139808, widget, 5, 0, 0);
                HideUnselectedCursor(primary, base);
                item_drag = {};
                if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ PC shovel selected {}", selection.shovel);
                continue;
              }
              if (selection.shovel) {
                const auto count = Read(base, widget + 0x583C);
                call(sub_821388A0, widget, int(local->x), int(local->y), 6);
                selection.shovel = false;
                HideUnselectedCursor(primary, base);
                if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ PC shovel dug {}", Read(base, widget + 0x583C) != count);
                continue;
              }
              const auto coin = Read(base, hit + 4) == 3 ? Read(base, hit) : 0;
              const auto type = coin ? Read(base, coin + 0x80) : 0;
              if (coin && ((type >= 1 && type <= 7) || type == 16)) {
                const auto before = Read(base, coin + 0x84);
                call(sub_8216EF50, coin, 0, 0, 0);
                if (type == 16) {
                  selection.packet = -1;
                  selection.usable_seed = MouseCursorType(widget, base) == 2;
                  if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse dropped seed picked {}", selection.usable_seed);
                }
                if (REXCVAR_GET(pvz_input_trace) && before != 7 && Read(base, coin + 0x84) == 7)
                  REXLOG_INFO("PvZ mouse collect coin type {} at {},{}", type, local->x, local->y);
                // Collection is a discrete click. Native Board MouseDown starts
                // a held drag collector, so it must not also receive this click.
                continue;
              }
              const auto index = game_mode == 19 ? -1 : PacketAt(selection.bank, *local, base);
              if (index >= 0) {
                const auto packet = selection.bank + 0x50 + uint32_t(index) * 0x7C;
                if (game_mode == 21 || game_mode == 24 || game_mode == 25) {
                  if (game_mode == 21) { base[challenge + 0x28] = 0; beghouled_mouse = {}; }
                  const auto sun = Read(base, widget + 0x55F4);
                  call(sub_8220F510, packet, 0, 0, 0);
                  if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ minigame {} buy packet {} type {} active {} sun {} -> {} upgrades {}/{}/{}", game_mode, index, Read(base, packet + 0x50), base[packet + 0x64], sun, Read(base, widget + 0x55F4), base[challenge + 0x6A], base[challenge + 0x6B], base[challenge + 0x6C]);
                  continue;
                }
                if (selection.packet == index) {
                  clear_item();
                  if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse toggle packet {} off", index);
                  continue;
                }
                const bool allowed = call(sub_8220F370, packet, 0, 0, 0) != 0;
                if (allowed) {
                  selection.shovel = false;
                  selection.usable_seed = false;
                  play_sample(app, 0x82D0A630); // SOUND_SEEDLIFT, verified Xbox resource assignment.
                  Write(base, primary + 0xBC, uint32_t(index));
                  Write(base, packet + 0x70, 0); // Restart native selection scale animation.
                  selection.packet = index;
                  call(sub_82190D80, primary, 1, 0, 0);
                  ApplyPointer(ctx, primary, base, event.point);
                  RememberPackets(base);
                  item_drag = {widget, primary, selection.bank, index, false, false, event.point};
                  if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ item drag seed begin packet {}", index);
                } else play_sample(app, 0x82D0AA24);
                if (REXCVAR_GET(pvz_input_trace))
                  REXLOG_INFO("PvZ mouse pick packet {} allowed {}", index, allowed);
              } else if (game_mode == 21 || game_mode == 24 || game_mode == 25 ||
                  (game_mode == 19 && Read(base, hit + 4) == 22) ||
                  (game_mode == 31 && selection.packet < 0 && !selection.usable_seed)) {
                const auto previous = beghouled_mouse.selected;
                if (game_mode == 21) {
                  if (beghouled_mouse.board != widget) beghouled_mouse = {widget};
                  const auto grid = [&](pvz::Point p) {
                    return std::pair(int(call(sub_82149538, widget, int(p.x), int(p.y), 0)),
                        int(call(sub_821495B0, widget, int(p.x), int(p.y), 0)));
                  };
                  const auto current_cell = grid(*local);
                  const auto previous_cell = previous ? grid(*previous) : std::pair(-99, -99);
                  if (previous && Read(base, hit + 4) == 1 && std::abs(current_cell.first - previous_cell.first) +
                      std::abs(current_cell.second - previous_cell.second) == 1) {
                    Write(base, challenge + 0x2C, uint32_t(int(previous->x)));
                    Write(base, challenge + 0x30, uint32_t(int(previous->y)));
                    base[challenge + 0x28] = 1;
                    call(sub_821516D8, challenge, int(local->x), int(local->y), 0);
                    beghouled_mouse.selected.reset();
                    beghouled_mouse.held = false;
                    continue;
                  }
                }
                call(sub_82153300, challenge, int(local->x), int(local->y), int(hit));
                if (game_mode == 21 && base[challenge + 0x28]) {
                  beghouled_mouse = {widget, *local, true};
                }
                if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ minigame {} field click {},{} hit {} state {}", game_mode, local->x, local->y, Read(base, hit + 4), Read(base, challenge + 0x74));
              } else if (Read(base, hit + 4) == 23) {
                // All Vasebreaker variants share Challenge's native vase hit
                // handler. Picking its dropped seed remains a separate click.
                const auto handled = call(sub_82153300, challenge, int(local->x), int(local->y), int(hit));
                if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ Vasebreaker click mode {} handled {}", game_mode, handled);
              } else if (selection.usable_seed) {
                call(sub_8214CDC0, widget, 6, 0, 0);
                selection.usable_seed = MouseCursorType(widget, base) == 2;
                if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse dropped seed placed {}", !selection.usable_seed);
                HideUnselectedCursor(primary, base);
              } else if (selection.packet >= 0 &&
                  uint32_t(selection.packet) < Read(base, selection.bank + 0x40)) {
                plant_packet(widget, primary);
              }
            }
          }
        } else {
          // Card clicks change the console's persistent selection without
          // also firing A on the lawn. Selection need not be affordable yet.
          const auto local = pvz::MapBoardPointer(event.point);
          const auto packet = Read(base, app + 0x9CC) != 19 && PlantMouse(primary, base) && local ?
              PacketAt(selection.bank, *local, base) : -1;
          if (!PcMouseMode() && Read(base, app + 0x9CC) == 19 && event.click > 0 && local) {
            if (event.down) {
              const auto hit = ctx.r1.u32 - 0x80;
              call(sub_82139238, widget, int(local->x), int(local->y), int(hit));
              if (Read(base, hit + 4) == 22) {
                call(sub_82153300, Read(base, widget + 0x1D4), int(local->x), int(local->y), int(hit));
                continue;
              }
            } else if (local->y < 80) continue;
          }
          if (!PcMouseMode() && event.click > 0 && packet >= 0) {
            if (event.down) {
              const auto game_mode = Read(base, app + 0x9CC);
              if (game_mode == 21 || game_mode == 24 || game_mode == 25) {
                call(sub_8220F510, selection.bank + 0x50 + uint32_t(packet) * 0x7C, 0, 0, 0);
                continue;
              }
              if (call(sub_8220F370, selection.bank + 0x50 + uint32_t(packet) * 0x7C, 0, 0, 0))
                play_sample(app, 0x82D0A630);
              else play_sample(app, 0x82D0AA24);
              Write(base, primary + 0xBC, uint32_t(packet));
              Write(base, selection.bank + 0x50 + uint32_t(packet) * 0x7C + 0x70, 0);
              call(sub_82190D80, primary, 1, 0, 0);
              if (REXCVAR_GET(pvz_input_trace))
                REXLOG_INFO("PvZ console select packet {}", packet);
            }
          } else if (event.click < 0 && PlantMouse(primary, base) &&
              MouseCursorType(widget, base) == 2) {
            if (event.down) { call(sub_82139AC0, widget, 0, 0, 0); play_sample(app, 0x82D0A43C); }
          } else if (event.click < 0 && PlantMouse(primary, base) && base[widget + 0x5691]) {
            if (event.down && local && !base[widget + 0x1D9] &&
                (Read(base, app + 0x9D0) == 3 || (tutorial >= 15 && tutorial <= 17))) {
              const auto count = Read(base, widget + 0x583C);
              // Mouse digging uses the preserved native tool action on press.
              call(sub_821388A0, widget, int(local->x), int(local->y), 6);
              console_shovel = {};
              if (tutorial >= 15 && tutorial <= 17 && call(sub_82134748, widget, 0, 0, 0) == 0)
                call(sub_8213D0F0, widget, 18, 0, 0);
              if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ console shovel press completed {} tutorial {} -> {}",
                  Read(base, widget + 0x583C) != count, tutorial, Read(base, widget + 0x5620));
            }
          } else {
            const auto bank = Read(base, widget + 0x1A4);
            const auto selected = Read(base, primary + 0xBC);
            const auto packet_address = bank && Read(base, bank) == 0x820CAED8 &&
                selected < Read(base, bank + 0x40) && selected < 10 ?
                bank + 0x50 + selected * 0x7C : 0;
            const auto uses = packet_address ? Read(base, packet_address + 0x68) : 0;
            call(event.down ? sub_8214CDC0 : sub_8214D1B8, widget,
                event.click > 0 ? 6 : 7, 0, 0);
            if (REXCVAR_GET(pvz_input_trace) && packet_address &&
                Read(base, packet_address + 0x68) != uses)
              REXLOG_INFO("PvZ console planted seed");
          }
        }
        // Planting and collection are handled above. Do not start Board mouse drag
        // as well: it collects sun continuously and can swallow UI clicks.
        if (PlantMouse(primary, base)) {
          if (event.down) captured_board = widget;
          continue;
        }
      }
    }
    const auto local = pvz::MapUiPointer(event.point);
    if (event.down && local)
      call(sub_824A6428, manager, int(local->x), int(local->y), event.click);
    if (REXCVAR_GET(pvz_input_trace))
      REXLOG_INFO("PvZ mouse {} ({}, {}) button {} widget {:08X} vtable {:08X}",
          event.down ? "down" : "up", event.point.x, event.point.y, event.click, widget,
          widget_vtable);
  }
}

void DispatchWidgets(PPCContext& ctx, uint8_t* base) {
  if (Read(base, ctx.r3.u32) != 0x8206404C) return;
  static bool enabled_before = false;
  const bool enabled = REXCVAR_GET(pvz_mouse_cursor);
  {
    std::lock_guard lock(pointer_mutex);
    mouse_events.SetMode(REXCVAR_GET(pvz_cursor_mode) == "pc");
    if (enabled_before && !enabled) mouse_events.Cancel(pvz::MouseCancelReason::Disabled);
    enabled_before = enabled;
    if (!enabled && !mouse_events.Pending()) return;
  }
  static uint32_t previous_root = 0;
  // A screen change ends every gesture that started on the previous screen.
  auto track_root = [&] {
    const auto root = TopRootWidget(ctx.r3.u32, base);
    if (previous_root && previous_root != root) CancelMouseGestures(ctx, base, ctx.r3.u32);
    previous_root = root;
  };
  bool dispatched = false;
  // Pop one at a time: a purchase callback can enter a nested modal loop.
  // Such a loop must still receive new input, and cancellations invalidate its
  // caller's old generation without retaining pointers into a drained batch.
  for (size_t i = 0; i < pvz::MouseInputStream::kCapacity; ++i) {
    std::optional<pvz::MouseInputEvent> event;
    bool lost_focus = false;
    {
      std::lock_guard lock(pointer_mutex);
      mouse_events.SetMode(REXCVAR_GET(pvz_cursor_mode) == "pc");
      event = mouse_events.Take();
      if (!event) break;
      if (!mouse_events.Current(*event)) continue;
      lost_focus = mouse_events.TakeFocusLost();
      pointer = event->point;
    }
    track_root();
    if (event->kind == pvz::MouseEventKind::Button &&
        !mouse_buttons.Accept(event->click, event->down)) {
      if (REXCVAR_GET(pvz_input_trace))
        REXLOG_INFO("PvZ mouse unmatched button {} generation {} ignored", event->sequence, event->generation);
      continue;
    }
    if (REXCVAR_GET(pvz_input_trace))
      REXLOG_INFO("PvZ mouse event {} generation {} kind {} point {},{} click {} down {} delta {} cancel {}",
          event->sequence, event->generation, int(event->kind),
          event->point ? event->point->x : -1, event->point ? event->point->y : -1,
          event->click, event->down, event->delta, int(event->reason));
    DispatchWidgetEvent(ctx, base, &*event, lost_focus);
    dispatched = true;
  }
  if (!dispatched && enabled) {
    track_root();
    DispatchWidgetEvent(ctx, base, nullptr, false);
  }
  const auto ready_root = TopRootWidget(ctx.r3.u32, base);
  replay_screen.store(ready_root ? Read(base, ready_root) : 0, std::memory_order_relaxed);
  bool queue_empty;
  { std::lock_guard lock(pointer_mutex); queue_empty = !mouse_events.Pending(); }
  replay_input_idle.store(queue_empty && !Read(base, ctx.r3.u32 + 0x114) &&
      !item_drag.board && !captured_menu && !captured_dialog && !console_shovel.held &&
      !beghouled_mouse.held && !heavy_weapon_mouse.firing, std::memory_order_relaxed);

}

void ApplyPointer(PPCContext& ctx, uint32_t controls, uint8_t* base, std::optional<pvz::Point> position) {
  if (!MouseActive()) return;
  const auto read = [&](uint32_t at) { return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + at)); };
  // Verified against this XEX's RTTI and GamepadControls::Update disassembly.
  // No guessed or PC-version offsets: primary player, matching concrete class.
  if (read(controls) != 0x820BA5E8 || int32_t(read(controls + 0xA0)) != pvz::MouseUserIndex()) return;
  std::optional<pvz::Point> snapshot;
  if (position) snapshot = position;
  else { std::lock_guard lock(pointer_mutex); snapshot = pointer; }
  if (!snapshot) return;
  snapshot = pvz::MapBoardPointer(*snapshot);
  if (!snapshot) return;
  const bool pc = PcMouseMode();
  if (!pc && console_pointer.controls == controls &&
      console_pointer.raw.x == snapshot->x && console_pointer.raw.y == snapshot->y) return;
  auto write_float = [&](uint32_t offset, float value) {
    *reinterpret_cast<uint32_t*>(base + controls + offset) =
        __builtin_bswap32(std::bit_cast<uint32_t>(value));
  };
  auto state = [&](int value) {
    PPCContext work = ctx;
    work.r1.u32 -= 0x100;
    work.r3.u32 = controls;
    work.r4.s64 = value;
    sub_82190D80(work, base);
  };
  const auto app = read(controls + 0x20);
  if (app && read(app + 0x9CC) == 20) {
    // Keep ordinary hit tests working; Heavy Weapon applies velocity itself.
    write_float(0x74, snapshot->x);
    write_float(0x78, snapshot->y);
    return;
  }
  if (pc) {
    if (console_pointer.controls == controls) {
      state(1); // Stop the previous console attraction when changing modes.
      console_pointer = {};
    }
  }
  // StateEnter(1), when leaving state 4, copies the old snap target to
  // 74/78. Write the PC position afterwards so even that first click is exact.
  write_float(0x74, snapshot->x);
  write_float(0x78, snapshot->y);
  write_float(0x7C, 0);
  write_float(0x80, 0);
  if (!pc) {
    console_pointer = {controls, *snapshot};
    const auto board = read(controls + 0x44);
    const auto app = read(controls + 0x20);
    if (PlantMouse(controls, base) && app && read(app + 0x9D0) == 3 &&
        int32_t(read(board + 0x56A0)) < 0 && !base[board + 0x1D9] &&
        read(controls + 0x68) >= 1 && read(controls + 0x68) <= 4) {
      const auto bank = MouseSeedBank(board, controls, base);
      const bool lawn = snapshot->y >= int32_t(read(bank + 0x28)) + int32_t(read(bank + 0x30));
      state(lawn ? 4 : 1); // Native release-to-grid attraction; bank stays free.
      if (REXCVAR_GET(pvz_input_trace) && lawn)
        REXLOG_INFO("PvZ console pointer {},{} target {},{}", snapshot->x, snapshot->y,
            std::bit_cast<float>(read(controls + 0x58)), std::bit_cast<float>(read(controls + 0x5C)));
    }
  }
}
}

void pvz::PublishPointer(std::optional<Point> position) {
  if (pvz::LocalPlayersActive() && pvz::MouseUserIndex() < 0) return;
  std::lock_guard lock(pointer_mutex);
  if (!REXCVAR_GET(pvz_mouse_cursor)) return;
  mouse_events.SetMode(REXCVAR_GET(pvz_cursor_mode) == "pc");
  if (mouse_events.Move(position)) mouse_owner.store(true, std::memory_order_relaxed);
}

void pvz::PublishButton(Point position, int click_count, bool down) {
  if (!REXCVAR_GET(pvz_mouse_cursor)) return;
  if (down && click_count > 0 && RequestIntroSkip()) return;
  if (down) pvz::ObserveLocalMouse();
  if (pvz::LocalPlayersActive() && pvz::MouseUserIndex() < 0) return;
  std::lock_guard lock(pointer_mutex);
  mouse_events.SetMode(REXCVAR_GET(pvz_cursor_mode) == "pc");
  if (down) mouse_owner.store(true, std::memory_order_relaxed);
  mouse_events.Button(position, click_count, down);
}
void pvz::PublishWheel(Point position, int delta) {
  if (pvz::LocalPlayersActive() && pvz::MouseUserIndex() < 0) return;
  std::lock_guard lock(pointer_mutex);
  if (!REXCVAR_GET(pvz_mouse_cursor)) return;
  mouse_events.SetMode(REXCVAR_GET(pvz_cursor_mode) == "pc");
  if (delta) mouse_owner.store(true, std::memory_order_relaxed);
  mouse_events.Wheel(position, delta);
}
void pvz::NotifyMouseModeChanged() {
  std::lock_guard lock(pointer_mutex);
  mouse_events.SetMode(REXCVAR_GET(pvz_cursor_mode) == "pc");
}
void pvz::PublishReplayMouseBatch(std::span<const MouseInputEvent> events) {
  if (rex::cvar::Query<std::string>("pvz_input_replay").empty() || !REXCVAR_GET(pvz_mouse_cursor)) return;
  if (std::any_of(events.begin(), events.end(), [](const auto& event) {
        return event.kind == MouseEventKind::Button && event.down;
      })) pvz::ObserveLocalMouse();
  if (pvz::LocalPlayersActive() && pvz::MouseUserIndex() < 0) return;
  std::lock_guard lock(pointer_mutex);
  mouse_events.SetMode(REXCVAR_GET(pvz_cursor_mode) == "pc");
  for (const auto& event : events) {
    switch (event.kind) {
      case MouseEventKind::Move:
        if (mouse_events.Move(event.point)) mouse_owner.store(true, std::memory_order_relaxed);
        break;
      case MouseEventKind::Button:
        if (event.point) {
          if (event.down) mouse_owner.store(true, std::memory_order_relaxed);
          mouse_events.Button(*event.point, event.click, event.down);
        }
        break;
      case MouseEventKind::Wheel:
        if (event.point) {
          if (event.delta) mouse_owner.store(true, std::memory_order_relaxed);
          mouse_events.Wheel(*event.point, event.delta);
        }
        break;
      case MouseEventKind::Cancel: mouse_events.Cancel(event.reason); break;
    }
  }
}
bool pvz::ReplayScreenReady(uint32_t vtable) { return replay_screen.load(std::memory_order_relaxed) == vtable; }
bool pvz::ReplayInputIdle() {
  std::lock_guard lock(pointer_mutex);
  return !mouse_events.Pending() && replay_input_idle.load(std::memory_order_relaxed);
}
bool pvz::MouseOwnsInput() { return MouseActive(); }
bool pvz::HostCursorVisible() {
  return !REXCVAR_GET(pvz_mouse_cursor) || (MouseActive() && !mouse_item_cursor.load() && !ServiceItemCursor());
}
void pvz::PublishFocusLost() {
  std::lock_guard lock(pointer_mutex);
  mouse_events.Cancel(MouseCancelReason::FocusLost);
}
void pvz::PublishControllerActivity() {
  if (pvz::LocalMouseAssigned()) return;
  if (!REXCVAR_GET(pvz_mouse_cursor) || !REXCVAR_GET(pvz_input_auto_switch)) return;
  std::lock_guard lock(pointer_mutex);
  // Cancel a mouse gesture once on the ownership transition. Repeating this
  // for every D-pad press also resets the framework's current gamepad focus.
  if (mouse_owner.exchange(false, std::memory_order_relaxed))
    mouse_events.Cancel(MouseCancelReason::Controller);
}
bool pvz::ReplaySceneReady(bool board) {
  return (board ? board_ready : menu_ready).load(std::memory_order_relaxed);
}
bool pvz::ReplayTitleReady() { return title_ready.load(std::memory_order_relaxed); }

REX_EXTERN(__imp__sub_8218B8A0);
REX_EXTERN(__imp__sub_8218A140);
REX_EXTERN(__imp__sub_824A6140);
REX_EXTERN(__imp__sub_824A6198);
REX_EXTERN(__imp__sub_82188310);
REX_EXTERN(__imp__sub_82146A60);
REX_EXTERN(__imp__sub_8216EF50);

REX_HOOK_RAW(sub_824AE518) {
  // Widget rects at 0,0 generate spurious native enters. The bridge emits
  // MouseEnter exactly once for the actual painted button in either mode.
  if (MouseActive() && IsGameButton(Read(base, ctx.r3.u32))) return;
  __imp__sub_824AE518(ctx, base);
}

REX_HOOK_RAW(sub_824A5F48) {
  const auto manager = ctx.r3.u32;
  // VS mode selection uses one shared focus. Keep a pad's button press until
  // its matching release; another player's mouse hover must not cancel it.
  if (pvz::LocalPlayersActive() && native_pad_button_press.manager == manager &&
      native_pad_button_press.widget == Read(base, manager + 0xA8) &&
      native_pad_button_press.widget && base[native_pad_button_press.widget + 0x74] &&
      ctx.r4.u32 != native_pad_button_press.widget) return;
  if (REXCVAR_GET(pvz_mouse_cursor) && PcMouseMode() && Read(base, manager) == 0x8206404C) {
    const auto requested = ctx.r4.u32;
    const auto current = Read(base, manager + 0xA8);
    if ((requested && IsGameButton(Read(base, requested))) ||
        (current && IsGameButton(Read(base, current)))) {
      std::optional<pvz::Point> snapshot;
      { std::lock_guard lock(pointer_mutex); snapshot = pointer; }
      ctx.r4.u32 = HoveredButton(manager, snapshot, base);
      const auto root = TopRootWidget(manager, base);
      if (root && Read(base, root) == 0x820B728C) {
        const auto cursor = MouseCursor(root, base);
        if ((selection.board == root && (selection.packet >= 0 || selection.shovel || selection.usable_seed)) ||
            (cursor && Read(base, cursor + 0x4C) != 0 &&
             (Read(base, Read(base, root + 0xF4) + 0x9CC) == 44 ||
              Read(base, Read(base, root + 0xF4) + 0x9CC) == 51))) ctx.r4.u32 = root;
      }
    }
  }
  __imp__sub_824A5F48(ctx, base);
}

REX_HOOK_RAW(sub_8216EF50) {
  // The console Update collects nearby coins automatically. In mouse
  // PC-style Adventure: supported coins (money, sun, seed reward) use clicks.
  // Other coin types, callers/modes/players
  // retain their original behavior, including scripted tutorial actions.
  const auto board = Read(base, ctx.r3.u32 + 0x20);
  const auto primary = board ? MouseControls(board, base) : 0;
  const auto type = Read(base, ctx.r3.u32 + 0x80);
  const auto app = board ? Read(base, board + 0xF4) : 0;
  const bool slot_seed = app && Read(base, app + 0x9CC) == 19 && type == 16;
  if (PcMouseMode() && slot_seed && !explicit_mouse_collect &&
      std::find(slot_auto_attempted.begin(), slot_auto_attempted.end(), ctx.r3.u32) != slot_auto_attempted.end()) return;
  if (REXCVAR_GET(pvz_mouse_cursor) && PcMouseMode() &&
      ((type == 16 && !explicit_mouse_collect) || ctx.lr == 0x8218C1C4 || ctx.lr == 0x821922F8) && ctx.r4.u32 == 0 &&
      !slot_seed && ((type >= 1 && type <= 7) || type == 16) && (PlantMouse(primary, base) ||
       (primary && Read(base, primary) == 0x820BA670 && int32_t(Read(base, primary + 0xA0)) == pvz::MouseUserIndex() && MouseActive()))) return;
  const auto coin = ctx.r3.u32;
  const bool trace_auto = REXCVAR_GET(pvz_input_trace) && REXCVAR_GET(pvz_mouse_cursor) &&
      !PcMouseMode() && (ctx.lr == 0x8218C1C4 || ctx.lr == 0x821922F8) && Read(base, coin + 0x84) != 7;
  __imp__sub_8216EF50(ctx, base);
  if (slot_seed && PcMouseMode() && PlantMouse(primary, base)) {
    if (MouseCursorType(board, base) == 2) {
      selection.packet = -1; selection.packets.clear(); selection.usable_seed = true;
      if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ Slot Machine auto picked dropped seed {}", Read(base, MouseCursor(board, base) + 0x44));
    }
  }
  if (trace_auto && Read(base, coin + 0x84) == 7)
    REXLOG_INFO("PvZ console auto collect coin type {}", type);
}
REX_EXTERN(__imp__sub_82133968);
REX_HOOK_RAW(sub_82133968) {
  __imp__sub_82133968(ctx, base);
  // Native coin slots are reused; a new award gets one new auto-pick attempt.
  std::erase(slot_auto_attempted, ctx.r3.u32);
}
REX_EXTERN(__imp__sub_8216B330);
REX_HOOK_RAW(sub_8216B330) {
  const auto coin = ctx.r3.u32, board = Read(base, coin + 0x20);
  const auto app = board ? Read(base, board + 0xF4) : 0;
  if (PcMouseMode() && app && Read(base, app + 0x9CC) == 19 && Read(base, app + 0x9D0) == 3 &&
      !base[board + 0x1D9] && Read(base, coin + 0x80) == 16 && !base[coin + 0x5C] && !base[coin + 0x74] &&
      PlantMouse(MouseControls(board, base), base) && MouseCursor(board, base) && MouseCursorType(board, base) == 0 &&
      std::find(slot_auto_attempted.begin(), slot_auto_attempted.end(), coin) == slot_auto_attempted.end()) {
    PPCContext work = ctx; work.r1.u32 -= 0x100; work.r3.u32 = coin; work.r4.u32 = 0;
    sub_8216EF50(work, base);
    if (MouseCursorType(board, base) == 2) slot_auto_attempted.push_back(coin);
  }
  __imp__sub_8216B330(ctx, base);
}

REX_HOOK_RAW(sub_821ABDE8) {
  // SeedPacket::MouseDown plays the lift sound when native controller logic
  // transfers the card to its cursor, which is the planting click in our
  // bridge. The bridge already played it when the mouse picked the card.
  const auto board = MouseActive() && ctx.lr == 0x82210034 ? Read(base, ctx.r3.u32 + 0x900) : 0;
  if (board && PlantMouse(MouseControls(board, base), base) &&
      ctx.r4.u32 == Read(base, 0x82D0A630)) {
    if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse deferred seed lift suppressed");
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_821ABDE8(ctx, base);
}

REX_HOOK_RAW(sub_824A85F0) {
  const auto dialog = ctx.r3.u32;
  if (!MouseActive() || !base[dialog + 0x180]) {
    __imp__sub_824A85F0(ctx, base);
    return;
  }
  const auto old_x = int32_t(Read(base, dialog + 0x40));
  const auto old_y = int32_t(Read(base, dialog + 0x44));
  const auto x = std::clamp(old_x + ctx.r4.s32 - int32_t(Read(base, dialog + 0x184)),
      -240, std::max(-240, 1040 - int(Read(base, dialog + 0x48))));
  const auto y = std::clamp(old_y + ctx.r5.s32 - int32_t(Read(base, dialog + 0x188)),
      -60, std::max(-60, 660 - int(Read(base, dialog + 0x4C))));
  Write(base, dialog + 0x184, uint32_t(old_x + ctx.r4.s32 - x));
  Write(base, dialog + 0x188, uint32_t(old_y + ctx.r5.s32 - y));
  PPCContext work = ctx;
  work.r1.u32 -= 0x100;
  work.r4.s64 = x; work.r5.s64 = y;
  rex::runtime::ResolveIndirectFunction(Read(base, Read(base, dialog) + 0x80))(work, base);
  if (REXCVAR_GET(pvz_input_trace) && (old_x != x || old_y != y))
    REXLOG_INFO("PvZ dialog drag {},{} -> {},{}", old_x, old_y, x, y);
}

REX_HOOK_RAW(sub_82146A60) {
  static bool traced = false;
  if (REXCVAR_GET(pvz_input_trace) && !traced &&
      int32_t(Read(base, ctx.r3.u32 + 0x56A0)) < 0) {
    const auto g = ctx.r4.u32;
    REXLOG_INFO("PvZ board draw translate {},{} scale {},{} origin {},{}",
        std::bit_cast<float>(Read(base, g + 8)), std::bit_cast<float>(Read(base, g + 12)),
        std::bit_cast<float>(Read(base, g + 16)), std::bit_cast<float>(Read(base, g + 20)),
        std::bit_cast<float>(Read(base, g + 24)), std::bit_cast<float>(Read(base, g + 28)));
    traced = true;
  }
  __imp__sub_82146A60(ctx, base);
}

REX_HOOK_RAW(sub_82188310) {
  if (pvz::DrawChooserTextButton(ctx, base)) return;
  if (pvz::DrawGardenNavigationButton(ctx, base)) return;
  static int traced = 0;
  static std::vector<uint32_t> traced_dialogs;
  const auto button = ctx.r3.u32;
  const auto graphics = ctx.r4.u32;
  const auto parent = Read(base, button + 0x24);
  if (REXCVAR_GET(pvz_input_trace) && parent && Read(base, parent) == 0x820C2784) {
    const auto id = Read(base, button + 0x6C);
    if ((id == 0 || id == 1 || id == 5) && !pvz::MouseOwnsInput()) {
      static std::array<int, 6> last_focus{-1, -1, -1, -1, -1, -1};
      const int flags = base[button + 0xA2] | (base[button + 0x75] << 1);
      if (last_focus[id] != flags) REXLOG_INFO("PvZ main button {} focus {} hover {} remembered {}", id,
          bool(base[button + 0xA2]), bool(base[button + 0x75]), Read(base, parent + 0x148));
      last_focus[id] = flags;
    }
    static uint32_t pressed_draw = 0;
    if (base[button + 0x74] && base[button + 0x75]) {
      if (pressed_draw != button) REXLOG_INFO("PvZ menu pressed draw button {}", Read(base, button + 0x6C));
      pressed_draw = button;
    } else if (pressed_draw == button) pressed_draw = 0;
  }
  if (REXCVAR_GET(pvz_input_trace) && IsGameButton(Read(base, button)) && parent &&
      Read(base, parent) != 0x820C2784 && traced_dialogs.size() < 64 &&
      std::find(traced_dialogs.begin(), traced_dialogs.end(), parent) == traced_dialogs.end()) {
    REXLOG_INFO("PvZ dialog button id {} parent {},{} draw {},{} size {},{} base scale {},{} graphics {},{} scale {},{} origin {},{}",
        Read(base, button + 0x6C), int32_t(Read(base, parent + 0x40)), int32_t(Read(base, parent + 0x44)),
        int32_t(Read(base, button + 0x190)), int32_t(Read(base, button + 0x194)),
        Read(base, button + 0x1A4), Read(base, button + 0x1A8),
        std::bit_cast<float>(Read(base, button + 0x1C4)), std::bit_cast<float>(Read(base, button + 0x1C8)),
        std::bit_cast<float>(Read(base, graphics + 8)), std::bit_cast<float>(Read(base, graphics + 12)),
        std::bit_cast<float>(Read(base, graphics + 16)), std::bit_cast<float>(Read(base, graphics + 20)),
        std::bit_cast<float>(Read(base, graphics + 24)), std::bit_cast<float>(Read(base, graphics + 28)));
    traced_dialogs.push_back(parent);
  }
  if (REXCVAR_GET(pvz_input_trace) && traced < 20 && Read(base, button) == 0x820BA0AC) {
    REXLOG_INFO("PvZ button draw id {} graphics {},{} scale {},{} origin {},{}",
        Read(base, button + 0x6C), std::bit_cast<float>(Read(base, graphics + 8)),
        std::bit_cast<float>(Read(base, graphics + 12)), std::bit_cast<float>(Read(base, graphics + 16)),
        std::bit_cast<float>(Read(base, graphics + 20)), std::bit_cast<float>(Read(base, graphics + 24)),
        std::bit_cast<float>(Read(base, graphics + 28)));
    ++traced;
  }
  __imp__sub_82188310(ctx, base);
}

REX_HOOK_RAW(sub_824A6140) {
  const auto manager = ctx.r3.u32;
  pvz::UpdateControllerAssets(ctx, base);
  DispatchWidgets(ctx, base);
  __imp__sub_824A6140(ctx, base);
  std::optional<pvz::Point> snapshot;
  { std::lock_guard lock(pointer_mutex); snapshot = pointer; }
  SyncMouseHover(ctx, manager, snapshot, base);
}
REX_HOOK_RAW(sub_824A6198) {
  const auto manager = ctx.r3.u32;
  pvz::UpdateControllerAssets(ctx, base);
  DispatchWidgets(ctx, base);
  __imp__sub_824A6198(ctx, base);
  std::optional<pvz::Point> snapshot;
  { std::lock_guard lock(pointer_mutex); snapshot = pointer; }
  SyncMouseHover(ctx, manager, snapshot, base);
}

// Only the two axis reads inside UpdateHeavyWeapon see mouse aim. Physical
// controller input and every other game mode continue through their native getters.
REX_HOOK_RAW(sub_82580C58) {
  if (heavy_weapon_aim && ctx.lr == 0x82165D3C) ctx.f1.f64 = heavy_weapon_aim->x;
  else __imp__sub_82580C58(ctx, base);
}

REX_HOOK_RAW(sub_8216DC40) {
  const auto coin = ctx.r3.u32, board = Read(base, coin + 0x20);
  const auto type = Read(base, coin + 0x80);
  const auto state = Read(base, coin + 0x84);
  const bool automatic = ctx.lr == 0x8216B554 || ctx.lr == 0x8216B490 || ctx.lr == 0x8216B3B0 || ctx.lr == 0x82158488;
  // State 7 is the homing pickup already requested by Coin::Collect on
  // a click. Update calls this function again when the coin reaches the
  // cursor; that completion must run to collect/credit it. Only reject
  // unsolicited pickup of a coin still falling or resting on the lawn.
  const bool slot_seed = type == 16 && board && Read(base, Read(base, board + 0xF4) + 0x9CC) == 19;
  if (PcMouseMode() && automatic && slot_seed && state != 7 && !base[coin + 0x74] &&
      std::find(slot_auto_attempted.begin(), slot_auto_attempted.end(), coin) != slot_auto_attempted.end()) return;
  if (PcMouseMode() && automatic && !slot_seed && state != 7 && !base[coin + 0x74] &&
      ((type >= 1 && type <= 7) || type == 16) && board && PlantMouse(MouseControls(board, base), base)) return;
  if (PcMouseMode() && type == 16 && REXCVAR_GET(pvz_input_trace))
    REXLOG_INFO("PvZ dropped packet pickup caller {:08X} picked {} seed {}", ctx.lr, base[coin + 0x74], Read(base, coin + 0x98));
  const bool completing = REXCVAR_GET(pvz_input_trace) && PcMouseMode() && automatic &&
      state == 7 && !base[coin + 0x74] && board;
  const auto sun = completing ? Read(base, board + 0x55F4) : 0;
  __imp__sub_8216DC40(ctx, base);
  if (completing) REXLOG_INFO("PvZ clicked coin pickup completed type {} state {} picked {} sun {} -> {}",
      type, state, bool(base[coin + 0x74]), sun, Read(base, board + 0x55F4));
}
REX_EXTERN(__imp__sub_8216F3D0);
REX_HOOK_RAW(sub_8216F3D0) {
  const auto coin = ctx.r3.u32, board = Read(base, coin + 0x20);
  const auto type = Read(base, coin + 0x80);
  if (!PcMouseMode() || !((type >= 1 && type <= 7) || type == 16) || !board ||
      !(PlantMouse(MouseControls(board, base), base) || pvz::ServicePcGarden())) {
    __imp__sub_8216F3D0(ctx, base); return;
  }
  // Console pickup extends coins by 100 pixels and selects packets by lawn
  // cell, stealing clicks from nearby plants. Use PC image bounds, with a
  // small margin for sun and the original PC hammer collection allowance.
  const pvz::Point point{float(ctx.r4.s32), float(ctx.r5.s32)};
  const auto mode = Read(base, Read(base, board + 0xF4) + 0x9CC);
  const int margin = type == 4 || mode == 31 ? 15 : 0;
  const int extra_height = mode == 31 ? 30 : 0;
  const auto cursor = MouseCursor(board, base);
  const bool hand_free = type != 16 || mode == 31 ||
      (cursor && (Read(base, cursor + 0x4C) == 0 || Read(base, cursor + 0x4C) == 7) && selection.packet < 0 && !selection.shovel);
  const bool hit = !base[coin + 0x5C] && !base[coin + 0x74] && Read(base, coin + 0x84) != 7 &&
      hand_free && pvz::InRect(point, std::bit_cast<float>(Read(base, coin + 0x40)) - margin,
          std::bit_cast<float>(Read(base, coin + 0x44)) - margin,
          Read(base, coin + 0x2C) + 2 * margin, Read(base, coin + 0x30) + 2 * margin + extra_height);
  Write(base, ctx.r6.u32, hit ? coin : 0);
  Write(base, ctx.r6.u32 + 4, hit ? 3 : 0);
  ctx.r3.u64 = hit;
  if (REXCVAR_GET(pvz_input_trace) && pvz::ServicePcGarden()) {
    static std::array<uint32_t, 3> previous{};
    const std::array state{uint32_t(ctx.r4.s32), uint32_t(ctx.r5.s32), Read(base, coin + 0x84)};
    if (state != previous) REXLOG_INFO("PvZ garden coin hit {},{} rect {},{},{},{} state {} hit {}",
        point.x, point.y, std::bit_cast<float>(Read(base, coin + 0x40)), std::bit_cast<float>(Read(base, coin + 0x44)),
        Read(base, coin + 0x2C), Read(base, coin + 0x30), state[2], hit);
    previous = state;
  }
}
REX_HOOK_RAW(sub_821516D8) {
  const auto challenge = ctx.r3.u32;
  const auto x = ctx.r4.s32, y = ctx.r5.s32;
  const auto from_x = Read(base, challenge + 0x2C), from_y = Read(base, challenge + 0x30);
  __imp__sub_821516D8(ctx, base);
  if (PcMouseMode() && REXCVAR_GET(pvz_input_trace) && !base[challenge + 0x28] &&
      (std::abs(int32_t(from_x) - x) >= 10 || std::abs(int32_t(from_y) - y) >= 10))
    REXLOG_INFO("PvZ Beghouled swap {},{} -> {},{} state {} capture {}", from_x, from_y, x, y, Read(base, challenge + 0x74), base[challenge + 0x28]);
}
REX_EXTERN(__imp__sub_82154930);
REX_HOOK_RAW(sub_82154930) {
  const auto challenge = ctx.r3.u32, board = Read(base, challenge + 0x20);
  const auto controls = MouseControls(board, base);
  // This update computes the Twist 2x2 target before Controls::Update, so
  // it needs the current pointer too, instead of last frame's pad snap.
  if (PcMouseMode() && PlantMouse(controls, base)) ApplyPointer(ctx, controls, base);
  __imp__sub_82154930(ctx, base);
  if (REXCVAR_GET(pvz_input_trace)) {
    static std::tuple<uint32_t, uint32_t, uint32_t> previous{};
    const auto current = std::tuple(challenge, Read(base, challenge + 0x74), Read(base, challenge + 0x84));
    if (current != previous) {
      REXLOG_INFO("PvZ puzzle mode {} state {} matches {}", Read(base, Read(base, board + 0xF4) + 0x9CC), Read(base, challenge + 0x74), Read(base, challenge + 0x84));
      previous = current;
    }
  }
}
bool MouseTwistOverlay(uint32_t challenge, uint8_t* base) {
  if (!MouseActive() || !challenge) return false;
  const auto board = Read(base, challenge + 0x20);
  return board && PlantMouse(MouseControls(board, base), base) &&
      Read(base, Read(base, board + 0xF4) + 0x9CC) == 25;
}
REX_EXTERN(__imp__sub_8227C190);
REX_HOOK_RAW(sub_8227C190) {
  // DrawBeghouled derives the console ring from a single focused cell.
  // Its mouse target is instead the same 2x2 block used by TwistMouseDown.
  if (ctx.lr == 0x82158BD4 && MouseTwistOverlay(ctx.r24.u32, base)) {
    const auto challenge = ctx.r24.u32;
    const int col = int32_t(Read(base, challenge + 0xCC)), row = int32_t(Read(base, challenge + 0xD0));
    if (col >= 0 && col <= 6 && row >= 0 && row <= 3) {
      PPCContext work = ctx; work.r1.u32 -= 0x100;
      work.r3.u32 = Read(base, challenge + 0x20); work.r4.s64 = col; work.r5.s64 = row;
      sub_82149848(work, base);
      // TodBltMatrix consumes framebuffer coordinates, bypassing the
      // Graphics translation used by ordinary plant/image draws.
      const auto graphics = ctx.r26.u32;
      ctx.f1.f64 = std::bit_cast<float>(Read(base, graphics + 8)) + 120 + col * 80;
      ctx.f2.f64 = std::bit_cast<float>(Read(base, graphics + 12)) + work.r3.s32 + 100;
    }
  }
  __imp__sub_8227C190(ctx, base);
}
REX_EXTERN(__imp__sub_8227C240);
REX_HOOK_RAW(sub_8227C240) {
  extern bool DrawAquariumPacketCell(PPCContext&, uint8_t*);
  if (DrawAquariumPacketCell(ctx, base)) return;
  if (ctx.lr == 0x82158C0C && MouseTwistOverlay(ctx.r24.u32, base)) {
    std::optional<pvz::Point> position;
    { std::lock_guard lock(pointer_mutex); position = pointer; }
    if (!position || !pvz::MapBoardPointer(*position) ||
        int32_t(Read(base, ctx.r24.u32 + 0xCC)) < 0 || int32_t(Read(base, ctx.r24.u32 + 0xD0)) < 0) return;
  }
  __imp__sub_8227C240(ctx, base);
}
REX_HOOK_RAW(sub_82580C80) {
  if (heavy_weapon_aim && ctx.lr == 0x82165D24) ctx.f1.f64 = heavy_weapon_aim->y;
  else __imp__sub_82580C80(ctx, base);
}
REX_HOOK_RAW(sub_821667C0) {
  if (heavy_weapon_aim && REXCVAR_GET(pvz_input_trace))
    REXLOG_INFO("PvZ Heavy Weapon fire direction {},{}", ctx.f1.f64, ctx.f2.f64);
  __imp__sub_821667C0(ctx, base);
}
REX_HOOK_RAW(sub_82165B58) {
  const auto challenge = ctx.r3.u32;
  const auto app = Read(base, challenge + 0x1C), board = Read(base, challenge + 0x20);
  const auto controls = board ? MouseControls(board, base) : 0;
  const bool mouse = MouseActive() && app && Read(base, app + 0x9CC) == 20 &&
      Read(base, app + 0x9D0) == 3 && board && Read(base, board) == 0x820B728C &&
      !base[board + 0x1D9] && controls && Read(base, controls) == 0x820BA5E8 &&
      Read(base, controls + 0x9C) == 0 && int32_t(Read(base, controls + 0xA0)) == pvz::MouseUserIndex();
  if (!mouse) {
    __imp__sub_82165B58(ctx, base);
    static int native_traced = 0;
    if (REXCVAR_GET(pvz_input_trace) && native_traced++ % 30 == 0)
      REXLOG_INFO("PvZ Heavy Weapon native carriage {} y {} angle {}",
          std::bit_cast<float>(Read(base, challenge + 0x118)),
          std::bit_cast<float>(Read(base, challenge + 0x11C)),
          std::bit_cast<float>(Read(base, challenge + 0x120)));
    return;
  }
  std::optional<pvz::Point> position;
  { std::lock_guard lock(pointer_mutex); position = pointer; }
  position = position ? pvz::MapBoardPointer(*position) : std::nullopt;
  const float x = std::bit_cast<float>(Read(base, challenge + 0x118));
  const float y = std::bit_cast<float>(Read(base, challenge + 0x11C));
  const auto mapped = position ? pvz::MapHeavyWeaponPointer(*position, x, y) :
      pvz::HeavyWeaponPointer{x, {0, 0}};
  const auto bank = Read(base, board + 0x1A4);
  const bool in_bank = position && bank && pvz::InRect(*position, int32_t(Read(base, bank + 0x24)),
      int32_t(Read(base, bank + 0x28)), Read(base, bank + 0x2C), Read(base, bank + 0x30));
  const bool firing = position && heavy_weapon_mouse.board == board && heavy_weapon_mouse.firing && !in_bank;
  // A neutral right stick gates every native weapon, including the auxiliary
  // missiles. Preserve aim visually without repeatedly resetting the cooldown.
  if (position && !firing)
    Write(base, challenge + 0x120, std::bit_cast<uint32_t>(-std::atan2(mapped.aim.x, mapped.aim.y)));
  const auto old_vx = Read(base, controls + 0x7C), old_vy = Read(base, controls + 0x80);
  // Native .016 is an input multiplier; provide the displacement from the
  // old carriage origin to the absolute mouse target through that input.
  // Let the native update move the carriage, animate the wheels, rotate the
  // turret, fire on its existing cooldown, and process power-ups/collisions.
  Write(base, controls + 0x7C, std::bit_cast<uint32_t>((mapped.carriage_x - x) / .016f));
  Write(base, controls + 0x80, 0);
  const auto previous = heavy_weapon_aim;
  heavy_weapon_aim = firing ? mapped.aim : pvz::Point{0, 0};
  __imp__sub_82165B58(ctx, base);
  heavy_weapon_aim = previous;
  Write(base, controls + 0x7C, old_vx);
  Write(base, controls + 0x80, old_vy);
  static int traced = 0;
  if (REXCVAR_GET(pvz_input_trace) && traced++ % 30 == 0)
    REXLOG_INFO("PvZ Heavy Weapon mouse cursor {},{} carriage {} -> {} y {} aim {},{} angle {}",
        position ? position->x : -999, position ? position->y : -999, x,
        std::bit_cast<float>(Read(base, challenge + 0x118)), y, mapped.aim.x, mapped.aim.y,
        std::bit_cast<float>(Read(base, challenge + 0x120)));
}

// UI thread only publishes a snapshot. Guest memory is touched on the guest
// thread, before hit testing / button actions and again before cursor drawing.
REX_HOOK_RAW(sub_8218B8A0) {
  const uint32_t controls = ctx.r3.u32;
  if (REXCVAR_GET(pvz_input_trace) && pvz::LocalPlayersActive() && Read(base, controls) == 0x820BA5E8) {
    const auto board = Read(base, controls + 0x44), app = Read(base, controls + 0x20);
    static std::array<uint32_t, 6> previous{};
    const auto c0 = Read(base, board + 0x1B0), c1 = Read(base, board + 0x1B4);
    const std::array state{board, Read(base, app + 0x9D0), Read(base, c0 + 0xA0), Read(base, c1 + 0xA0),
        Read(base, board + 0x55F4), Read(base, board + 0x55F8)};
    if (state != previous) {
      REXLOG_INFO("PvZ local board mode {} scene {} users {},{} resources {},{}", Read(base, app + 0x9CC),
          state[1], int32_t(state[2]), int32_t(state[3]), state[4], state[5]);
      previous = state;
    }
  }
  if (selection.controls == controls && int32_t(Read(base, controls + 0xA0)) != pvz::MouseUserIndex()) selection = {};
  if (REXCVAR_GET(pvz_mouse_cursor) && Read(base, controls) == 0x820BA5E8 &&
      int32_t(Read(base, controls + 0xA0)) == pvz::MouseUserIndex()) {
    const auto board = Read(base, controls + 0x44);
    const auto app = Read(base, controls + 0x20);
    board_ready.store(board && app && int32_t(Read(base, board + 0x56A0)) < 0 &&
        Read(base, app + 0x9D0) == 3 && !base[board + 0x1D9], std::memory_order_relaxed);
  }
  static int traced_controls = 0;
  if (REXCVAR_GET(pvz_input_trace) && Read(base, controls) == 0x820BA5E8 &&
      int32_t(Read(base, controls + 0xA0)) == pvz::MouseUserIndex() && traced_controls++ % 100 == 0) {
    const auto board = Read(base, controls + 0x44);
    const auto bank = Read(base, board + 0x1A4);
    static bool layout_logged = false;
    if (bank && !layout_logged) {
      REXLOG_INFO("PvZ Board rect {},{},{},{} bank rect {},{},{},{} packet rect {},{},{},{} offset {}",
          int32_t(Read(base, board + 0x40)), int32_t(Read(base, board + 0x44)), Read(base, board + 0x48), Read(base, board + 0x4C),
          int32_t(Read(base, bank + 0x24)), int32_t(Read(base, bank + 0x28)), Read(base, bank + 0x2C), Read(base, bank + 0x30),
          int32_t(Read(base, bank + 0x74)), int32_t(Read(base, bank + 0x78)), Read(base, bank + 0x7C), Read(base, bank + 0x80),
          int32_t(Read(base, bank + 0x9C)));
      layout_logged = true;
    }
    REXLOG_INFO("PvZ controls {:08X} mode {} playerindex {} packet {} seedbank {:08X} rect {},{},{},{} count {} board state {} sun {} cursor {},{}",
        controls, Read(base, controls + 0x68), Read(base, controls + 0x9C), Read(base, controls + 0xBC),
        bank, bank ? int32_t(Read(base, bank + 0x24)) : 0, bank ? int32_t(Read(base, bank + 0x28)) : 0,
        bank ? Read(base, bank + 0x2C) : 0, bank ? Read(base, bank + 0x30) : 0, bank ? Read(base, bank + 0x40) : 0,
        int32_t(Read(base, board + 0x56A0)), Read(base, board + 0x55F4),
        std::bit_cast<float>(Read(base, controls + 0x74)), std::bit_cast<float>(Read(base, controls + 0x78)));
    REXLOG_INFO("PvZ mouse state adventure {} mode {} alternate {} tutorial {} Dave index {} Dave state {}",
        PlantMouse(controls, base), Read(base, Read(base, controls + 0x20) + 0x9CC), base[controls + 0xC4],
        Read(base, board + 0x5620), Read(base, Read(base, controls + 0x20) + 0xA24), Read(base, Read(base, controls + 0x20) + 0xA18));
  }
  ApplyPointer(ctx, controls, base);
  __imp__sub_8218B8A0(ctx, base);
  if (PcMouseMode()) ApplyPointer(ctx, controls, base);
  HideUnselectedCursor(controls, base);
  if (MouseActive() && PlantMouse(controls, base)) {
    const auto game_mode = Read(base, Read(base, controls + 0x20) + 0x9CC);
    puzzle_highlights.clear();
    if (game_mode == 21 || game_mode == 25) {
      static uint32_t traced_puzzle = 0;
      if (REXCVAR_GET(pvz_input_trace) && traced_puzzle != selection.board && !base[selection.board + 0x1D9]) {
        traced_puzzle = selection.board;
        const auto state = ctx.r1.u32 - 0x400, challenge = Read(base, selection.board + 0x1D4);
        PPCContext probe = ctx; probe.r1.u32 -= 0x800; probe.r3.u32 = challenge; probe.r4.u32 = state;
        sub_8214F5E8(probe, base);
        int found = 0;
        for (int col = 0; col < 8 && found < 4; ++col) for (int row = 0; row < 5 && found < 4; ++row) {
          if (game_mode == 25) {
            if (col > 6 || row > 3) continue;
            probe = ctx; probe.r1.u32 -= 0x800; probe.r3.s64 = col; probe.r4.s64 = row; probe.r5.u32 = state;
            sub_82150EB8(probe, base);
            if (probe.r3.u32) { REXLOG_INFO("PvZ replay valid Twist block {},{} pointer {},{}", col, row, 225 + col * 50, 200 + row * 100 / 1.2f); ++found; }
          } else for (const auto delta : {std::pair(1, 0), std::pair(0, 1)}) {
            if (col + delta.first >= 8 || row + delta.second >= 5) continue;
            probe = ctx; probe.r1.u32 -= 0x800; probe.r3.u32 = challenge; probe.r4.s64 = col; probe.r5.s64 = row;
            probe.r6.s64 = col + delta.first; probe.r7.s64 = row + delta.second; probe.r8.u32 = state;
            sub_82151600(probe, base);
            if (probe.r3.u32) { REXLOG_INFO("PvZ replay valid Beghouled pair {},{} -> {},{}", col, row, col + delta.first, row + delta.second); ++found; }
          }
        }
      }
      std::optional<pvz::Point> position;
      { std::lock_guard lock(pointer_mutex); position = pointer; }
      const auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt;
      auto highlight = [&](pvz::Point p) {
        PPCContext work = ctx; work.r1.u32 -= 0x180;
        work.r3.u32 = selection.board; work.r4.s64 = int(p.x); work.r5.s64 = int(p.y);
        sub_82149538(work, base); const int col = work.r3.s32;
        work = ctx; work.r1.u32 -= 0x180; work.r3.u32 = selection.board; work.r4.s64 = int(p.x); work.r5.s64 = int(p.y);
        sub_821495B0(work, base); const int row = work.r3.s32;
        if (col < 0 || col >= 9 || row < 0 || row >= 6 || p.y < 90) return;
        const auto out = ctx.r1.u32 - 0x80;
        for (int i = 0; i < 4; ++i) Write(base, out + i * 4, 0);
        work = ctx; work.r1.u32 -= 0x180; work.r3.u32 = selection.board; work.r4.s64 = col; work.r5.s64 = row; work.r6.u32 = out;
        sub_82134340(work, base);
        for (int i = 0; i < 4; ++i) if (auto plant = Read(base, out + i * 4)) puzzle_highlights.push_back(plant);
      };
      if (local) highlight(*local);
      if (local && game_mode == 25) {
        puzzle_highlights.clear();
        PPCContext work = ctx; work.r1.u32 -= 0x180;
        work.r3.u32 = Read(base, selection.board + 0x1D4);
        work.r4.s64 = int(local->x); work.r5.s64 = int(local->y);
        work.r6.u32 = ctx.r1.u32 - 0x88; work.r7.u32 = ctx.r1.u32 - 0x84;
        sub_82150F60(work, base);
        if (work.r3.u32) {
          const int col = int32_t(Read(base, ctx.r1.u32 - 0x88)), row = int32_t(Read(base, ctx.r1.u32 - 0x84));
          const auto challenge = Read(base, selection.board + 0x1D4);
          if (Read(base, challenge + 0x74) == 0) {
            Write(base, challenge + 0xCC, col); Write(base, challenge + 0xD0, row);
          }
          for (int dx = 0; dx < 2; ++dx) for (int dy = 0; dy < 2; ++dy) {
            const auto out = ctx.r1.u32 - 0x80;
            for (int i = 0; i < 4; ++i) Write(base, out + i * 4, 0);
            work = ctx; work.r1.u32 -= 0x180; work.r3.u32 = selection.board;
            work.r4.s64 = col + dx; work.r5.s64 = row + dy; work.r6.u32 = out;
            sub_82134340(work, base);
            for (int i = 0; i < 4; ++i) if (auto plant = Read(base, out + i * 4)) puzzle_highlights.push_back(plant);
          }
        }
      }
      if (game_mode == 21 && beghouled_mouse.board == selection.board && beghouled_mouse.selected) highlight(*beghouled_mouse.selected);
    }
  }
  if (int32_t(Read(base, controls + 0xA0)) == pvz::MouseUserIndex()) {
    const auto board = Read(base, controls + 0x44), app = Read(base, controls + 0x20);
    bool inside = false;
    { std::lock_guard lock(pointer_mutex); inside = pointer && pvz::MapUiPointer(*pointer).has_value(); }
    const int tutorial = board ? int32_t(Read(base, board + 0x5620)) : -1;
    const bool holding = inside && PcMouseMode() && selection.controls == controls &&
        (selection.shovel || selection.usable_seed || selection.packet >= 0) && board && app &&
        (Read(base, app + 0x9D0) == 3 || (tutorial >= 15 && tutorial <= 17)) && !base[board + 0x1D9];
    if (mouse_item_cursor.exchange(holding) != holding) pvz::RefreshHostSettings();
  }
  if (selection.controls == controls) selection.shovel_plant = 0;
  if (PcMouseMode() && PlantMouse(controls, base) && selection.shovel) {
    const auto board = selection.board;
    PPCContext work = ctx;
    auto call = [&](PPCFunc* fn, uint32_t self, uint32_t a, uint32_t b, uint32_t c = 0) {
      work = ctx; work.r1.u32 -= 0x100;
      work.r3.u32 = self; work.r4.u32 = a; work.r5.u32 = b; work.r6.u32 = c;
      fn(work, base); return work.r3.u32;
    };
    const int x = int(std::bit_cast<float>(Read(base, controls + 0x74)));
    const int y = int(std::bit_cast<float>(Read(base, controls + 0x78)));
    const int col = int(call(sub_82149538, board, x, y));
    const int row = int(call(sub_82149728, board, x, y));
    if (col >= 0 && col < 9 && row >= 0 && row < 6) {
      const auto result = ctx.r1.u32 - 0x80;
      for (int i = 0; i < 4; ++i) Write(base, result + i * 4, 0);
      call(sub_82134340, board, col, row, result);
      // Match MouseDownWithTool's native digging order, including overlays.
      for (const int i : {2, 3, 1, 0})
        if (auto plant = Read(base, result + i * 4)) { selection.shovel_plant = plant; break; }
    }
  }
}

REX_EXTERN(__imp__sub_821D22C0);
REX_HOOK_RAW(sub_821D22C0) {
  const auto plant = ctx.r3.u32;
  const auto old = base[plant + 0x161]; // Verified native additive-highlight flag.
  if (PcMouseMode() && selection.shovel && Read(base, plant + 0x20) == selection.board)
    base[plant + 0x161] = plant == selection.shovel_plant;
  if (PcMouseMode() && Read(base, plant + 0x20) == selection.board && selection.board) {
    const auto mode = Read(base, Read(base, selection.board + 0xF4) + 0x9CC);
    if (mode == 21 || mode == 25) base[plant + 0x161] =
        std::find(puzzle_highlights.begin(), puzzle_highlights.end(), plant) != puzzle_highlights.end();
  }
  __imp__sub_821D22C0(ctx, base);
  base[plant + 0x161] = old;
}

REX_EXTERN(__imp__sub_8218D0F8);
REX_EXTERN(sub_8218D0F8);
REX_HOOK_RAW(sub_8218D0F8) {
  const auto controls = ctx.r3.u32;
  if (!drawing_late_cursor && late_cursor.board && PcMouseMode() && PlantMouse(controls, base) &&
      late_cursor.board == Read(base, controls + 0x44)) {
    late_cursor.controls = controls;
    for (size_t i = 0; i < late_cursor.graphics.size(); ++i)
      late_cursor.graphics[i] = Read(base, ctx.r4.u32 + 8 + uint32_t(i * 4));
    return;
  }
  if (REXCVAR_GET(pvz_input_trace)) {
    static int drawn = 0;
    if (int32_t(Read(base, controls + 0xA0)) == pvz::MouseUserIndex() && drawn++ < 3)
      REXLOG_INFO("PvZ PC cursor draw hook pc {} adventure {} board {:08X}", PcMouseMode(), PlantMouse(controls, base), Read(base, controls + 0x44));
  }
  if (!PcMouseMode() || !PlantMouse(controls, base)) {
    const auto saved = console_draw_offset;
    if (MouseActive() && !PcMouseMode() && PlantMouse(controls, base) && Read(base, controls + 0x68) != 6) {
      std::optional<pvz::Point> position;
      { std::lock_guard lock(pointer_mutex); position = pointer; }
      if (auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt)
        console_draw_offset = pvz::Point{local->x - std::bit_cast<float>(Read(base, controls + 0x74)),
            local->y - std::bit_cast<float>(Read(base, controls + 0x78))};
    }
    __imp__sub_8218D0F8(ctx, base);
    console_draw_offset = saved; return;
  }
  // The console renderer combines a grid reticle, drop shadow and seed preview.
  // The preserved PC CursorObject renderer draws just the item in the hand.
  const auto graphics = ctx.r4.u32;
  const auto cursor = MouseCursor(selection.board, base);
  if (!cursor || (!selection.shovel && !selection.usable_seed && selection.packet < 0 && Read(base, cursor + 0x4C) != 7)) return;
  const auto app = Read(base, selection.board + 0xF4);
  const int tutorial = int32_t(Read(base, selection.board + 0x5620));
  if (base[selection.board + 0x1D9] || (Read(base, app + 0x9D0) != 3 &&
      (tutorial < 15 || tutorial > 17))) return;
  std::optional<pvz::Point> position;
  { std::lock_guard lock(pointer_mutex); position = pointer; }
  const auto item_position = position ? pvz::MapUiPointer(*position) : std::nullopt;
  if (!item_position) return;
  if (base[controls + 0xC4]) {
    // Zombie cards have their own native cached preview. CursorObject's
    // plant renderer is unsuitable for zombie seed IDs (61..85).
    if (selection.packet >= 0 && Read(base, controls + 0xF0)) {
      const auto gx = Read(base, graphics + 8), gy = Read(base, graphics + 12);
      const auto field = pvz::MapBoardPointer(*position);
      if (field && OverPlantingField(selection.bank, *field, base)) {
        PPCContext ghost = ctx; ghost.r1.u32 -= 0x100;
        ghost.r3.u32 = selection.board; ghost.r4.s64 = int(field->x); ghost.r5.s64 = int(field->y);
        sub_82149538(ghost, base); const int col = ghost.r3.s32;
        ghost = ctx; ghost.r1.u32 -= 0x100; ghost.r3.u32 = selection.board;
        ghost.r4.s64 = int(field->x); ghost.r5.s64 = int(field->y);
        sub_82149728(ghost, base); const int row = ghost.r3.s32;
        if (col >= 0 && col < 9 && row >= 0 && row < 6) {
          ghost = ctx; ghost.r1.u32 -= 0x100; ghost.r3.u32 = selection.board;
          ghost.r4.s64 = col; ghost.r5.s64 = row; sub_82149848(ghost, base);
          Write(base, graphics + 8, std::bit_cast<uint32_t>(std::bit_cast<float>(gx) + 40.f + col * 80.f));
          Write(base, graphics + 12, std::bit_cast<uint32_t>(std::bit_cast<float>(gy) + float(ghost.r3.s32)));
          ghost = ctx; ghost.r1.u32 -= 0x100; sub_8218F9A8(ghost, base);
          Write(base, graphics + 8, gx); Write(base, graphics + 12, gy);
        }
      }
      Write(base, graphics + 8, std::bit_cast<uint32_t>(std::bit_cast<float>(gx) + item_position->x));
      Write(base, graphics + 12, std::bit_cast<uint32_t>(std::bit_cast<float>(gy) + item_position->y));
      PPCContext work = ctx; work.r1.u32 -= 0x100;
      sub_8218F9A8(work, base);
      Write(base, graphics + 8, gx); Write(base, graphics + 12, gy);
    }
    return;
  }
  // Rendering needs a seed cursor even when the console update leaves its
  // preview empty. Restore these fields immediately; planting owns its state.
  const std::array<uint32_t, 4> cursor_saved{Read(base, cursor + 0x40), Read(base, cursor + 0x44),
      Read(base, cursor + 0x48), Read(base, cursor + 0x4C)};
  const auto preview_saved = Read(base, controls + 0xC0);
  const auto preview_valid = base[controls + 0xC5];
  if (selection.packet >= 0) {
    const auto packet = selection.bank + 0x50 + uint32_t(selection.packet) * 0x7C;
    const auto seed = Read(base, packet + 0x50), imitater = Read(base, packet + 0x54);
    Write(base, cursor + 0x40, Read(base, packet + 0x48));
    Write(base, cursor + 0x44, seed); Write(base, cursor + 0x48, imitater); Write(base, cursor + 0x4C, 1);
    Write(base, controls + 0xC0, seed == 48 ? imitater : seed);
    base[controls + 0xC5] = 0; // Draw directly; the console preview image may not exist.
  }
  const auto gx = Read(base, graphics + 8), gy = Read(base, graphics + 12);
  const auto field_position = pvz::MapBoardPointer(*position);
  if ((selection.packet >= 0 || selection.usable_seed) && field_position &&
      OverPlantingField(selection.bank, *field_position, base)) {
    PPCContext preview = ctx; preview.r1.u32 -= 0x100;
    preview.r3.u32 = selection.board; preview.r4.s64 = int(item_position->x); preview.r5.s64 = int(item_position->y);
    sub_82149538(preview, base); const int col = preview.r3.s32;
    preview.r3.u32 = selection.board; preview.r4.s64 = int(item_position->x); preview.r5.s64 = int(item_position->y);
    sub_82149728(preview, base); const int row = preview.r3.s32;
    const bool cell_valid = col >= 0 && col < 9 && row >= 0 && row < 6;
    preview.r3.u32 = 1;
    if (cell_valid) {
      preview.r3.u32 = selection.board; preview.r4.s64 = col; preview.r5.s64 = row;
      preview.r6.u32 = Read(base, controls + 0xC0); sub_82135C48(preview, base);
    }
    if (cell_valid && preview.r3.u32 == 0) {
      if (REXCVAR_GET(pvz_input_trace)) {
        static int last_col = -1, last_row = -1;
        if (col != last_col || row != last_row) {
          REXLOG_INFO("PvZ mouse planting preview cell {},{} seed {}", col, row, Read(base, controls + 0xC0));
          last_col = col; last_row = row;
        }
      }
      preview.r3.u32 = selection.board; preview.r4.s64 = col; preview.r5.s64 = row;
      sub_82149848(preview, base);
      Write(base, graphics + 8, std::bit_cast<uint32_t>(std::bit_cast<float>(gx) + float(col * 80 + 40)));
      Write(base, graphics + 12, std::bit_cast<uint32_t>(std::bit_cast<float>(gy) + float(preview.r3.s32)));
      preview = ctx; preview.r1.u32 -= 0x100;
      sub_8218F9A8(preview, base);
      Write(base, graphics + 8, gx); Write(base, graphics + 12, gy);
    }
  }
  Write(base, graphics + 8, std::bit_cast<uint32_t>(std::bit_cast<float>(gx) +
      item_position->x - 25));
  Write(base, graphics + 12, std::bit_cast<uint32_t>(std::bit_cast<float>(gy) +
      item_position->y - 35));
  PPCContext work = ctx; work.r1.u32 -= 0x100; work.r3.u32 = cursor;
  sub_821774C0(work, base);
  Write(base, graphics + 8, gx); Write(base, graphics + 12, gy);
  for (unsigned i = 0; i < cursor_saved.size(); ++i) Write(base, cursor + 0x40 + i * 4, cursor_saved[i]);
  Write(base, controls + 0xC0, preview_saved); base[controls + 0xC5] = preview_valid;
}

REX_EXTERN(__imp__sub_8212A658);
REX_HOOK_RAW(sub_8212A658) {
  const auto button = Read(base, ctx.r3.u32 + 0xD8);
  if (REXCVAR_GET(pvz_input_trace) && button) {
    static bool traced = false;
    if (!traced) {
      REXLOG_INFO("PvZ Award button rect {},{},{},{} screen {},{},{},{}", Read(base, button + 0x190),
          Read(base, button + 0x194), Read(base, button + 0x1A4), Read(base, button + 0x1A8),
          Read(base, ctx.r3.u32 + 0x40), Read(base, ctx.r3.u32 + 0x44), Read(base, ctx.r3.u32 + 0x48), Read(base, ctx.r3.u32 + 0x4C));
      traced = true;
    }
  }
  if (PcMouseMode() && button) {
    std::optional<pvz::Point> position;
    { std::lock_guard lock(pointer_mutex); position = pointer; }
    const auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt;
    const bool over = local && AwardButtonAt(ctx.r3.u32, *local, base);
    base[button + 0x75] = over; // AwardScreen::Update otherwise forces this true.
    base[button + 0xA2] = over;
  }
  __imp__sub_8212A658(ctx, base);
}

REX_HOOK_RAW(sub_8218A140) {
  ApplyPointer(ctx, ctx.r3.u32, base);
  __imp__sub_8218A140(ctx, base);
}

REX_EXTERN(__imp__sub_82196600);
REX_HOOK_RAW(sub_82196600) {
  // Shared console prompt strip: button glyphs and their action labels.
  // Suppress drawing only; retain the native prompt definitions and input.
  static std::vector<std::pair<uint32_t, bool>> traced;
  const auto trace = std::pair(uint32_t(ctx.lr), MouseActive());
  if (REXCVAR_GET(pvz_input_trace) && std::find(traced.begin(), traced.end(), trace) == traced.end()) {
    REXLOG_INFO("PvZ button hints caller {:08X} visible {}", ctx.lr, !MouseActive());
    traced.push_back(trace);
  }
  if (!MouseActive()) __imp__sub_82196600(ctx, base);
}

REX_EXTERN(__imp__sub_82210768);
REX_EXTERN(__imp__sub_82210308);
REX_HOOK_RAW(sub_82210308) {
  const auto packet = ctx.r3.u32;
  const bool picked = PcMouseMode() && selection.packet >= 0 &&
      packet == selection.bank + 0x50 + uint32_t(selection.packet) * 0x7C;
  __imp__sub_82210308(ctx, base);
  // Conveyor Use removes a packet and shifts its neighbours into this slot.
  // Clear the hand on use, before that slot can become a different plant.
  if (picked) {
    if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ mouse used packet {}", selection.packet);
    selection.packet = -1;
    // Native planting still owns the cursor during Use. The update hook
    // empties the hand after that call has completed.
  }
}

REX_EXTERN(__imp__sub_8249D388);
REX_HOOK_RAW(sub_8220AFA8) {
  const auto chooser = ctx.r3.u32;
  const PPCContext original = ctx;
  if (!MouseActive() || Read(base, chooser) != 0x820CA94C) { __imp__sub_8220AFA8(ctx, base); return; }
  const auto visible = base[chooser + 0x71];
  base[chooser + 0x71] = 1;
  __imp__sub_8220AFA8(ctx, base);
  base[chooser + 0x71] = visible;
  if (ctx.r3.s32 >= 0) return;
  // Fill only the small spacing between cards, using the same hit test for
  // hover and clicks. Do not extend the outside edge of either card group.
  const pvz::Point point{float(original.r4.s32), float(original.r5.s32)};
  const auto imitater = Read(base, chooser + 0xDC);
  if (!base[chooser + 0xEB8] && imitater && base[imitater + 0x70] && !base[imitater + 0x72] &&
      pvz::InRect(point, int32_t(Read(base, imitater + 0x190)), int32_t(Read(base, imitater + 0x194)), 50, 70)) {
    ctx.r3.s64 = 48;
    return;
  }
  struct Card { int seed, x, y; };
  std::vector<Card> cards;
  for (int i = 0; i < 49; ++i) {
    PPCContext work = original; work.r1.u32 -= 0x180;
    work.r3.u32 = Read(base, chooser + 0xE60); work.r4.s64 = i + (base[chooser + 0xEB8] ? 61 : 0); work.r5.u32 = base[chooser + 0xEB8];
    sub_821A8CE8(work, base);
    const auto chosen = chooser + 0xE0 + i * 0x40;
    if (!work.r3.u32 || Read(base, chosen + 0x28) == 4) continue;
    work = original; work.r1.u32 -= 0x180;
    work.r3.u32 = chooser; work.r4.s64 = i;
    work.r5.u32 = ctx.r1.u32 - 0x90; work.r6.u32 = ctx.r1.u32 - 0x8C;
    sub_82208550(work, base);
    cards.push_back({i, int32_t(Read(base, ctx.r1.u32 - 0x90)), int32_t(Read(base, ctx.r1.u32 - 0x8C))});
    if (Read(base, chosen + 0x28) == 1)
      cards.push_back({i, int32_t(Read(base, chosen)), int32_t(Read(base, chosen + 4))});
  }
  float best = 1e9f; int seed = -1;
  for (const auto& card : cards) {
    if (!pvz::InRect(point, card.x - 2, card.y - 2, 54, 74)) continue;
    const bool exterior_x = (point.x < card.x || point.x >= card.x + 50) &&
        std::none_of(cards.begin(), cards.end(), [&](const Card& other) {
          return other.y == card.y && ((point.x < card.x && other.x < card.x) || (point.x >= card.x + 50 && other.x > card.x));
        });
    const bool exterior_y = (point.y < card.y || point.y >= card.y + 70) &&
        std::none_of(cards.begin(), cards.end(), [&](const Card& other) {
          return other.x == card.x && ((point.y < card.y && other.y < card.y) || (point.y >= card.y + 70 && other.y > card.y));
        });
    const float dx = point.x - card.x - 25, dy = point.y - card.y - 35;
    if (!exterior_x && !exterior_y && dx * dx + dy * dy < best) { best = dx * dx + dy * dy; seed = card.seed; }
  }
  ctx.r3.s64 = seed < 0 ? -1 : seed + (base[chooser + 0xEB8] ? 61 : 0);
}
REX_EXTERN(__imp__sub_82206A18);
REX_HOOK_RAW(sub_82206A18) {
  const auto dialog = ctx.r3.u32;
  if (!PcMouseMode()) { __imp__sub_82206A18(ctx, base); return; }
  std::optional<pvz::Point> position;
  { std::lock_guard lock(pointer_mutex); position = pointer; }
  const auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt;
  int seed = -1;
  if (local) {
    const auto parent = Read(base, dialog + 0x24);
    PPCContext work = ctx; work.r1.u32 -= 0x100;
    work.r4.s64 = int(local->x) - int32_t(Read(base, dialog + 0x40)) - (parent ? int32_t(Read(base, parent + 0x40)) : 0);
    work.r5.s64 = int(local->y) - int32_t(Read(base, dialog + 0x44)) - (parent ? int32_t(Read(base, parent + 0x44)) : 0);
    sub_82206970(work, base); seed = work.r3.s32;
  }
  Write(base, dialog + 0x1C8, uint32_t(seed));
  const auto app = Read(base, dialog + 0x1A4);
  const auto native = base[app + 0x5A6]; base[app + 0x5A6] = seed >= 0;
  __imp__sub_82206A18(ctx, base);
  base[app + 0x5A6] = native;
  if (seed < 0) { const auto tooltip = Read(base, dialog + 0x1C0); if (tooltip) base[tooltip + 0x64] = 0; }
}
int HoveredChooserSeed(PPCContext& ctx, uint8_t* base, uint32_t chooser) {
  std::optional<pvz::Point> position;
  { std::lock_guard lock(pointer_mutex); position = pointer; }
  const auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt;
  if (!local) return -1;
  PPCContext work = ctx; work.r1.u32 -= 0x100;
  work.r3.u32 = chooser;
  work.r4.s64 = int(local->x) - int32_t(Read(base, chooser + 0x40));
  work.r5.s64 = int(local->y) - int32_t(Read(base, chooser + 0x44));
  sub_8220AFA8(work, base);
  return int(work.r3.u32);
}
REX_EXTERN(__imp__sub_8220B7B0);
REX_HOOK_RAW(sub_8220B7B0) {
  const auto chooser = ctx.r3.u32;
  const auto user = pvz::LocalPlayersActive() ? uint32_t(std::max(0, pvz::MouseUserIndex())) : 0u;
  const auto offset = user * 4;
  if (MouseActive() && ctx.r4.u32 == user) {
    const int hover = HoveredChooserSeed(ctx, base, chooser);
    if (hover < 0) {
      if (auto tooltip = Read(base, chooser + 0xE7C + offset)) base[tooltip + 0x64] = 0;
      Write(base, chooser + 0xE84 + offset, uint32_t(-1));
      return;
    }
    if (const auto tooltip = Read(base, chooser + 0xE7C + offset); tooltip && !base[tooltip + 0x64])
      Write(base, chooser + 0xE84 + offset, uint32_t(-1));
    const auto saved = Read(base, chooser + 0xEAC + offset);
    const auto old_x = Read(base, chooser + 0xE8C + offset), old_y = Read(base, chooser + 0xE94 + offset);
    std::optional<pvz::Point> position;
    { std::lock_guard lock(pointer_mutex); position = pointer; }
    const auto local = position ? pvz::MapBoardPointer(*position) : std::nullopt;
    if (local) {
      Write(base, chooser + 0xE8C + offset, uint32_t(int(local->x) - int32_t(Read(base, chooser + 0x40))));
      Write(base, chooser + 0xE94 + offset, uint32_t(int(local->y) - int32_t(Read(base, chooser + 0x44))));
    }
    Write(base, chooser + 0xEAC + offset, hover - (base[chooser + 0xEB8] ? 61 : 0));
    __imp__sub_8220B7B0(ctx, base);
    Write(base, chooser + 0xEAC + offset, saved);
    Write(base, chooser + 0xE8C + offset, old_x); Write(base, chooser + 0xE94 + offset, old_y); return;
  }
  __imp__sub_8220B7B0(ctx, base);
}
REX_EXTERN(__imp__sub_82208B18);
REX_HOOK_RAW(sub_82208B18) {
  const auto chooser = ctx.r3.u32;
  const auto offset = pvz::LocalPlayersActive() ? uint32_t(std::max(0, pvz::MouseUserIndex())) * 4 : 0u;
  const auto saved = Read(base, chooser + 0xEAC + offset);
  const auto tooltip = Read(base, chooser + 0xE7C + offset);
  const auto visible = tooltip ? base[tooltip + 0x64] : 0;
  const auto ready = base[chooser + 0xD8];
  if (MouseActive()) base[chooser + 0xD8] = 0; // Clickable text button supplies the label.
  if (MouseActive()) {
    const int hover = HoveredChooserSeed(ctx, base, chooser);
    Write(base, chooser + 0xEAC + offset, hover < 0 ? uint32_t(-1) : hover - (base[chooser + 0xEB8] ? 61 : 0));
    if (tooltip && (hover < 0 || int32_t(Read(base, chooser + 0xE84 + offset)) != hover)) base[tooltip + 0x64] = 0;
  }
  __imp__sub_82208B18(ctx, base);
  base[chooser + 0xD8] = ready;
  if (tooltip) base[tooltip + 0x64] = visible;
  Write(base, chooser + 0xEAC + offset, saved);
}
bool ConsoleCursorImage(uint32_t image, uint8_t* base) {
  if (!PcMouseMode() || !image) return false;
  const auto board = selection.board;
  if (!pvz::ServicePcGarden()) {
    if (!board || !selection.controls || !PlantMouse(selection.controls, base)) return false;
    const auto secondary = Read(base, board + 0x1B4);
    if (!pvz::LocalPlayersActive() && secondary && Read(base, secondary + 0xA0) != uint32_t(-1)) return false;
  }
  for (auto global : {0x82D09F84u, 0x82D0A308u, 0x82D0A324u, 0x82D0A548u, 0x82D0A6A4u, 0x82D0A838u})
    if (image == Read(base, global)) return true;
  return false;
}
REX_EXTERN(__imp__sub_8249D0E8);
REX_EXTERN(__imp__sub_82143880);
REX_HOOK_RAW(sub_82143880) {
  const auto board = ctx.r3.u32;
  const auto hammer = base[board + 0x5693];
  if (PcMouseMode()) base[board + 0x5693] = 0;
  __imp__sub_82143880(ctx, base);
  base[board + 0x5693] = hammer;
}
REX_HOOK_RAW(sub_8249D0E8) {
  if (ctx.lr == 0x82215B70 && ctx.r4.u32 && ctx.r4.u32 == Read(base, 0x82D0A0AC) && REXCVAR_GET(pvz_input_trace)) {
    static std::optional<bool> last_mouse;
    const bool mouse = MouseActive();
    if (last_mouse != mouse) {
      last_mouse = mouse;
      REXLOG_INFO("PvZ Shop open hatchback rendered mouse {}", mouse);
    }
  }
  // The store's taillight overlays contain baked-in LB/RB labels. Mouse
  // navigation provides animated arrow buttons in their place.
  if (MouseActive() && (ctx.lr == 0x822159F8 || ctx.lr == 0x82215A18 ||
      (ctx.lr == 0x82215B70 && (ctx.r4.u32 == Read(base, 0x82D0A6F4) || ctx.r4.u32 == Read(base, 0x82D0A4E8))))) return;
  // DrawTools paints SHOVELBANK and then SHOVEL separately. Keep the native
  // empty tray while the shovel is carried; cursor/coin uses of this image
  // retain their rendering. No replacement asset is required.
  if (PcMouseMode() && selection.shovel && ctx.lr == 0x82143B28) return;
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32) return;
  if (!ConsoleCursorImage(ctx.r4.u32, base)) {
    const bool tool_bank = ctx.lr == 0x82142E9C && pvz::ServicePcGarden();
    if (tool_bank) {
      PPCContext saved = ctx;
      __imp__sub_8249D0E8(ctx, base);
      pvz::DrawServiceToolHover(saved, base);
    } else __imp__sub_8249D0E8(ctx, base);
  }
}
REX_EXTERN(__imp__sub_8249C430);
REX_HOOK_RAW(sub_8249C430) {
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32) return;
  if (ConsoleCursorImage(ctx.r4.u32, base)) return;
  // Move only the native arrow and its shadow. The ghost plant, cell focus,
  // planting position and gamepad attraction keep their native grid logic.
  const bool arrow = console_draw_offset && (ctx.lr == 0x8218D69C || ctx.lr == 0x8218D6FC ||
      ctx.lr == 0x8218D738 || ctx.lr == 0x8218D7CC || ctx.lr == 0x8218D82C);
  const auto matrix = ctx.r5.u32;
  const auto x = arrow ? Read(base, matrix + 8) : 0, y = arrow ? Read(base, matrix + 0x14) : 0;
  if (arrow) {
    Write(base, matrix + 8, std::bit_cast<uint32_t>(std::bit_cast<float>(x) + console_draw_offset->x));
    Write(base, matrix + 0x14, std::bit_cast<uint32_t>(std::bit_cast<float>(y) + console_draw_offset->y));
  }
  __imp__sub_8249C430(ctx, base);
  if (arrow) { Write(base, matrix + 8, x); Write(base, matrix + 0x14, y); }
}

REX_EXTERN(__imp__sub_82580A78);
REX_HOOK_RAW(sub_82580A78) {
  // Only the native primary-player shovel update sees mouse RMB as held B.
  // Other pad queries and the physical controller remain unchanged.
  if (ctx.lr == 0x8218B8EC && ctx.r4.u32 == 7 && MouseActive() && !PcMouseMode() &&
      console_shovel.held && ctx.r31.u32 == console_shovel.controls) {
    ctx.r3.u64 = 1; return;
  }
  __imp__sub_82580A78(ctx, base);
}
REX_HOOK_RAW(sub_8249D388) {
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32) return;
  // Only the two console reticle draws in SeedChooser. Animation and flight
  // draws use other call sites and remain native.
  if (PcMouseMode() && (ctx.lr == 0x82209710 || ctx.lr == 0x82209764) &&
      (!pvz::LocalPlayersActive() || pvz::MouseUserIndex() == (ctx.lr == 0x82209710 ? 0 : 1))) return;
  if (ConsoleCursorImage(ctx.r4.u32, base)) return;
  __imp__sub_8249D388(ctx, base);
}

REX_EXTERN(__imp__sub_8249B408);
REX_HOOK_RAW(sub_8249B408) {
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32 || ConsoleCursorImage(ctx.r4.u32, base)) return;
  __imp__sub_8249B408(ctx, base);
}
REX_EXTERN(__imp__sub_8249BC50);
REX_HOOK_RAW(sub_8249BC50) {
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32 || ConsoleCursorImage(ctx.r4.u32, base)) return;
  __imp__sub_8249BC50(ctx, base);
}
REX_EXTERN(__imp__sub_8249C058);
REX_HOOK_RAW(sub_8249C058) {
  pvz::RecordChallengeRowPaint(ctx, base);
  // Zen's bank selection follows the carried cursor on Xbox. PC tools
  // remain in the hand without permanently selecting their toolbar tile.
  if (pvz::ServicePcGarden() && ctx.lr == 0x82142ED4) {
    if (REXCVAR_GET(pvz_input_trace)) {
      static bool traced = false;
      if (!traced) REXLOG_INFO("PvZ PC garden persistent toolbar highlight suppressed");
      traced = true;
    }
    return;
  }
  // Keep the native card outline, positioned on the actual mouse hover.
  // The bouncing controller arrows remain hidden by their own draw hooks.
  if (PcMouseMode() && ctx.lr == 0x82209480) {
    const auto chooser = ctx.r26.u32;
    const int hover = HoveredChooserSeed(ctx, base, chooser);
    const int index = hover - (base[chooser + 0xEB8] ? 61 : 0);
    if (ctx.r28.u32 != 0x1B0 || hover < 0 || index < 0 || index >= 49) return;
    const auto chosen = chooser + 0xE0 + uint32_t(index) * 0x40;
    ctx.r5.s64 = int32_t(Read(base, chosen)) - 8;
    ctx.r6.s64 = int32_t(Read(base, chosen + 4)) - 4;
  }
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32 || ConsoleCursorImage(ctx.r4.u32, base)) return;
  __imp__sub_8249C058(ctx, base);
}
REX_EXTERN(__imp__sub_8249CFB8);
REX_HOOK_RAW(sub_8249CFB8) {
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32 || ConsoleCursorImage(ctx.r4.u32, base)) return;
  __imp__sub_8249CFB8(ctx, base);
}
REX_EXTERN(__imp__sub_8249DA90);
REX_HOOK_RAW(sub_8249DA90) {
  if (ctx.lr == 0x82143B6C && MouseActive() && !PcMouseMode() && pvz::MouseRmbImage()) {
    // Native B occupies this exact shovel badge slot. Use a mouse drawing,
    // keeping the tool tray, training tint and gamepad atlas unchanged.
    ctx.r4.u32 = pvz::MouseRmbImage();
    const auto graphics = ctx.r3.u32;
    const auto tint = base[graphics + 0x48]; base[graphics + 0x48] = 0;
    __imp__sub_8249D0E8(ctx, base);
    base[graphics + 0x48] = tint;
    return;
  }
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (!ctx.r4.u32 || ConsoleCursorImage(ctx.r4.u32, base)) return;
  __imp__sub_8249DA90(ctx, base);
}

REX_EXTERN(__imp__sub_8220E5D0);
REX_HOOK_RAW(sub_8220E5D0) {
  const auto packet = ctx.r3.u32, bank = Read(base, packet + 0x6C);
  if (!PcMouseMode() || !selection.controls || bank != selection.bank ||
      !PlantMouse(selection.controls, base)) { __imp__sub_8220E5D0(ctx, base); return; }
  const auto original = Read(base, selection.controls + 0xBC);
  const auto time = Read(base, packet + 0x70);
  const bool picked = selection.packet >= 0 && packet == bank + 0x50 + uint32_t(selection.packet) * 0x7C;
  Write(base, selection.controls + 0xBC, picked ? Read(base, packet + 0x48) : 0x7FFFFFFF);
  // Native conveyor packets can retain their old slot index. Selection is
  // the actual card in the hand, not whichever card inherited that index.
  if (!picked) Write(base, packet + 0x70, std::bit_cast<uint32_t>(1.0f));
  __imp__sub_8220E5D0(ctx, base);
  Write(base, packet + 0x70, time);
  Write(base, selection.controls + 0xBC, original);
}

REX_EXTERN(__imp__sub_82284D98);
REX_HOOK_RAW(sub_82284D98) {
  const auto result = ctx.r3.u32, key = ctx.r4.u32;
  constexpr std::u16string_view hint = u"[CLICK_TO_CONTINUE]";
  bool hide = MouseActive() && Read(base, key + 0x14) == hint.size();
  const auto data = Read(base, key + 0x18) >= 8 ? Read(base, key + 4) : key + 4;
  for (size_t i = 0; hide && i < hint.size(); ++i)
    hide = ((unsigned(base[data + i * 2]) << 8) | base[data + i * 2 + 1]) == hint[i];
  __imp__sub_82284D98(ctx, base);
  if (hide) {
    const auto text = Read(base, result + 0x18) >= 8 ? Read(base, result + 4) : result + 4;
    Write(base, result + 0x14, 0); base[text] = base[text + 1] = 0;
  }
}

REX_HOOK_RAW(sub_82210768) {
  const auto bank = ctx.r3.u32;
  const auto board = Read(base, bank + 0x20);
  const auto controls = board ? MouseControls(board, base) : 0;
  if (PcMouseMode() && PlantMouse(controls, base) && bank == selection.bank) {
    static uint32_t drawn_bank = 0;
    static int drawn_packet = -1;
    if (drawn_bank == bank && drawn_packet != selection.packet) {
      const auto count = Read(base, bank + 0x40);
      if (drawn_packet >= 0 && uint32_t(drawn_packet) < count && drawn_packet < 10)
        Write(base, bank + 0x50 + uint32_t(drawn_packet) * 0x7C + 0x70, 0);
    }
    if (REXCVAR_GET(pvz_input_trace) && (drawn_bank != bank || drawn_packet != selection.packet))
      REXLOG_INFO("PvZ PC seed highlight {} (gameplay index {})", selection.packet, Read(base, controls + 0xBC));
    drawn_bank = bank;
    drawn_packet = selection.packet;
    const auto original = Read(base, controls + 0xBC);
    // The renderer compares BC to each packet index without indexing through
    // it. Keep a valid gameplay index outside this draw call (Update needs it).
    Write(base, controls + 0xBC, selection.packet < 0 ? 0x7FFFFFFF : uint32_t(selection.packet));
    __imp__sub_82210768(ctx, base);
    Write(base, controls + 0xBC, original);
    static int animation_frames = 0;
    if (selection.packet >= 0 && REXCVAR_GET(pvz_input_trace) && animation_frames++ < 16) {
      const auto packet = bank + 0x50 + uint32_t(selection.packet) * 0x7C;
      REXLOG_INFO("PvZ seed animation time {} scale {}",
          std::bit_cast<float>(Read(base, packet + 0x70)),
          std::bit_cast<float>(Read(base, packet + 0x78)));
    }
    return;
  }
  __imp__sub_82210768(ctx, base);
}

REX_EXTERN(__imp__sub_821C6468);
REX_EXTERN(__imp__sub_82146678);
REX_EXTERN(__imp__sub_82492A48);
REX_HOOK_RAW(sub_82492A48) {
  // WidgetContainer::DrawAll(this, modalFlags, graphics) draws the Board,
  // then its child buttons. Its final pass is the actual top of this widget.
  const auto widget = ctx.r3.u32;
  PPCContext cursor_draw = ctx;
  cursor_draw.r4.u32 = ctx.r5.u32;
  const bool board = Read(base, widget) == 0x820B728C;
  const auto previous = late_cursor;
  if (board) late_cursor = {widget};
  __imp__sub_82492A48(ctx, base);
  if (board) {
    pvz::DrawServiceCursor(cursor_draw, base, widget);
    if (late_cursor.controls && PcMouseMode()) {
      const auto graphics = cursor_draw.r4.u32;
      std::array<uint32_t, 17> saved{};
      for (size_t i = 0; i < saved.size(); ++i) {
        saved[i] = Read(base, graphics + 8 + uint32_t(i * 4));
        Write(base, graphics + 8 + uint32_t(i * 4), late_cursor.graphics[i]);
      }
      cursor_draw.r1.u32 -= 0x100; cursor_draw.r3.u32 = late_cursor.controls;
      drawing_late_cursor = true;
      sub_8218D0F8(cursor_draw, base);
      drawing_late_cursor = false;
      for (size_t i = 0; i < saved.size(); ++i) Write(base, graphics + 8 + uint32_t(i * 4), saved[i]);
    }
    late_cursor = previous;
  }
}
REX_HOOK_RAW(sub_82146678) {
  const auto board = ctx.r3.u32;
  // Board::DrawOverlay renders the blinking P2_JOIN independently of the
  // shared prompt strip. Match its string key so P2_JOINED stays visible.
  const auto key = board + 0x5864;
  if (MouseActive() && Read(base, board) == 0x820B728C && Read(base, key + 0x14) == 9) {
    const auto data = Read(base, key + 0x18) >= 8 ? Read(base, key + 4) : key + 4;
    constexpr std::u16string_view join = u"[P2_JOIN]";
    bool matches = data != 0;
    for (size_t i = 0; matches && i < join.size(); ++i)
      matches = ((unsigned(base[data + i * 2]) << 8) | base[data + i * 2 + 1]) == join[i];
    if (matches) {
      const auto blink = base[board + 0x585C];
      base[board + 0x585C] = 0;
      static bool traced = false;
      if (REXCVAR_GET(pvz_input_trace) && blink && !traced) {
        REXLOG_INFO("PvZ mouse P2 join hint hidden (native blink retained)");
        traced = true;
      }
      __imp__sub_82146678(ctx, base);
      base[board + 0x585C] = blink;
      return;
    }
  }
  __imp__sub_82146678(ctx, base);
}

REX_HOOK_RAW(sub_821C6468) {
  const auto message = ctx.r3.u32;
  // MessageWidget has a fixed 128-code-unit UTF-16 label. Animated wave text
  // uses per-character reanimations and must retain its original layout.
  if (MouseActive() && int32_t(Read(base, message + 0x32C)) == -1) {
    std::u16string label;
    for (int i = 0; i < 128; ++i) {
      const auto at = message + 0x20 + i * 2;
      const char16_t code = (unsigned(base[at]) << 8) | base[at + 1];
      if (!code) break;
      label += code;
    }
    const auto adapted = pvz::AdaptMouseMessage(label, PcMouseMode());
    if (adapted != label && adapted.size() < 128) {
      std::array<uint8_t, 256> original;
      std::copy_n(base + message + 0x20, original.size(), original.begin());
      for (size_t i = 0; i <= adapted.size(); ++i) {
        const auto code = i < adapted.size() ? adapted[i] : char16_t(0);
        base[message + 0x20 + i * 2] = uint8_t(code >> 8);
        base[message + 0x21 + i * 2] = uint8_t(code);
      }
      static bool traced = false;
      if (REXCVAR_GET(pvz_input_trace) && !traced) {
        REXLOG_INFO("PvZ mouse tutorial label adapted (native label retained)");
        traced = true;
      }
      __imp__sub_821C6468(ctx, base);
      std::copy(original.begin(), original.end(), base + message + 0x20);
      return;
    }
  }
  if (REXCVAR_GET(pvz_input_trace)) {
    static std::string previous;
    std::string label;
    for (int i = 0; i < 128; ++i) {
      const auto at = ctx.r3.u32 + 0x20 + i * 2;
      const unsigned code = (unsigned(base[at]) << 8) | base[at + 1];
      if (!code) break;
      if (code < 128) label += char(code);
      else label += "[" + std::to_string(code) + "]";
    }
    if (label != previous) {
      REXLOG_INFO("PvZ message label {} reanim {}", label, int32_t(Read(base, ctx.r3.u32 + 0x32C)));
      previous = label;
    }
  }
  __imp__sub_821C6468(ctx, base);
}

REX_EXTERN(__imp__sub_824A6890); // WidgetManager::GamepadButtonDown
REX_EXTERN(sub_8228B5C0); // VSSetupMenu::GamepadButtonDown
REX_EXTERN(sub_82491DE8); // XML WidgetContainer::GetWidget(id)
REX_HOOK_RAW(sub_824A6890) {
  if (pvz::RouteHostMenuButton(ctx.r4.u32, ctx.r5.u32)) return;
  // Device observation runs during the preceding pad poll. Restore native
  // focus before forwarding this first press, rather than one Update later.
  if (REXCVAR_GET(pvz_mouse_cursor) && !MouseActive()) DispatchWidgets(ctx, base);
  const auto top = TopRootWidget(ctx.r3.u32, base);
  if (pvz::LocalPlayersActive() && top && Read(base, top) == 0x820CA94C) {
    // Co-op shares a chooser but keeps a cursor and bank per signed-in user.
    // Mouse focus on a child button must not swallow the other player's pad.
    PPCContext work = ctx; work.r1.u32 -= 0x100; work.r3.u32 = top;
    sub_8220C280(work, base);
    if (REXCVAR_GET(pvz_input_trace) && ctx.r4.u32 <= 9)
      REXLOG_INFO("PvZ native co-op chooser button {} user {} cursors {},{} banks {},{}", ctx.r4.u32,
          ctx.r5.u32, Read(base, top + 0xEAC), Read(base, top + 0xEB0), Read(base, top + 0xE74), Read(base, top + 0xE78));
    return;
  }
  if (pvz::LocalPlayersActive() && top && Read(base, top) == 0x820CC2EC && Read(base, top + 0x114) == 1) {
    PPCContext work = ctx; work.r1.u32 -= 0x100; work.r3.u32 = top;
    sub_8228B5C0(work, base);
    if (REXCVAR_GET(pvz_input_trace))
      REXLOG_INFO("PvZ native VS pad button {} user {} routed phase {} sides {},{} ready {},{}",
          ctx.r4.u32, ctx.r5.u32, Read(base, top + 0x114),
          int32_t(Read(base, top + 0x120)), int32_t(Read(base, top + 0x124)),
          base[top + 0x128], base[top + 0x129]);
    return;
  }
  if (pvz::LocalPlayersActive() && top && Read(base, top) == 0x820CC2EC && Read(base, top + 0x114) == 2) {
    const auto focus = Read(base, ctx.r3.u32 + 0xA8);
    // Ready disappears once both sides are confirmed. A mouse-owned focus
    // can still point at that hidden button when P1 uses a pad to start.
    if (!focus || !IsGameButton(Read(base, focus)) || !base[focus + 0x70] || base[focus + 0x72]) {
      // Use the XML lookup used by native SetState; the mode controls need
      // not be direct WidgetContainer children. Preserve its remembered mode.
      PPCContext lookup = ctx; lookup.r1.u32 -= 0x100; lookup.r3.u32 = top;
      const auto mode = Read(base, top + 0x134);
      lookup.r4.u32 = mode == 1 ? 10 : mode == 2 ? 11 : 9;
      sub_82491DE8(lookup, base);
      const auto quick = lookup.r3.u32;
      if (quick) {
        PPCContext work = ctx; work.r1.u32 -= 0x100; work.r4.u32 = quick;
        __imp__sub_824A5F48(work, base);
      }
    }
    if (REXCVAR_GET(pvz_input_trace)) {
      const auto current = Read(base, ctx.r3.u32 + 0xA8);
      REXLOG_INFO("PvZ native VS mode button {} user {} focus {:08X} id {}", ctx.r4.u32,
          ctx.r5.u32, current, current ? Read(base, current + 0x6C) : 0);
    }
    const auto current = Read(base, ctx.r3.u32 + 0xA8);
    if (ctx.r4.u32 == 6 && current && IsGameButton(Read(base, current)))
      native_pad_button_press = {ctx.r3.u32, current, ctx.r5.u32};
  }
  if (top && (Read(base, top) == 0x820CB23C || Read(base, top) == 0x820BB58C)) {
    // Store has no native focusable child. A painted mouse navigation button
    // can leave focus null (or on a child that was hidden on the owner switch).
    // The native Store handler validates the player itself.
    PPCContext focus = ctx; focus.r1.u32 -= 0x100; focus.r4.u32 = top;
    __imp__sub_824A5F48(focus, base);
    if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ native shop/house pad button {} routed", ctx.r4.u32);
  }
  if (top && Read(base, top) == 0x820B899C && ctx.r5.u32 == Read(base, top + 0xC0)) {
    PPCContext focus = ctx; focus.r1.u32 -= 0x100; focus.r4.u32 = top;
    __imp__sub_824A5F48(focus, base);
    if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ native challenge pad button {} routed", ctx.r4.u32);
  }
  const auto root = FindRootWidget(ctx.r3.u32, 0x820B728C, base, true);
  if (root && !base[root + 0x1D9] && (top == root || (top && Read(base, top) == 0x820BA804)) && (ctx.r5.u32 == 0 || pvz::LocalPlayersActive())) {
    PPCContext focus = ctx; focus.r1.u32 -= 0x100; focus.r4.u32 = root;
    __imp__sub_824A5F48(focus, base);
    if (REXCVAR_GET(pvz_input_trace)) REXLOG_INFO("PvZ native board pad button {} routed", ctx.r4.u32);
  }
  __imp__sub_824A6890(ctx, base);
}
REX_EXTERN(__imp__sub_824A68C0); // WidgetManager::GamepadButtonUp
REX_HOOK_RAW(sub_824A68C0) {
  if (pvz::RouteHostMenuButtonUp(ctx.r4.u32, ctx.r5.u32)) return;
  if (ctx.r4.u32 == 6 && native_pad_button_press.manager == ctx.r3.u32 &&
      native_pad_button_press.user == ctx.r5.u32) native_pad_button_press = {};
  __imp__sub_824A68C0(ctx, base);
}

REX_EXTERN(__imp__sub_8249D250); // Graphics::DrawImageCel
REX_HOOK_RAW(sub_8249D250) {
  ctx.r4.u64 = pvz::ResolveControllerImage(ctx.r4.u32, base);
  if (ctx.r4.u32 && !ConsoleCursorImage(ctx.r4.u32, base)) __imp__sub_8249D250(ctx, base);
}
