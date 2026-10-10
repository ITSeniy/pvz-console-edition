#include "replay_fixtures.h"
#include "mouse_bridge.h"
#include "board_mapping.h"
#include "guest_memory.h"
#include "guest_call.h"
#include "guest_layout.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <span>
#include <vector>
#include <atomic>
#include <mutex>
#include <optional>

REX_EXTERN(sub_82133968); // Board::AddCoin(x, y, type, motion)
REX_EXTERN(sub_821A1BD0); // LawnApp::StartGame(mode,load)
REX_EXTERN(sub_821A3070); // LawnApp::ShowAwardScreen
REX_EXTERN(sub_82149848); // Board::GridToPixelY
REX_EXTERN(sub_82150EB8); // BeghouledTwistMoveCausesMatch (scratch copy)
REX_EXTERN(sub_8214F5E8); // LoadBeghouledBoardState (scratch copy)
REX_EXTERN(sub_82151600); // BeghouledMoveCausesMatch (scratch copy)

using namespace pvz::layout;
using pvz::GuestCall;

namespace {
using pvz::guest::Read;
using pvz::guest::Write;
constexpr uint32_t kLawnApp = 0x82D0BC88; // Verified global LawnApp.

std::atomic_int test_adventure_level{0};
std::atomic_bool test_award{false};
std::atomic_int test_mode{-1};
std::atomic_int test_heavy_weapon_sun{-1};
std::atomic_int test_slot_seed{-1};
std::mutex garden_coin_mutex;
std::optional<pvz::Point> test_garden_coin;
std::atomic_bool test_twist_click{false};
std::atomic_bool test_beghouled_swipe{false};

// 144 and 151 are Zen (44) and Tree (51) with a prepared profile.
constexpr std::array kFixtureModes{144, 151, 51, 52, 53, 37, 90, 19, 20, 21, 23, 24, 25, 31, 32};
constexpr std::array kSunModes{kModeSlotMachine, kModeHeavyWeapon, kModeBeghouled, kModeSeeingStars,
    kModeZombiquarium, kModeBeghouledTwist, kModeWhackAZombie, kModeLastStand};

struct PendingSwipe {
  std::vector<pvz::MouseInputEvent> steps;
  size_t next = 0;
  std::chrono::steady_clock::time_point due;
} pending_swipe;

bool Contains(const auto& values, auto value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}
// Live, unpaused Board in the given game mode (any mode when mode < 0).
bool PlayingBoard(uint8_t* base, uint32_t root, int mode, bool unpaused = true) {
  if (!root || Read(base, root) != kBoard || (unpaused && BoardPaused(base, root))) return false;
  return mode < 0 || BoardMode(base, root) == uint32_t(mode);
}

void StartFixtureMode(PPCContext& ctx, uint8_t* base, uint32_t menu, int mode) {
  const auto app = Read(base, kLawnApp);
  const auto profile = Read(base, app + 0x9FC);
  // The native game constructor owns all objects for the selected mode.
  if (mode == kModeTreeOfWisdom) Write(base, profile + 0x424, 1005);
  if (mode == 144) {
    Write(base, profile + 0x3FC, 1); // Mushroom Garden purchase.
    Write(base, profile + 0x418, 1); // Aquarium purchase.
  }
  if (mode == 151) {
    // Store::PurchaseItem(27) sets ownership and height to one. Tree's
    // constructor starts its first-visit speech while food is below 1000.
    Write(base, profile + 0x420, 1);
    Write(base, profile + 0x2E8, 1);
    Write(base, profile + 0x424, 0);
  }
  base[menu + 0x70] = 0; // Direct entry bypasses MainMenu's exit animation.
  GuestCall(ctx, base, sub_821A1BD0, app, mode == 151 ? 51 : mode == 144 ? 44 : mode, 0, 0, 0);
  REXLOG_INFO("PvZ replay fixture game mode {}", mode);
}

void AddGardenCoin(PPCContext& ctx, uint8_t* base, uint32_t board, pvz::Point point) {
  const auto local = pvz::MapBoardPointer(point);
  if (!local) return;
  // Native silver coin, ordinary coin motion.
  if (GuestCall(ctx, base, sub_82133968, board, int(local->x), int(local->y), 1, 3)) REXLOG_INFO("PvZ replay fixture garden coin at {},{}", local->x, local->y);
}

void DropSlotSeed(PPCContext& ctx, uint8_t* base, uint32_t board, int seed) {
  if (const auto coin = GuestCall(ctx, base, sub_82133968, board, 360, 85, 16, 3)) {
    Write(base, coin + 0x98, uint32_t(seed));
    REXLOG_INFO("PvZ replay fixture Slot Machine dropped seed {}", seed);
  }
}

// Imported boards can refill differently on startup. Find a legal move
// through the native match checker, then exercise normal mouse dispatch.
// Do not edit plants, the RNG, the score or the rotation state.
void SwipeBeghouled(PPCContext& ctx, uint8_t* base, uint32_t board) {
  const auto challenge = Read(base, board + 0x1D4), state = ctx.r1.u32 - 0x400;
  GuestCall<0x800>(ctx, base, sub_8214F5E8, challenge, state);
  if (Read(base, challenge + 0x74) == 0) for (int col = 0; col < 8; ++col)
    for (int row = 0; row < 5; ++row)
      for (const auto direction : {pvz::Point{1, 0}, pvz::Point{0, 1}}) {
        if (col + int(direction.x) >= 8 || row + int(direction.y) >= 5) continue;
        if (!GuestCall<0x800>(ctx, base, sub_82151600, challenge, col, row,
            col + int(direction.x), row + int(direction.y), state)) continue;
        const int y = int32_t(GuestCall<0x800>(ctx, base, sub_82149848, board, col, row));
        const pvz::Point start{200.f + col * 50, (float(y) + 110) / 1.2f};
        const pvz::Point end{start.x + direction.x * 25, start.y + direction.y * 25};
        // Publish the drag over several updates, like a real mouse: Console
        // Beghouled follows the grabbed plant's cursor between updates.
        pending_swipe = {};
        pending_swipe.steps.push_back({pvz::MouseEventKind::Button, start, 1, true});
        for (int step = 1; step <= 5; ++step)
          pending_swipe.steps.push_back({pvz::MouseEventKind::Move, pvz::Point{start.x + (end.x - start.x) * step / 5,
              start.y + (end.y - start.y) * step / 5}});
        pending_swipe.steps.push_back({pvz::MouseEventKind::Button, end, 1, false});
        pending_swipe.due = std::chrono::steady_clock::now();
        REXLOG_INFO("PvZ replay Beghouled swipe {},{} to {},{}", col, row,
            col + int(direction.x), row + int(direction.y));
        return;
      }
  REXLOG_WARN("PvZ replay Beghouled swipe: no legal pair ready");
}

void ClickTwist(PPCContext& ctx, uint8_t* base, uint32_t board) {
  const auto challenge = Read(base, board + 0x1D4), state = ctx.r1.u32 - 0x400;
  GuestCall<0x800>(ctx, base, sub_8214F5E8, challenge, state);
  if (Read(base, challenge + 0x74) == 0) for (int col = 0; col < 7; ++col)
    for (int row = 0; row < 4; ++row) {
      if (!GuestCall<0x800>(ctx, base, sub_82150EB8, col, row, state)) continue;
      const pvz::Point point{225.f + col * 50, 200.f + row * 100 / 1.2f};
      pvz::PublishButton(point, 1, true); pvz::PublishButton(point, 1, false);
      REXLOG_INFO("PvZ replay Twist match click block {},{}", col, row);
      return;
    }
  REXLOG_WARN("PvZ replay Twist match click: no legal block ready");
}
}

