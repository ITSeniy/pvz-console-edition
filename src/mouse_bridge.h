#pragma once
#include <optional>
#include "pointer_mapping.h"
#include "mouse_input.h"
#include <span>

namespace pvz {
void PublishPointer(std::optional<Point> position);
// click_count follows Sexy's convention: 1 = left, -1 = right.
void PublishButton(Point position, int click_count, bool down);
void PublishWheel(Point position, int delta);
void NotifyMouseModeChanged();
void PublishReplayMouseBatch(std::span<const MouseInputEvent> events);
bool ReplayScreenReady(uint32_t vtable);
bool ReplayInputIdle();
bool MouseOwnsInput();
bool HostCursorVisible();
void PublishFocusLost();
void PublishTestAdventureLevel(int level); // Diagnostic replay fixtures only.
void PublishTestAward(); // Diagnostic replay fixtures only.
void PublishTestMode(int mode); // Explicit replay only, isolated audit fixtures.
void PublishTestHeavyWeaponSun(int sun); // Diagnostic replay fixtures only.
void PublishTestSlotSeed(int seed); // Native dropped-packet replay fixture.
void PublishTestGardenCoin(Point); // Native coin in an isolated garden replay.
void PublishTestTwistClick(); // Click a currently legal match in a replay.
void PublishTestBeghouledSwipe(); // Swipe a legal pair without changing the board.
void PublishControllerActivity();
bool ReplaySceneReady(bool board);
bool ReplayTitleReady();
}
