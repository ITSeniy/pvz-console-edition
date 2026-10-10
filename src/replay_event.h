#pragma once
#include "pointer_mapping.h"
#include "board_mapping.h"
#include <array>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace pvz {
enum class ReplayAction {
  Move, LeftDown, LeftUp, RightDown, RightUp, MiddleDown, MiddleUp, Cancel, WheelUp, WheelDown,
  WheelHalfUp, WheelHalfDown, FocusLost, WaitMenu, WaitBoard, WaitTitle,
  WaitShop, WaitAlmanac, WaitChooser, WaitChallenge, WaitIdle,
  ModePc, ModeConsole, PadPress, PadNeutral, PadDrift,
  Controller, ControllerLeft, ControllerRight, Controller2, ControllerLeft2, ControllerRight2, FixtureLevel, FixtureAward,
  FixtureMode, FixtureHeavySun, FixtureSlotSeed, FixtureGardenCoin, TwistMatch, BeghouledMatch, FixtureVsync, BatchBegin, BatchEnd
};
enum class ReplayGroup { Mouse, Wait, Mode, PadActivity, Controller, Fixture, Batch };
struct ReplayActionInfo { std::string_view name; ReplayAction action; ReplayGroup group; };
inline constexpr ReplayActionInfo kReplayActions[] = {
  {"move", ReplayAction::Move, ReplayGroup::Mouse},
  {"left_down", ReplayAction::LeftDown, ReplayGroup::Mouse},
  {"left_up", ReplayAction::LeftUp, ReplayGroup::Mouse},
  {"right_down", ReplayAction::RightDown, ReplayGroup::Mouse},
  {"right_up", ReplayAction::RightUp, ReplayGroup::Mouse},
  {"middle_down", ReplayAction::MiddleDown, ReplayGroup::Mouse},
  {"middle_up", ReplayAction::MiddleUp, ReplayGroup::Mouse},
  {"cancel", ReplayAction::Cancel, ReplayGroup::Mouse},
  {"wheel_up", ReplayAction::WheelUp, ReplayGroup::Mouse},
  {"wheel_down", ReplayAction::WheelDown, ReplayGroup::Mouse},
  {"wheel_half_up", ReplayAction::WheelHalfUp, ReplayGroup::Mouse},
  {"wheel_half_down", ReplayAction::WheelHalfDown, ReplayGroup::Mouse},
  {"focus_lost", ReplayAction::FocusLost, ReplayGroup::Mouse},
  {"wait_menu", ReplayAction::WaitMenu, ReplayGroup::Wait},
  {"wait_board", ReplayAction::WaitBoard, ReplayGroup::Wait},
  {"wait_title", ReplayAction::WaitTitle, ReplayGroup::Wait},
  {"wait_shop", ReplayAction::WaitShop, ReplayGroup::Wait},
  {"wait_almanac", ReplayAction::WaitAlmanac, ReplayGroup::Wait},
  {"wait_chooser", ReplayAction::WaitChooser, ReplayGroup::Wait},
  {"wait_challenge", ReplayAction::WaitChallenge, ReplayGroup::Wait},
  {"wait_idle", ReplayAction::WaitIdle, ReplayGroup::Wait},
  {"mode_pc", ReplayAction::ModePc, ReplayGroup::Mode},
  {"mode_console", ReplayAction::ModeConsole, ReplayGroup::Mode},
  {"pad_press", ReplayAction::PadPress, ReplayGroup::PadActivity},
  {"pad_neutral", ReplayAction::PadNeutral, ReplayGroup::PadActivity},
  {"pad_drift", ReplayAction::PadDrift, ReplayGroup::PadActivity},
  {"controller", ReplayAction::Controller, ReplayGroup::Controller},
  {"controller_left", ReplayAction::ControllerLeft, ReplayGroup::Controller},
  {"controller_right", ReplayAction::ControllerRight, ReplayGroup::Controller},
  {"controller2", ReplayAction::Controller2, ReplayGroup::Controller},
  {"controller_left2", ReplayAction::ControllerLeft2, ReplayGroup::Controller},
  {"controller_right2", ReplayAction::ControllerRight2, ReplayGroup::Controller},
  {"fixture_level", ReplayAction::FixtureLevel, ReplayGroup::Fixture},
  {"fixture_award", ReplayAction::FixtureAward, ReplayGroup::Fixture},
  {"fixture_mode", ReplayAction::FixtureMode, ReplayGroup::Fixture},
  {"fixture_heavy_sun", ReplayAction::FixtureHeavySun, ReplayGroup::Fixture},
  {"fixture_slot_seed", ReplayAction::FixtureSlotSeed, ReplayGroup::Fixture},
  {"fixture_garden_coin", ReplayAction::FixtureGardenCoin, ReplayGroup::Fixture},
  {"click_twist_match", ReplayAction::TwistMatch, ReplayGroup::Fixture},
  {"swipe_beghouled_match", ReplayAction::BeghouledMatch, ReplayGroup::Fixture},
  {"fixture_vsync", ReplayAction::FixtureVsync, ReplayGroup::Fixture},
  {"batch_begin", ReplayAction::BatchBegin, ReplayGroup::Batch},
  {"batch_end", ReplayAction::BatchEnd, ReplayGroup::Batch}
};
struct ReplayEvent { double seconds = 0; Point point{}; ReplayActionInfo info{}; };
inline bool ParseReplayRow(std::string line, ReplayEvent& event) {
  std::replace(line.begin(), line.end(), ',', ' ');
  std::istringstream row(line);
  std::string name, extra;
  if (!(row >> event.seconds >> event.point.x >> event.point.y >> name) || (row >> extra) ||
      !std::isfinite(event.seconds) || event.seconds < 0 ||
      !std::isfinite(event.point.x) || !std::isfinite(event.point.y)) return false;
  const auto found = std::find_if(std::begin(kReplayActions), std::end(kReplayActions),
      [&](const auto& info) { return info.name == name; });
  if (found == std::end(kReplayActions)) return false;
  event.info = *found;
  auto integer = [](float x, int min, int max) { return x >= min && x <= max && std::trunc(x) == x; };
  using enum ReplayAction;
  switch (event.info.action) {
    case FixtureLevel: return integer(event.point.x, 1, 50);
    case FixtureHeavySun: return integer(event.point.x, 0, 1000000);
    case FixtureSlotSeed: return integer(event.point.x, 0, 48);
    case FixtureVsync: return integer(event.point.x, 0, 1);
    case FixtureMode: return event.point.x == 37 || event.point.x == 51 || event.point.x == 52 ||
        event.point.x == 53 || event.point.x == 90 || event.point.x == 144 || event.point.x == 151 ||
        event.point.x == 19 || event.point.x == 20 || event.point.x == 21 || event.point.x == 23 ||
        event.point.x == 24 || event.point.x == 25 || event.point.x == 31 || event.point.x == 32;
    case FixtureGardenCoin: return MapBoardPointer(event.point).has_value();
    case Controller: case Controller2: return integer(event.point.x, 0, 65535) && integer(event.point.y, 0, 2);
    case ControllerLeft: case ControllerRight: case ControllerLeft2: case ControllerRight2:
      return integer(event.point.x, -32768, 32767) && integer(event.point.y, -32768, 32767);
    default: return true;
  }
}
inline bool ValidateReplay(const std::vector<ReplayEvent>& events) {
  bool batch = false;
  size_t count = 0;
  double previous = 0, batch_time = 0;
  for (const auto& event : events) {
    if (event.seconds < previous) return false;
    previous = event.seconds;
    if (event.info.action == ReplayAction::BatchBegin) {
      if (batch) return false;
      batch = true; count = 0; batch_time = event.seconds;
    } else if (event.info.action == ReplayAction::BatchEnd) {
      if (!batch || !count || event.seconds != batch_time) return false;
      batch = false;
    } else if (batch && (event.info.group != ReplayGroup::Mouse ||
        event.seconds != batch_time || ++count > 256)) return false;
  }
  return !batch;
}
}
