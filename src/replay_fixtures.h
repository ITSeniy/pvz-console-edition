#pragma once
#include "pointer_mapping.h"
#include <cstdint>
struct PPCContext;

// Diagnostic setup requested by an explicit input replay. The replay thread
// only publishes requests; the guest thread applies them before mouse dispatch.
namespace pvz {
void PublishTestAdventureLevel(int level);
void PublishTestAward();
void PublishTestMode(int mode); // Isolated audit fixtures.
void PublishTestHeavyWeaponSun(int sun);
void PublishTestSlotSeed(int seed); // Native dropped-packet fixture.
void PublishTestGardenCoin(Point); // Native coin in an isolated garden.
void PublishTestCoin(int type); // Native coin of this type on the playing lawn.
void RequestReplayCapture(uint32_t frames); // Frames ~50 ms apart, with pvz_capture_frame.
void PublishTestTwistClick(); // Click a currently legal match.
void PublishTestBeghouledSwipe(); // Swipe a legal pair without changing the board.

// Returns true when a fixture started a new game; the caller must stop
// dispatching this frame. main_menu is the visible, enabled MainMenu or 0.
bool ApplyReplayFixtures(PPCContext&, uint8_t* base, uint32_t focused_root, uint32_t main_menu);
}
