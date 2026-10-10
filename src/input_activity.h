#pragma once
#include <array>
#include <cstdint>
#include <cstdlib>

namespace pvz {
struct PadSample {
  uint16_t buttons = 0;
  std::array<int, 6> axes{}; // LX, LY, RX, RY, LT, RT
};

// Observe before keyboard and mouse emulation are merged into the pad state.
// A held button, neutral release or small analog noise is not a new action.
class PadActivity {
 public:
  bool Observe(PadSample sample) {
    bool active = (sample.buttons & ~previous_.buttons) != 0;
    for (size_t i = 0; i < sample.axes.size(); ++i) {
      const int threshold = i < 4 ? 7849 : 30;
      if (std::abs(sample.axes[i]) <= threshold) sample.axes[i] = 0;
      const int delta = i < 4 ? 1200 : 5;
      if (sample.axes[i] && (!previous_.axes[i] ||
          std::abs(sample.axes[i] - previous_.axes[i]) >= delta)) active = true;
    }
    if (active || sample.buttons != previous_.buttons) previous_ = sample;
    else {
      // Neutral values must reset the baseline for the next deliberate motion.
      for (size_t i = 0; i < sample.axes.size(); ++i)
        if (!sample.axes[i]) previous_.axes[i] = 0;
    }
    return active;
  }
 private:
  PadSample previous_;
};
}