// Requests are always consumed, so a stale one cannot fire on a later screen.
bool pvz::ApplyReplayFixtures(PPCContext& ctx, uint8_t* base, uint32_t focused_root, uint32_t main_menu) {
  const bool replay = !rex::cvar::Query<std::string>("pvz_input_replay").empty();
  if (pending_swipe.next < pending_swipe.steps.size() && std::chrono::steady_clock::now() >= pending_swipe.due) {
    pvz::PublishReplayMouseBatch(std::span(&pending_swipe.steps[pending_swipe.next++], 1));
    pending_swipe.due = std::chrono::steady_clock::now() + std::chrono::milliseconds(60);
  }
  const bool on_menu = focused_root && Read(base, focused_root) == kMainMenu;
  if (const int mode = test_mode.exchange(-1); replay && on_menu && Contains(kFixtureModes, mode)) {
    StartFixtureMode(ctx, base, focused_root, mode);
    return true;
  }
  const int sun = test_heavy_weapon_sun.exchange(-1);
  if (replay && sun >= 0 && PlayingBoard(base, focused_root, -1, false) &&
      Contains(kSunModes, BoardMode(base, focused_root))) {
    Write(base, focused_root + 0x55F4, uint32_t(sun));
    REXLOG_INFO("PvZ replay fixture Heavy Weapon sun {}", sun);
  }
  const int seed = test_slot_seed.exchange(-1);
  std::optional<pvz::Point> coin;
  { std::lock_guard lock(garden_coin_mutex); coin.swap(test_garden_coin); }
  if (replay && coin && PlayingBoard(base, focused_root, 44)) AddGardenCoin(ctx, base, focused_root, *coin);
  if (replay && seed >= 0 && PlayingBoard(base, focused_root, 19)) DropSlotSeed(ctx, base, focused_root, seed);
  if (test_beghouled_swipe.exchange(false) && replay && PlayingBoard(base, focused_root, 21))
    SwipeBeghouled(ctx, base, focused_root);
  if (test_twist_click.exchange(false) && replay && PlayingBoard(base, focused_root, 25))
    ClickTwist(ctx, base, focused_root);
  if (main_menu) {
    const int level = test_adventure_level.exchange(0);
    if (replay && level >= 1 && level <= 50) {
      const auto app = Read(base, kLawnApp);
      const auto profile = app ? Read(base, app + 0x9FC) : 0;
      if (profile) { Write(base, profile + 0x21C, level); REXLOG_INFO("PvZ replay fixture Adventure level {}", level); }
    }
    if (test_award.exchange(false) && replay) {
      base[main_menu + 0x70] = 0; // Isolated diagnostic screen; do not leave the menu over it.
      GuestCall(ctx, base, sub_821A3070, Read(base, kLawnApp), 0, 0, 0, 0);
      REXLOG_INFO("PvZ replay Award screen {:08X}", Read(base, Read(base, kLawnApp) + 0x92C));
    }
  }
  return false;
}

void pvz::PublishTestAdventureLevel(int level) { test_adventure_level.store(level); }
void pvz::PublishTestAward() { test_award.store(true); }
void pvz::PublishTestMode(int mode) { test_mode.store(mode); }
void pvz::PublishTestHeavyWeaponSun(int sun) { test_heavy_weapon_sun.store(std::clamp(sun, 0, 9990)); }
void pvz::PublishTestSlotSeed(int seed) { if (seed >= 0 && seed < 48) test_slot_seed.store(seed); }
void pvz::PublishTestGardenCoin(Point point) { std::lock_guard lock(garden_coin_mutex); test_garden_coin = point; }
void pvz::PublishTestTwistClick() { test_twist_click.store(true); }
void pvz::PublishTestBeghouledSwipe() { test_beghouled_swipe.store(true); }
