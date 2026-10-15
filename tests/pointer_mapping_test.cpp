#include "pointer_mapping.h"
#include "menu_mapping.h"
#include "board_mapping.h"
#include "heavy_weapon_pointer.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <source_location>

void check(bool condition, std::source_location location = std::source_location::current()) {
  if (!condition) { std::cerr << "Pointer mapping check failed at line " << location.line() << '\n'; std::exit(1); }
}
bool near(float actual, float expected) { return std::abs(actual - expected) < 0.01f; }

int main() {
  // Heavy Weapon (Deluxe steering). Carriage origin 300, Y 500: turret at 322,540.
  auto heavy = pvz::MapHeavyWeaponPointer({322, 100}, 300, 500); // Straight above.
  check(near(heavy.carriage_x, 300) && near(heavy.aim.x, 0) && near(heavy.aim.y, 1));
  heavy = pvz::MapHeavyWeaponPointer({700, 100}, 300, 500); // Far up-right.
  check(heavy.carriage_x > 300 && heavy.carriage_x <= 302.5f && heavy.aim.x > 0 && heavy.aim.y > 0);
  check(near(std::hypot(heavy.aim.x, heavy.aim.y), 1));
  heavy = pvz::MapHeavyWeaponPointer({352, 520}, 300, 500); // Near the turret: aim only.
  check(near(heavy.carriage_x, 300) && heavy.aim.x > 0.5f);
  heavy = pvz::MapHeavyWeaponPointer({0, 590}, 300, 500); // Far left, below the hull.
  check(near(heavy.carriage_x, 297.5f) && near(heavy.aim.x, -1) && near(heavy.aim.y, 0));
  heavy = pvz::MapHeavyWeaponPointer({-240, 100}, 25, 500); // Left limit.
  check(near(heavy.carriage_x, 25) && heavy.aim.x < 0);
  heavy = pvz::MapHeavyWeaponPointer({1040, 100}, 700, 500); // Right limit.
  check(near(heavy.carriage_x, 700) && heavy.aim.x > 0);
  heavy = pvz::MapHeavyWeaponPointer({322, 650}, 300, 500); // Below, exactly under.
  check(near(heavy.carriage_x, 300) && near(heavy.aim.x, 0) && near(heavy.aim.y, 1));
  // Settled: the approach does not creep forever with the wheels turning.
  float carriage = 300;
  for (int i = 0; i < 2000; ++i) carriage = pvz::MapHeavyWeaponPointer({600, 300}, carriage, 500).carriage_x;
  check(near(pvz::MapHeavyWeaponPointer({600, 300}, carriage, 500).carriage_x, carriage) && carriage > 450);
  auto point = pvz::MapPointer(640, 360, 1280, 720, true);
  check(point && near(point->x, 400) && near(point->y, 300));
  point = pvz::MapBoardPointer({218, 80}); // Card at guest pixel 348.8,96
  check(point && near(point->x, 108.8f) && near(point->y, 36));
  point = pvz::MapBoardPointer({500, 320});
  check(point && near(point->x, 560) && near(point->y, 324));
  check(!pvz::MapBoardPointer({149, 300}));
  check(!pvz::MapBoardPointer({650, 300}));
  point = pvz::MapUiPointer({0, 0});
  check(point && near(point->x, -240) && near(point->y, -60));
  point = pvz::MapUiPointer({799, 599});
  check(point && point->x > 800 && point->y > 600);
  check(!pvz::MapUiPointer({800, 300}));
  check(!pvz::MapUiPointer({-1, 300}));
  point = pvz::MapBoardPointer({150, 50});
  check(point && near(point->x, 0) && near(point->y, 0));
  check(pvz::InRect({85, 8}, 85, 8, 50, 70));
  check(!pvz::InRect({135, 8}, 85, 8, 50, 70));
  check(!pvz::InRect({90, 78}, 85, 8, 50, 70));
  check(!pvz::InRect({90, 9}, 85, 8, 0, 70));
  // Same location on a display with 200% physical pixel density.
  point = pvz::MapPointer(1280, 720, 2560, 1440, true);
  check(point && near(point->x, 400) && near(point->y, 300));
  // 4:3 host: the 16:9 guest occupies y=150..1050, with dead bars.
  check(!pvz::MapPointer(500, 149, 1600, 1200, true));
  point = pvz::MapPointer(0, 150, 1600, 1200, true);
  check(point && near(point->x, 0) && near(point->y, 0));
  check(pvz::MainMenuButtonAt({230, 320}) == 0);
  check(pvz::MainMenuButtonAt({441, 395}) == 4);
  check(pvz::MainMenuButtonAt({478, 444}) == 5);
  check(pvz::MainMenuButtonAt({510, 397}) == 6);
  check(pvz::MainMenuButtonAt({546, 447}) == 7);
  check(pvz::MainMenuButtonAt({480, 380}) == -1); // Gap between flowers
  check(pvz::MainMenuButtonAt({441, 365}) == -1); // Outside petal hit area
  check(pvz::MainMenuButtonAt({149, 320}) == -1); // Left of Board/UI canvas
  check(pvz::MainMenuButtonAt({700, 320}) == -1); // Right of Board/UI canvas
  check(pvz::MainMenuButtonAt({400, 300}) == -1); // Background inside canvas
  check(!pvz::MapPointer(500, 1050, 1600, 1200, true));
  // Wide host: ignore side bars; half-open right/bottom edges avoid OOB clicks.
  check(!pvz::MapPointer(399, 300, 2400, 900, true));
  point = pvz::MapPointer(400, 0, 2400, 900, true);
  check(point && near(point->x, 0));
  check(!pvz::MapPointer(2000, 300, 2400, 900, true));
  check(!pvz::MapPointer(10, 10, 0, 720, true));
  check(!pvz::MapPointer(std::numeric_limits<float>::quiet_NaN(), 10, 1280, 720, true));
  check(!pvz::MapPointer(10, 10, std::numeric_limits<float>::infinity(), 720, true));
  check(!pvz::MapPointer(10, 10, 1280, 720, true, 800, 600,
      std::numeric_limits<float>::infinity()));
  point = pvz::MapPointer(0, 0, 1600, 1200, false);
  check(point && near(point->x, 0) && near(point->y, 0));
}
