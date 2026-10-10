#pragma once
#include "pointer_mapping.h"

namespace pvz {
// This XEX draws its 800x600 board centered in a 1280x720 guest frame.
// Screen points are normalized to 800x600 by MapPointer, not board-local.
inline std::optional<Point> MapUiPointer(Point screen) {
  Point local{screen.x * 1.6f - 240.0f, screen.y * 1.2f - 60.0f};
  if (!std::isfinite(local.x) || !std::isfinite(local.y) ||
      local.x < -240 || local.y < -60 || local.x >= 1040 || local.y >= 660)
    return std::nullopt;
  return local;
}
inline std::optional<Point> MapBoardPointer(Point screen) {
  const auto local = MapUiPointer(screen);
  if (!local || local->x < 0 || local->y < 0 || local->x >= 800 || local->y >= 600)
    return std::nullopt;
  return local;
}
inline bool InRect(Point p, int x, int y, int width, int height) {
  return width > 0 && height > 0 && p.x >= x && p.y >= y &&
      double(p.x) < double(x) + width && double(p.y) < double(y) + height;
}
}
