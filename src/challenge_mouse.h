#pragma once
#include "pointer_mapping.h"
#include <optional>
#include <cstdint>
struct PPCContext;
namespace pvz {
void UpdateChallengePointer(PPCContext&, uint8_t*, uint32_t screen, std::optional<Point>);
void ScrollChallenge(PPCContext&, uint8_t*, uint32_t screen, int steps);
bool ChallengeMouseButton(PPCContext&, uint8_t*, uint32_t screen, Point, int button, bool down);
void CancelChallengeMouse(uint8_t*, uint32_t live_screen);
void RecordChallengeRowPaint(PPCContext&, uint8_t*);
}
