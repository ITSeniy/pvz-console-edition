#include "input_activity.h"
#include <iostream>

int main() {
  pvz::PadActivity activity;
  auto check = [](bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
  };
  pvz::PadSample s{};
  check(!activity.Observe(s), "idle pad steals mouse");
  s.axes[0] = 7800; s.axes[4] = 30;
  check(!activity.Observe(s), "drift steals mouse");
  s.buttons = 0x1000;
  check(activity.Observe(s), "press not detected");
  check(!activity.Observe(s), "held button steals mouse back");
  s.buttons = 0;
  check(!activity.Observe(s), "button release takes ownership");
  s.axes[0] = -16000;
  check(activity.Observe(s), "negative stick motion not detected");
  s.axes[0] = -16100;
  check(!activity.Observe(s), "analog jitter takes ownership");
  s.axes[0] = -18000;
  check(activity.Observe(s), "deliberate stick motion not detected");
  s.axes[0] = 0;
  check(!activity.Observe(s), "stick release takes ownership");
  s.axes[0] = -16000;
  check(activity.Observe(s), "second stick motion not detected");
  s.axes[4] = 80;
  check(activity.Observe(s), "trigger press not detected");
  check(!activity.Observe(s), "held trigger takes ownership");
  s.axes[4] = 0;
  check(!activity.Observe(s), "trigger release takes ownership");
  s.buttons = 0x2000;
  check(activity.Observe(s), "second button not detected");
  std::cout << "input activity checks passed\n";
}
