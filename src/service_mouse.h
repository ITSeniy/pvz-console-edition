#pragma once
#include "pointer_mapping.h"
#include <optional>
#include <cstdint>
struct PPCContext;
namespace pvz {
void UpdateServicePointer(PPCContext&, uint8_t*, uint32_t screen, std::optional<Point>);
void ScrollService(PPCContext&, uint8_t*, uint32_t screen, int steps);
bool ServiceMouseButton(PPCContext&, uint8_t*, uint32_t screen, Point, int button, bool down);
void CancelServiceMouse(PPCContext&, uint8_t*, uint32_t live_board);
bool ServiceItemCursor();
bool ServicePcGarden();
bool CancelCarriedGardenTool(PPCContext&, uint8_t*, uint32_t board);
void DrawServiceToolHover(PPCContext&, uint8_t*);
void DrawServiceCursor(PPCContext&, uint8_t*, uint32_t board);
}
