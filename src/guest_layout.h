#pragma once
#include "guest_memory.h"

// Verified Xbox XEX layout shared by the mouse adapters. Vtables come from
// RTTI (see input_widget_inventory.h); game modes from gChallengeDefs, whose
// numbering matches the Android build rather than PC.
namespace pvz::layout {
inline constexpr uint32_t kWidgetManager = 0x8206404C;
inline constexpr uint32_t kBoard = 0x820B728C;
inline constexpr uint32_t kMainMenu = 0x820C2784;
inline constexpr uint32_t kTitleScreen = 0x820CB6BC;
inline constexpr uint32_t kSeedChooser = 0x820CA94C;
inline constexpr uint32_t kImitaterDialog = 0x820CAAC4;
inline constexpr uint32_t kAwardScreen = 0x820B4F44;
inline constexpr uint32_t kHelpBar = 0x820BA804;
inline constexpr uint32_t kHelpTextScreen = 0x820BB22C;
inline constexpr uint32_t kHouseMenu = 0x820BBF94;
inline constexpr uint32_t kHouseChooser = 0x820BB58C;
inline constexpr uint32_t kLeaderboardMenu = 0x820C1D24;
inline constexpr uint32_t kMailScreen = 0x820C21A4;
inline constexpr uint32_t kCreditScreen = 0x820B96EC;
inline constexpr uint32_t kVSSetupMenu = 0x820CC2EC;
inline constexpr uint32_t kVSResultsMenu = 0x820CC0AC;
inline constexpr uint32_t kWaitForSecondPlayer = 0x820C0A8C;
inline constexpr uint32_t kStoreScreen = 0x820CB23C;
inline constexpr uint32_t kAlmanac = 0x820B466C;
inline constexpr uint32_t kChallengeScreen = 0x820B899C;
inline constexpr uint32_t kGameButton = 0x820BA0AC;
inline constexpr uint32_t kLawnStoneButton = 0x820BA21C;
inline constexpr uint32_t kStretchableGameButton = 0x820BA38C;
inline constexpr uint32_t kGamepadControls = 0x820BA5E8;
inline constexpr uint32_t kZenControls = 0x820BA670;
inline constexpr uint32_t kTreeControls = 0x820BA62C;
inline constexpr uint32_t kSeedBank = 0x820CAED8;

inline constexpr uint32_t kModeAdventure = 0;
inline constexpr uint32_t kModeSlotMachine = 19;
inline constexpr uint32_t kModeHeavyWeapon = 20;
inline constexpr uint32_t kModeBeghouled = 21;
inline constexpr uint32_t kModeSeeingStars = 23;
inline constexpr uint32_t kModeZombiquarium = 24;
inline constexpr uint32_t kModeBeghouledTwist = 25;
inline constexpr uint32_t kModeWhackAZombie = 31;
inline constexpr uint32_t kModeLastStand = 32;
inline constexpr uint32_t kModeZenGarden = 44;
inline constexpr uint32_t kModeTreeOfWisdom = 51;
inline constexpr uint32_t kModeVersus = 76;
// Co-op boards: each player has a seed bank of their own.
inline bool CoopMode(uint32_t mode) { return mode >= 77 && mode <= 89; }

inline constexpr uint32_t kSceneLevelIntro = 2;
inline constexpr uint32_t kScenePlaying = 3;

inline uint32_t BoardApp(uint8_t* base, uint32_t board) { return guest::Read(base, board + 0xF4); }
inline uint32_t GameMode(uint8_t* base, uint32_t app) { return guest::Read(base, app + 0x9CC); }
inline uint32_t GameScene(uint8_t* base, uint32_t app) { return guest::Read(base, app + 0x9D0); }
inline uint32_t BoardMode(uint8_t* base, uint32_t board) { return GameMode(base, BoardApp(base, board)); }
// VS shows one SeedChooser per side; +EA8 is the signed-in user that owns it
// (the zombie panel is user 0 when player 1 took the zombies).
inline bool VersusChooser(uint8_t* base, uint32_t chooser) {
  const auto app = guest::Read(base, chooser + 0xE60);
  return app && GameMode(base, app) == kModeVersus;
}
inline bool BoardPaused(uint8_t* base, uint32_t board) { return base[board + 0x1D9] != 0; }
// Adventure level 5 teaches the shovel through Board tutorial states 15..17.
inline bool InShovelLesson(uint8_t* base, uint32_t board) {
  const auto tutorial = int32_t(guest::Read(base, board + 0x5620));
  return tutorial >= 15 && tutorial <= 17;
}
}
