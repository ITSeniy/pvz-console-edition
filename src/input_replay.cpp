#include "bridge_mnk_driver.h"
#include "mouse_bridge.h"
#include "replay_fixtures.h"
#include "input_activity.h"
#include "input_backend.h"
#include "game_settings.h"
#include "replay_event.h"
#include "native/replacement.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <fstream>
#include <thread>
#include <atomic>
#include <cmath>
REXCVAR_DECLARE(bool, pvz_replay_frame_locked);
REXCVAR_DECLARE(bool, pvz_replay_record_steps);
namespace pvz { extern std::atomic<uint64_t> replay_step; }

REXCVAR_DEFINE_STRING(pvz_input_replay, "", "Diagnostics",
    "Replay CSV: seconds,x,y,action; supports atomic mouse batches and screen waits");

namespace {
using pvz::ReplayAction;
bool Ready(ReplayAction action) {
  using enum ReplayAction;
  switch (action) {
    case WaitTitle: return pvz::ReplayTitleReady();
    case WaitMenu: return pvz::ReplaySceneReady(false);
    case WaitBoard: return pvz::ReplaySceneReady(true);
    case WaitShop: return pvz::ReplayScreenReady(0x820CB23C);
    case WaitAlmanac: return pvz::ReplayScreenReady(0x820B466C);
    case WaitChooser: return pvz::ReplayScreenReady(0x820CA94C);
    case WaitChallenge: return pvz::ReplayScreenReady(0x820B899C);
    case WaitIdle: return pvz::ReplayInputIdle();
    default: return false;
  }
}
pvz::MouseInputEvent MouseEvent(const pvz::ReplayEvent& event) {
  using enum ReplayAction;
  pvz::MouseInputEvent input{pvz::MouseEventKind::Move, event.point};
  switch (event.info.action) {
    case LeftDown: case LeftUp: case RightDown: case RightUp: case MiddleDown: case MiddleUp:
      input.kind = pvz::MouseEventKind::Button;
      input.click = event.info.action == MiddleDown || event.info.action == MiddleUp ? 2 :
          event.info.action == LeftDown || event.info.action == LeftUp ? 1 : -1;
      input.down = event.info.action == LeftDown || event.info.action == RightDown || event.info.action == MiddleDown;
      break;
    case WheelUp: case WheelDown: case WheelHalfUp: case WheelHalfDown:
      input.kind = pvz::MouseEventKind::Wheel;
      input.delta = event.info.action == WheelUp ? 120 : event.info.action == WheelDown ? -120 :
          event.info.action == WheelHalfUp ? 60 : -60;
      break;
    case Cancel: case FocusLost:
      input.kind = pvz::MouseEventKind::Cancel; input.point.reset();
      input.reason = event.info.action == FocusLost ? pvz::MouseCancelReason::FocusLost : pvz::MouseCancelReason::PointerLeft;
      break;
    default: break;
  }
  return input;
}
void PublishMouse(const pvz::MouseInputEvent& input) {
  switch (input.kind) {
    case pvz::MouseEventKind::Move: pvz::PublishPointer(input.point); break;
    case pvz::MouseEventKind::Button: pvz::PublishButton(*input.point, input.click, input.down); break;
    case pvz::MouseEventKind::Wheel: pvz::PublishWheel(*input.point, input.delta); break;
    case pvz::MouseEventKind::Cancel:
      if (input.reason == pvz::MouseCancelReason::FocusLost) pvz::PublishFocusLost();
      else pvz::PublishPointer(std::nullopt);
      break;
    case pvz::MouseEventKind::Key: pvz::PublishKeyAction(input.delta, input.down); break;
  }
}
}

