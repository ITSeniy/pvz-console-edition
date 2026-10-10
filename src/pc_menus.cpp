#include "pc_menus.h"
#include "guest_memory.h"
#include "widget_tree.h"
#include "mouse_bridge.h"
#include "input_backend.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <mutex>
#include <atomic>
#include <deque>
#include <utility>

REX_EXTERN(__imp__sub_821C0630);
REX_EXTERN(__imp__sub_82289228); // TypingCheck::Check
REX_EXTERN(sub_821475A8); // Board::DoTypingCheck
REX_EXTERN(sub_821258D8);
REX_EXTERN(sub_821256C0);
REX_EXTERN(sub_821A3FF8); // LawnApp::DoDialog, modal result 1000/1001
REX_EXTERN(sub_824A5F48);
REX_EXTERN(sub_824A6208); // WidgetManager::MouseMove

namespace {
using pvz::guest::Read;
using pvz::guest::Write;
std::mutex mutex;
std::function<void(pvz::HostMenu)> callback;
pvz::CheatState state;
std::deque<size_t> commands;
std::atomic_bool open{false};
std::atomic<uint32_t> buttons{0};
std::atomic<uint32_t> held_buttons{0};
// Guest thread: the panel whose button last left the main menu, and whether
// the current MainMenu instance has been handled.
uint32_t return_panel = 1;
bool menu_seen = false;
// Override matching for exactly one native evaluation, on the guest thread.
// All effects and Tree requirements still run inside Board::DoTypingCheck.
thread_local uint32_t requested_check = 0;
constexpr std::array<uint32_t, 7> checks{2684, 2692, 2700, 2704, 2708, 2712, 2716};
constexpr std::array<int, 7> heights{0, 0, 0, 1000, 500, 100, 0};
uint32_t Call(PPCContext& ctx, uint8_t* b, PPCFunc* fn, std::initializer_list<uint32_t> args) {
  PPCContext work = ctx; work.r1.u32 -= 0x800;
  const std::array regs{&work.r3, &work.r4, &work.r5, &work.r6, &work.r7, &work.r8, &work.r9, &work.r10};
  size_t i = 0; for (auto arg : args) regs[i++]->u64 = arg;
  fn(work, b); return work.r3.u32;
}
uint32_t ConfirmExit(PPCContext& ctx, uint8_t* b, uint32_t app) {
  const std::array<std::u16string_view, 4> text{u"Quit Plants vs. Zombies?",
      u"Are you sure you want to quit the game?", u"Quit", u"Cancel"};
  std::array<uint32_t, 4> strings{};
  for (size_t i = 0; i < text.size(); ++i) {
    const auto chars = ctx.r1.u32 - 0x500 + uint32_t(i * 0x100);
    // DoDialog accepts raw UTF-16 pointers and constructs its own strings.
    strings[i] = chars;
    for (size_t j = 0; j <= text[i].size(); ++j) {
      const auto ch = j < text[i].size() ? text[i][j] : char16_t(0);
      b[chars + j * 2] = uint8_t(ch >> 8); b[chars + j * 2 + 1] = uint8_t(ch);
    }
  }
  const auto result = Call(ctx, b, sub_821A3FF8,
      {app, 42, strings[0], strings[1], strings[2], strings[3], 1});
  return result;
}
}
void pvz::SetHostMenuCallback(std::function<void(HostMenu)> fn) {
  std::lock_guard lock(mutex); callback = std::move(fn);
}
void pvz::RequestHostMenu(HostMenu menu) {
  std::lock_guard lock(mutex); if (callback) callback(menu);
}
void pvz::SetHostMenuOpen(bool value) { open.store(value); buttons.store(0); held_buttons.store(0); }
bool pvz::HostMenuOpen() { return open.load(); }
bool pvz::RouteHostMenuButton(uint32_t button, uint32_t player) {
  if (!open.load()) return false;
  if (player == 0 && button < 32) {
    const auto bit = 1u << button;
    if (!(held_buttons.fetch_or(bit) & bit)) buttons.fetch_or(bit);
  }
  return true;
}
bool pvz::RouteHostMenuButtonUp(uint32_t button, uint32_t player) {
  if (!open.load()) return false;
  if (player == 0 && button < 32) held_buttons.fetch_and(~(1u << button));
  return true;
}
uint32_t pvz::TakeHostMenuButtons() { return buttons.exchange(0); }
pvz::CheatState pvz::ReadCheatState() { std::lock_guard lock(mutex); return state; }
void pvz::RequestCheat(size_t index) {
  if (index < checks.size()) { std::lock_guard lock(mutex); commands.push_back(index); }
}
void pvz::UpdatePcMenus(PPCContext& ctx, uint8_t* b, uint32_t manager) {
  uint32_t app = 0;
  guest::VisitWidgets(b, manager, [&](uint32_t widget) {
    const auto vt = Read(b, widget);
    if (vt == 0x820C2784) app = Read(b, widget + 0x110);
    else if (vt == 0x820B728C) app = Read(b, widget + 0xF4);
    else if (vt == 0x820BAA54) app = Read(b, widget + 0x1A4);
    return !app;
  });
  CheatState snapshot;
  if (app) {
    snapshot.available = true;
    const auto profile = Read(b, app + 0x9FC);
    const int height = profile ? int32_t(Read(b, profile + 0x2E8)) : 0;
    std::deque<size_t> pending;
    { std::lock_guard lock(mutex); pending.swap(commands); }
    for (auto index : pending) {
      const auto board = Read(b, app + 0x900);
      if (board && guest::LiveWidget(b, manager, board) && Read(b, board) == 0x820B728C) {
        requested_check = Read(b, app + checks[index]);
        if (requested_check) Call(ctx, b, sub_821475A8, {board});
        requested_check = 0;
      } else if (height >= heights[index]) {
        // The constructor copies these native app preferences to a new Board.
        b[app + 0xAA0 + index] = !b[app + 0xAA0 + index];
      }
      REXLOG_INFO("PvZ native cheat {} enabled {}", index, bool(b[app + 0xAA0 + index]));
    }
    for (size_t i = 0; i < 7; ++i) {
      snapshot.enabled[i] = b[app + 0xAA0 + i] != 0;
      snapshot.allowed[i] = height >= heights[i];
    }
  }
  { std::lock_guard lock(mutex); state = snapshot; }
}
REX_HOOK_RAW(sub_82289228) {
  if (requested_check) { ctx.r3.u64 = ctx.r3.u32 == requested_check; return; }
  __imp__sub_82289228(ctx, base);
}
REX_HOOK_RAW(sub_822CA128) {
  pvz::RequestHostMenu(pvz::HostMenu::Achievements);
  ctx.r3.u64 = 0;
}
REX_HOOK_RAW(sub_821C0630) {
  const auto menu = ctx.r3.u32 - 0xD4;
  // Native depress accepts input only on a settled panel after the intro.
  if (Read(base, menu + 0x118) == Read(base, menu + 0x11C) && !base[menu + 0xD8] &&
      int32_t(Read(base, menu + 0x168)) <= 0) return_panel = Read(base, menu + 0x118);
  if ((ctx.r4.u32 == 10 || ctx.r4.u32 == 11) && Read(base, menu + 0x118) == Read(base, menu + 0x11C) &&
      !base[menu + 0xD8]) pvz::BeginLocalPlayers(pvz::MouseOwnsInput());
  const bool options = ctx.r4.u32 == 5;
  const auto manager = Read(base, menu + 0x20);
  if (ctx.r4.u32 == 7 && Read(base, menu + 0x118) == Read(base, menu + 0x11C) &&
      !base[menu + 0xD8] && int32_t(Read(base, menu + 0x168)) <= 0) {
    const auto app = Read(base, menu + 0x110);
    const auto result = ConfirmExit(ctx, base, app);
    REXLOG_INFO("PvZ PC quit confirmation result {}", result);
    if (result == 1000) pvz::RequestHostMenu(pvz::HostMenu::Quit);
    return;
  }
  // MainMenu::GotFocus restores this native button ID after the modal closes.
  // Restoring only WidgetManager's child pointer leaves Adventure remembered.
  if (options) Write(base, menu + 0x148, 5);
  __imp__sub_821C0630(ctx, base);
  // The native modal caller takes focus back after RemovedFromManager.
  // Restore Options after that final handoff, so Adventure is not selected.
  if (options && !pvz::MouseOwnsInput() && pvz::guest::LiveWidget(base, manager, menu)) {
    uint32_t target = 0;
    pvz::guest::VisitWidgets(base, manager, [&](uint32_t widget) {
      if (Read(base, widget + 0x24) == menu && Read(base, widget + 0x6C) == 5) target = widget;
      return !target;
    });
    if (target) {
      Write(base, menu + 0x148, 5);
      // The modal restores the cached mouse position, which may lie on
      // Adventure although a controller owns input. Flush that hover too.
      Call(ctx, base, sub_824A6208, {manager, uint32_t(-1), uint32_t(-1)});
      pvz::guest::VisitWidgets(base, menu, [&](uint32_t widget) {
        if (Read(base, widget + 0x24) == menu) {
          base[widget + 0xA2] = widget == target;
          base[widget + 0x75] = widget == target;
          base[widget + 0x74] = 0;
        }
        return true;
      });
      Call(ctx, base, sub_824A5F48, {manager, target});
    }
    REXLOG_INFO("PvZ settings returned gamepad focus to Options {}", bool(target));
  }
}
REX_EXTERN(__imp__sub_821A25C8);
REX_HOOK_RAW(sub_821A25C8) {
  pvz::ResetLocalPlayers();
  __imp__sub_821A25C8(ctx, base);
}

void pvz::RestoreMenuPanel(PPCContext& ctx, uint8_t* b, uint32_t manager) {
  uint32_t menu = 0;
  int budget = 64;
  guest::VisitChildren(b, manager, budget, [&](uint32_t child) {
    if (Read(b, child) == 0x820C2784 && b[child + 0x70]) menu = child;
    return !menu;
  });
  if (!menu) { menu_seen = false; return; }
  // Wait for the new menu to settle on its main panel (1).
  if (menu_seen || Read(b, menu + 0x118) != 1 || Read(b, menu + 0x11C) != 1 || b[menu + 0xD8]) return;
  menu_seen = true;
  const auto panel = std::exchange(return_panel, 1u);
  if (panel != 0 && panel != 2) return;
  // Returning from a sub-screen skips the crow intro, then slides through the
  // native More Ways to Play (2) or Extras (8) button.
  Write(b, menu + 0x168, 0);
  Call(ctx, b, __imp__sub_821C0630, {menu + 0xD4, panel == 0 ? 2u : 8u});
  REXLOG_INFO("PvZ main menu returned to panel {}", panel);
}
