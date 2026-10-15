#pragma once
#include "pointer_mapping.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace pvz {
struct HeavyWeaponPointer {
  float carriage_x; // Native carriage origin (+118) after this update.
  Point aim;        // Native right stick: positive Y points upwards.
};

// Turret ball centre relative to the carriage origin (+118) and Y (+11C),
// measured from guest frames at three carriage positions.
inline constexpr float kHeavyTurretX = 22, kHeavyTurretY = 40;
// Heavy Weapon Deluxe tank steering (HeavyWeapon.exe 00412DC0), scaled from
// its 640x480 field to 800x600: the tank stops within 50 px of the cursor,
// reaches full speed from 100 px, and moves at most 2.5 px per update.
inline constexpr float kHeavyStop = 50, kHeavyFull = 100, kHeavySpeed = 2.5f;
inline constexpr float kHeavyHull = 15; // Distance line below the turret (original 12).
// Wheel animation runs at full rate for any nonzero carriage speed, while
// the scaled approach only decays towards zero; settle below this speed.
inline constexpr float kHeavyMinSpeed = .25f;

// Aim from the turret to the cursor, then drive towards it as Deluxe does:
// speed follows the aim angle, so the turret can aim without moving while the
// cursor is near it, and the carriage settles under a distant cursor.
inline HeavyWeaponPointer MapHeavyWeaponPointer(Point cursor, float x, float y) {
  const float turret_x = x + kHeavyTurretX, turret_y = y + kHeavyTurretY;
  const float dx = cursor.x - turret_x;
  const float half_pi = std::numbers::pi_v<float> / 2;
  // Exactly above the turret keeps a stable vertical aim; below it the
  // angle saturates to horizontal.
  const float angle = std::abs(dx) < .001f ? 0.0f :
      std::clamp(std::atan2(dx, turret_y - cursor.y), -half_pi, half_pi);
  const Point aim{std::sin(angle), std::max(0.0f, std::cos(angle))};
  const float hull = turret_y + kHeavyHull;
  const float distance = cursor.y < hull ? std::hypot(dx, hull - cursor.y) : std::abs(dx);
  float speed = angle * 180 / std::numbers::pi_v<float> / 18 * 1.25f;
  if (distance < kHeavyStop) speed = 0;
  else if (distance < kHeavyFull) speed *= (distance - kHeavyStop) / (kHeavyFull - kHeavyStop);
  speed = std::abs(speed) < kHeavyMinSpeed ? 0 : std::clamp(speed, -kHeavySpeed, kHeavySpeed);
  return {std::clamp(x + speed, 25.0f, 700.0f), aim};
}
}
