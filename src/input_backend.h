#pragma once
#include <memory>
#include <rex/system/interfaces/input.h>
std::unique_ptr<rex::system::IInputSystem> CreatePvzInputSystem(bool tool_mode);

#include "input_activity.h"
namespace pvz {
void PublishReplayPad(PadSample sample, uint32_t device = 0);
void BeginLocalPlayers(bool mouse_first);
void ResetLocalPlayers();
bool LocalPlayersActive();
bool LocalMouseAssigned();
int MouseUserIndex();
void ObserveLocalMouse();
// The device a local player (0, 1) uses: none yet, the mouse, or a pad.
enum class PadKind { None, Mouse, Xbox, PlayStation };
PadKind LocalPlayerPad(int user);
}
