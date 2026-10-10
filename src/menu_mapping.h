#pragma once
#include "pointer_mapping.h"

namespace pvz {
// Center panel only. These fallback hit areas were measured from the guest
// frame because reanimation attachments leave GameButton's Widget rect at 0,0.
// Adventure's area returns 0; the bridge resolves the visible native variant
// (0 = Start Adventure, 1 = Adventure with profile progress).
inline int MainMenuButtonAt(Point p) {
  if (p.x >= 155 && p.x < 315 && p.y >= 265 && p.y < 370) return 0;
  if (p.x >= 165 && p.x < 310 && p.y >= 380 && p.y < 450) return 2;
  if (p.x >= 325 && p.x < 410 && p.y >= 320 && p.y < 405) return 3;
  if (p.x >= 570 && p.x < 655 && p.y >= 355 && p.y < 405) return 8;
  const auto flower = [&](float x, float y) {
    return (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y) < 26 * 26;
  };
  if (flower(441, 395)) return 4; // Achievements
  if (flower(478, 444)) return 5; // Options
  if (flower(510, 397)) return 6; // Marketplace / buy full game
  if (flower(546, 447)) return 7; // Xbox dashboard / quit
  return -1;
}
}
