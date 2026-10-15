#include "bonus_games.h"
#include "guest_memory.h"
#include "guest_call.h"
#include "guest_layout.h"
#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/func.h>
#include <rex/runtime.h>
#include <algorithm>
#include <array>

REX_EXTERN(sub_821A32C0); // LawnApp::ShowChallengeScreen(page)
REX_EXTERN(sub_821AB788); // LawnApp::GetNumTrophies(page)
REX_EXTERN(sub_824930D0); // WidgetContainer::RemoveWidget
REX_EXTERN(__imp__sub_821A32C0);
REX_EXTERN(__imp__sub_82169968); // ChallengeScreen::GamepadButtonDown

REXCVAR_DEFINE_BOOL(pvz_test_bonus_unlocked, false, "Diagnostics",
    "Show the Bonus Games entry without all Mini-Games trophies");

using namespace pvz::layout;
using pvz::GuestCall;

namespace {
using pvz::guest::Read;
using pvz::guest::Write;
// ChallengeDefinition[94]: mode, thumbnail cel, page, row, column, name.
constexpr uint32_t kDefinitions = 0x82816830, kDefinitionSize = 24, kDefinitionCount = 94;
constexpr uint32_t kLawnApp = 0x82D0BC88;
constexpr uint32_t kChallengeThumbnails = 0x82D0A388; // IMAGE_CHALLENGE_THUMBNAILS
constexpr uint32_t kChallengeBlank = 0x82D0A4EC;      // IMAGE_CHALLENGE_BLANK
constexpr uint32_t kHiddenPage = 99;
// Playable page-2 entries. Zen Garden and Tree of Wisdom have their own
// screens; Upsell, Intro and Squirrel are not games in this build.
constexpr std::array kBonusModes{12, 13, 15, 16, 37, 38, 39, 40, 41, 42, 43, 45, 46, 47, 48, 49, 90};
// survival_thumbnails cels (Hard variants) for the Endless maps.
constexpr std::array<std::pair<int, int>, 4> kSurvivalIcons{{{12, 5}, {13, 6}, {15, 8}, {16, 9}}};
bool drawing_bonus = false;
uint32_t unlocked_screen = 0;
bool unlocked = false;

uint32_t Definition(uint32_t index) { return kDefinitions + index * kDefinitionSize; }
bool BonusMode(int mode) { return std::find(kBonusModes.begin(), kBonusModes.end(), mode) != kBonusModes.end(); }
}

void pvz::PrepareBonusPage(uint8_t* base) {
  static bool prepared = false;
  if (prepared) return;
  prepared = true;
  // The table must still be the one this list was written for.
  if (Read(base, Definition(0)) != 2 || Read(base, Definition(88) + 0) != 90) {
    REXLOG_WARN("PvZ bonus games: unexpected challenge table, page left unchanged");
    return;
  }
  int kept = 0;
  for (uint32_t i = 0; i < kDefinitionCount; ++i) {
    const auto record = Definition(i);
    if (Read(base, record + 8) != kBonusPage) continue;
    if (BonusMode(int(Read(base, record)))) ++kept;
    else Write(base, record + 8, kHiddenPage);
  }
  REXLOG_INFO("PvZ bonus games: {} modes on page {}", kept, kBonusPage);
}

bool pvz::BonusGamesUnlocked(PPCContext& ctx, uint8_t* base, uint32_t screen) {
  if (REXCVAR_GET(pvz_test_bonus_unlocked)) return true;
  if (screen == unlocked_screen) return unlocked;
  unlocked_screen = screen;
  const auto app = Read(base, kLawnApp);
  uint32_t total = 0;
  for (uint32_t i = 0; i < kDefinitionCount; ++i) total += Read(base, Definition(i) + 8) == kMiniGamesPage;
  const auto earned = app ? GuestCall(ctx, base, sub_821AB788, app, kMiniGamesPage) : 0;
  unlocked = total && earned >= total;
  REXLOG_INFO("PvZ bonus games: {}/{} Mini-Games trophies, unlocked {}", earned, total, unlocked);
  return unlocked;
}

void pvz::SwitchChallengePage(PPCContext& ctx, uint8_t* base, uint32_t screen, uint32_t page) {
  const auto app = Read(base, screen + 0x250);
  if (!app || Read(base, app + 0x934) != screen) return;
  // Native Back (82169968, action 7): remove and safely delete the screen,
  // then open the other page instead of returning to the main menu.
  GuestCall(ctx, base, sub_824930D0, Read(base, app + 0x36C), screen);
  GuestCall(ctx, base, rex::runtime::ResolveIndirectFunction(Read(base, Read(base, app) + 0x90)), app, screen);
  Write(base, app + 0x934, 0);
  GuestCall(ctx, base, sub_821A32C0, app, page);
  REXLOG_INFO("PvZ challenge page {} -> {}", Read(base, screen + 0x258), page);
}

bool pvz::DrawingBonusPage() { return drawing_bonus; }
void pvz::SetDrawingBonusPage(bool value) { drawing_bonus = value; }

pvz::BonusIconScope::BonusIconScope(uint8_t* base, uint32_t screen, int id) : base_(base) {
  if (id < 0 || id >= int(kDefinitionCount) || Read(base, screen + 0x258) != kBonusPage) return;
  active_ = true;
  screen_ = screen;
  record_ = Definition(uint32_t(id));
  page_ = Read(base, screen + 0x258);
  cel_ = Read(base, record_ + 4);
  const int mode = int(Read(base, record_));
  for (const auto& [survival, cel] : kSurvivalIcons)
    if (mode == survival) {
      // Pages 0 and 5 draw from survival_thumbnails.
      Write(base, screen + 0x258, 0);
      Write(base, record_ + 4, uint32_t(cel));
      return;
    }
  if (const auto blank = Read(base, kChallengeBlank)) {
    generic_ = true;
    thumbnails_ = Read(base, kChallengeThumbnails);
    Write(base, kChallengeThumbnails, blank);
    Write(base, record_ + 4, 0);
  }
}

pvz::BonusIconScope::~BonusIconScope() {
  if (!active_) return;
  Write(base_, screen_ + 0x258, page_);
  Write(base_, record_ + 4, cel_);
  if (generic_) Write(base_, kChallengeThumbnails, thumbnails_);
}

REX_HOOK_RAW(sub_821A32C0) {
  pvz::PrepareBonusPage(base);
  // AwardScreen's exit (8212C0A8) picks the page from fixed mode ranges:
  // Endless maps would land in Survival, the others in Mini-Games.
  const auto app = ctx.r3.u32;
  if (ctx.lr >= 0x8212C0A8 && ctx.lr < 0x8212C500 && BonusMode(int(GameMode(base, app)))) {
    REXLOG_INFO("PvZ bonus game {} finished: back to Bonus Games", GameMode(base, app));
    ctx.r4.u64 = pvz::kBonusPage;
  }
  __imp__sub_821A32C0(ctx, base);
}

REX_HOOK_RAW(sub_82169968) {
  // Back from Bonus Games returns to Mini-Games, not to the main menu.
  const auto screen = ctx.r3.u32;
  if (ctx.r4.u32 == 7 && Read(base, screen) == kChallengeScreen && Read(base, screen + 0x258) == pvz::kBonusPage) {
    pvz::SwitchChallengePage(ctx, base, screen, pvz::kMiniGamesPage);
    return;
  }
  __imp__sub_82169968(ctx, base);
}
