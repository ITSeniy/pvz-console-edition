#pragma once
#include <cstdint>
struct PPCContext;

// Bonus mini-games: ChallengeScreen's unused page 2 (PC's hidden "limbo"
// page), opened from Mini-Games once all of its trophies are earned.
namespace pvz {
inline constexpr uint32_t kMiniGamesPage = 1;
inline constexpr uint32_t kBonusPage = 2;
// Drops page-2 entries that are not playable bonus games (once).
void PrepareBonusPage(uint8_t* base);
// All Mini-Games trophies (or pvz_test_bonus_unlocked); cached per screen.
bool BonusGamesUnlocked(PPCContext& ctx, uint8_t* base, uint32_t screen);
// Replaces ChallengeScreen `screen` by the screen of `page`, like native Back.
void SwitchChallengePage(PPCContext& ctx, uint8_t* base, uint32_t screen, uint32_t page);
// Around ChallengeScreen::Draw: the header reads "Bonus Games" on page 2.
bool DrawingBonusPage();
void SetDrawingBonusPage(bool value);
// Around ChallengeScreen::DrawChallenge for record `id`: own icons for the
// survival maps, the game's blank challenge frame for the others.
class BonusIconScope {
 public:
  BonusIconScope(uint8_t* base, uint32_t screen, int id);
  ~BonusIconScope();
  BonusIconScope(const BonusIconScope&) = delete;
  BonusIconScope& operator=(const BonusIconScope&) = delete;
 private:
  uint8_t* base_;
  uint32_t screen_ = 0, record_ = 0, page_ = 0, cel_ = 0, thumbnails_ = 0;
  bool active_ = false, generic_ = false;
};
}
