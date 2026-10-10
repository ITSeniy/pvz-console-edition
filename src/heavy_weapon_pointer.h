#pragma once
#include "pointer_mapping.h"
#include <algorithm>
#include <numbers>

namespace pvz {
struct HeavyWeaponPointer {
  float carriage_x;
  Point aim; // Native right stick: positive Y points upwards.
};
inline HeavyWeaponPointer MapHeavyWeaponPointer(Point cursor, float x, float y) {
  // Native PvZ turret pivot and carriage limits; absolute mouse positioning.
  const float dx = cursor.x - (x + 60.0f), dy = y + 20.0f - cursor.y;
  // Preserve a stable vertical aim exactly on/below the pivot.
  const float angle = std::abs(dx) < .001f ? 0.0f : std::clamp(
      std::atan2(dx, dy), -std::numbers::pi_v<float> / 2, std::numbers::pi_v<float> / 2);
  const Point direction{std::sin(angle), std::max(0.0f, std::cos(angle))};
  // The mouse points at the centre of the carriage, whose native origin
  // is 60 pixels left of that centre. Keep aiming from the current turret.
  return {std::clamp(cursor.x - 60.0f, 25.0f, 700.0f), direction};
}
}
