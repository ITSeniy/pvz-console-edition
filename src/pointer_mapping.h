#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace pvz {
struct Point { float x, y; };

// MouseEvent uses physical pixels. The framebuffer is presented at 16:9;
// return normalized screen coordinates (Board has a separate draw offset).
inline std::optional<Point> MapPointer(float x, float y, float width, float height,
    bool letterbox, float canvas_width = 800, float canvas_height = 600,
    float aspect = 16.0f / 9.0f) {
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
      !std::isfinite(height) || !std::isfinite(canvas_width) ||
      !std::isfinite(canvas_height) || !std::isfinite(aspect) || !(width > 0 && height > 0 &&
      canvas_width > 0 && canvas_height > 0 && aspect > 0)) return std::nullopt;
  float view_width = width, view_height = height;
  if (letterbox) {
    view_width = std::min(width, height * aspect);
    view_height = view_width / aspect;
  }
  const float left = (width - view_width) / 2, top = (height - view_height) / 2;
  if (x < left || y < top || x >= left + view_width || y >= top + view_height)
    return std::nullopt;
  return Point{(x - left) * canvas_width / view_width,
               (y - top) * canvas_height / view_height};
}
}
