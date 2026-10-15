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
// The mouse owns input and, in a local match, belongs to player 1: screen
// texts, hints and help pictures then describe the mouse. With a pad as
// player 1 they keep the console presentation.
bool MouseOwnsMenus();
// Select Second Player is on screen (as of the last widget update).
bool SecondPlayerPromptOpen();
bool HostCursorVisible();
void PublishFocusLost();
void PublishControllerActivity();
bool ReplaySceneReady(bool board);
bool ReplayTitleReady();
}