std::jthread StartInputReplay() {
  const auto path = REXCVAR_GET(pvz_input_replay);
  if (path.empty() || REXCVAR_GET(pvz_replay_frame_locked)) return {};
  return std::jthread([path](std::stop_token stop) {
    std::vector<pvz::ReplayEvent> events;
    std::ifstream stream(path);
    if (!stream) { REXLOG_ERROR("Cannot open input replay {}", path); return; }
    std::string line;
    while (std::getline(stream, line)) {
      const auto first = line.find_first_not_of(" \t\r");
      if (first == std::string::npos || line[first] == '#') continue;
      pvz::ReplayEvent event;
      if (!pvz::ParseReplayRow(line, event)) {
        REXLOG_ERROR("Invalid input replay row: {}", line); return;
      }
      events.push_back(event);
    }
    if (!pvz::ValidateReplay(events)) { REXLOG_ERROR("Invalid input replay order or batch: {}", path); return; }
    auto start = std::chrono::steady_clock::now();
    pvz::PadActivity pad_activity;
    std::vector<pvz::MouseInputEvent> batch;
    bool batching = false;
    size_t row_index = 0;
    struct RecordDelivery {
      size_t row;
      ~RecordDelivery() {
        if (REXCVAR_GET(pvz_replay_record_steps))
          REXLOG_INFO("PvZ replay recorded row {} step {}", row, pvz::replay_step.load());
      }
    };
    for (const auto& event : events) {
      while (!stop.stop_requested() &&
          std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < event.seconds)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      if (stop.stop_requested()) return;
      const RecordDelivery record{row_index++};
      const auto action = event.info.action;
      using enum ReplayAction;
      if (event.info.group == pvz::ReplayGroup::Batch) {
        if (action == BatchBegin) { batching = true; batch.clear(); }
        else { pvz::PublishReplayMouseBatch(batch); batching = false;
          REXLOG_INFO("PvZ replay batch published {} events", batch.size()); }
        continue;
      }
      if (event.info.group == pvz::ReplayGroup::Mouse) {
        const auto input = MouseEvent(event);
        if (batching) batch.push_back(input);
        else PublishMouse(input);
        REXLOG_INFO("PvZ replay {} ({}, {})", event.info.name, event.point.x, event.point.y);
        continue;
      }
      if (event.info.group == pvz::ReplayGroup::Controller) {
        pvz::PadSample sample{};
        if (action == Controller || action == Controller2) {
          sample.buttons = uint16_t(event.point.x);
          if (event.point.y == 1) sample.axes[4] = 255;
          if (event.point.y == 2) sample.axes[5] = 255;
          if (event.point.y == 3) pvz::KeyboardStart().store(true);
          if (event.point.y == 4) pvz::ScreenshotRequested().store(true);
        } else {
          const int axis = action == ControllerLeft || action == ControllerLeft2 ? 0 : 2;
          sample.axes[axis] = int(event.point.x); sample.axes[axis + 1] = int(event.point.y);
        }
        const int device = action == Controller2 || action == ControllerLeft2 || action == ControllerRight2 ? 1 : 0;
        pvz::PublishReplayPad(sample, device);
        REXLOG_INFO("PvZ replay {} buttons {} axes {},{},{},{} trigger {}", event.info.name,
            sample.buttons, sample.axes[0], sample.axes[1], sample.axes[2], sample.axes[3], event.point.y);
      } else if (event.info.group == pvz::ReplayGroup::PadActivity) {
        pvz::PadSample sample{};
        if (action == PadPress) sample.buttons = 0x1000;
        if (action == PadDrift) sample.axes[0] = 2000;
        if (pad_activity.Observe(sample)) pvz::PublishControllerActivity();
        REXLOG_INFO("PvZ replay {} (activity only; no guest button injection)", event.info.name);
      } else if (event.info.group == pvz::ReplayGroup::Mode) {
        rex::cvar::SetFlagByName("pvz_cursor_mode", action == ModePc ? "pc" : "console");
        pvz::NotifyMouseModeChanged();
        REXLOG_INFO("PvZ replay {}", event.info.name);
      } else if (event.info.group == pvz::ReplayGroup::Wait) {
        const auto wait_start = std::chrono::steady_clock::now();
        pvz::PublishPointer(event.point);
        while (!stop.stop_requested() && !Ready(action)) {
          if (std::chrono::steady_clock::now() - wait_start > std::chrono::seconds(90)) {
            REXLOG_ERROR("PvZ replay {} timed out", event.info.name); return;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        if (stop.stop_requested()) return;
        start += std::chrono::steady_clock::now() - wait_start;
        REXLOG_INFO("PvZ replay {} ready", event.info.name);
      } else {
        switch (action) {
          case FixtureLevel: pvz::PublishTestAdventureLevel(int(event.point.x)); break;
          case FixtureAward: pvz::PublishTestAward(); break;
          case FixtureMode: pvz::PublishTestMode(int(event.point.x)); break;
          case FixtureHeavySun: pvz::PublishTestHeavyWeaponSun(int(event.point.x)); break;
          case FixtureSlotSeed: pvz::PublishTestSlotSeed(int(event.point.x)); break;
          case FixtureGardenCoin: pvz::PublishTestGardenCoin(event.point); break;
          case FixtureCoin: pvz::PublishTestCoin(int(event.point.x)); break;
          case Capture: pvz::RequestReplayCapture(uint32_t(event.point.x)); break;
          case KeyDown: case KeyUp: {
            const auto bound = pvz::HandleGameKey(rex::ui::VirtualKey(uint16_t(event.point.x)), action == KeyDown);
            REXLOG_INFO("PvZ replay {} {} action {}", event.info.name, event.point.x, bound ? int(*bound) : -1);
            break;
          }
          case TwistMatch: pvz::PublishTestTwistClick(); break;
          case BeghouledMatch: pvz::PublishTestBeghouledSwipe(); break;
          case FixtureVsync:
            rex::cvar::SetFlagByName("pvz_vsync", event.point.x ? "true" : "false");
            pvz::RefreshHostSettings();
            REXLOG_INFO("PvZ replay VSync {}", event.point.x != 0); break;
          default: break;
        }
      }
    }
    REXLOG_INFO("PvZ replay completed {}", path);
    pvz::native::ReplayCompleted();
  });
}

namespace pvz {
void ReplayLogicalStep(uint64_t step) {
  static bool loaded = false, failed = false, complete = false;
  static std::vector<ReplayEvent> events;
  static size_t index = 0;
  static PadActivity pad_activity;
  static std::vector<MouseInputEvent> batch;
  static bool batching = false;
  if (failed || complete) return;
  if (!loaded) {
    loaded = true;
    std::ifstream input(REXCVAR_GET(pvz_input_replay));
    std::string line;
    if (!std::getline(input, line) || line != "# pvz-frame-locked-v1") {
      REXLOG_ERROR("PvZ frame replay requires converter header"); failed = true; return;
    }
    while (std::getline(input, line)) {
      const auto first = line.find_first_not_of(" \t\r");
      if (first == std::string::npos || line[first] == '#') continue;
      ReplayEvent event;
      if (!ParseReplayRow(line, event) || event.seconds != std::trunc(event.seconds) || event.seconds > 9007199254740991.0) {
        REXLOG_ERROR("Invalid frame replay row: {}", line); failed = true; return;
      }
      events.push_back(event);
    }
    if (!ValidateReplay(events)) { REXLOG_ERROR("Invalid frame replay order or batch"); failed = true; return; }
  }
  auto dispatch = [&](const ReplayEvent& event) {
      const auto action = event.info.action;
      using enum ReplayAction;
      if (event.info.group == pvz::ReplayGroup::Batch) {
        if (action == BatchBegin) { batching = true; batch.clear(); }
        else { pvz::PublishReplayMouseBatch(batch); batching = false;
          REXLOG_INFO("PvZ replay batch published {} events", batch.size()); }
        return;
      }
      if (event.info.group == pvz::ReplayGroup::Mouse) {
        const auto input = MouseEvent(event);
        if (batching) batch.push_back(input);
        else PublishMouse(input);
        REXLOG_INFO("PvZ replay {} ({}, {})", event.info.name, event.point.x, event.point.y);
        return;
      }
      if (event.info.group == pvz::ReplayGroup::Controller) {
        pvz::PadSample sample{};
        if (action == Controller || action == Controller2) {
          sample.buttons = uint16_t(event.point.x);
          if (event.point.y == 1) sample.axes[4] = 255;
          if (event.point.y == 2) sample.axes[5] = 255;
          if (event.point.y == 3) pvz::KeyboardStart().store(true);
          if (event.point.y == 4) pvz::ScreenshotRequested().store(true);
        } else {
          const int axis = action == ControllerLeft || action == ControllerLeft2 ? 0 : 2;
          sample.axes[axis] = int(event.point.x); sample.axes[axis + 1] = int(event.point.y);
        }
        const int device = action == Controller2 || action == ControllerLeft2 || action == ControllerRight2 ? 1 : 0;
        pvz::PublishReplayPad(sample, device);
        REXLOG_INFO("PvZ replay {} buttons {} axes {},{},{},{} trigger {}", event.info.name,
            sample.buttons, sample.axes[0], sample.axes[1], sample.axes[2], sample.axes[3], event.point.y);
      } else if (event.info.group == pvz::ReplayGroup::PadActivity) {
        pvz::PadSample sample{};
        if (action == PadPress) sample.buttons = 0x1000;
        if (action == PadDrift) sample.axes[0] = 2000;
        if (pad_activity.Observe(sample)) pvz::PublishControllerActivity();
        REXLOG_INFO("PvZ replay {} (activity only; no guest button injection)", event.info.name);
      } else if (event.info.group == pvz::ReplayGroup::Mode) {
        rex::cvar::SetFlagByName("pvz_cursor_mode", action == ModePc ? "pc" : "console");
        pvz::NotifyMouseModeChanged();
        REXLOG_INFO("PvZ replay {}", event.info.name);
      } else if (event.info.group == pvz::ReplayGroup::Wait) {
        pvz::PublishPointer(event.point);
        if (!Ready(action)) {
          REXLOG_ERROR("PvZ frame replay readiness failed at step {} action {}", step, event.info.name);
          failed = true;
        }
      } else {
        switch (action) {
          case FixtureLevel: pvz::PublishTestAdventureLevel(int(event.point.x)); break;
          case FixtureAward: pvz::PublishTestAward(); break;
          case FixtureMode: pvz::PublishTestMode(int(event.point.x)); break;
          case FixtureHeavySun: pvz::PublishTestHeavyWeaponSun(int(event.point.x)); break;
          case FixtureSlotSeed: pvz::PublishTestSlotSeed(int(event.point.x)); break;
          case FixtureGardenCoin: pvz::PublishTestGardenCoin(event.point); break;
          case FixtureCoin: pvz::PublishTestCoin(int(event.point.x)); break;
          case Capture: pvz::RequestReplayCapture(uint32_t(event.point.x)); break;
          case KeyDown: case KeyUp: {
            const auto bound = pvz::HandleGameKey(rex::ui::VirtualKey(uint16_t(event.point.x)), action == KeyDown);
            REXLOG_INFO("PvZ replay {} {} action {}", event.info.name, event.point.x, bound ? int(*bound) : -1);
            break;
          }
          case TwistMatch: pvz::PublishTestTwistClick(); break;
          case BeghouledMatch: pvz::PublishTestBeghouledSwipe(); break;
          case FixtureVsync:
            rex::cvar::SetFlagByName("pvz_vsync", event.point.x ? "true" : "false");
            pvz::RefreshHostSettings();
            REXLOG_INFO("PvZ replay VSync {}", event.point.x != 0); break;
          default: break;
        }
      }
  };
  while (!failed && index < events.size() && uint64_t(events[index].seconds) <= step) {
    const auto& event = events[index];
    if (uint64_t(event.seconds) != step) {
      REXLOG_ERROR("PvZ frame replay missed row {} wanted {} actual {}", index, event.seconds, step);
      failed = true; return;
    }
    dispatch(event);
    REXLOG_INFO("PvZ frame replay delivered row {} step {}", index, step);
    ++index;
  }
  if (!failed && index == events.size()) {
    complete = true; REXLOG_INFO("PvZ frame replay completed {}", REXCVAR_GET(pvz_input_replay));
    pvz::native::ReplayCompleted();
  }
}
}
