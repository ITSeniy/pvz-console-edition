#include <rex/cvar.h>
#include <rex/runtime.h>
#include <rex/ui/presenter.h>
#include <rex/logging.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <algorithm>
#include "replay_fixtures.h"

REXCVAR_DEFINE_STRING(pvz_capture_frame, "", "Diagnostics", "Save guest frame(s) as PPM after startup");
REXCVAR_DEFINE_UINT32(pvz_capture_delay, 8, "Diagnostics", "Seconds before guest frame capture");
REXCVAR_DEFINE_UINT32(pvz_capture_count, 1, "Diagnostics", "Number of guest frames to capture (1-16)");
REXCVAR_DEFINE_UINT32(pvz_capture_interval, 4, "Diagnostics", "Seconds between diagnostic frame captures");

namespace {
// Frames requested by the input replay's capture action, taken ~50 ms apart.
std::atomic<uint32_t> replay_frames{0};
}
void pvz::RequestReplayCapture(uint32_t frames) { replay_frames.fetch_add(std::min(frames, 32u)); }

std::jthread StartFrameCapture(rex::Runtime* runtime) {
  const auto path = REXCVAR_GET(pvz_capture_frame);
  const auto delay = std::min(REXCVAR_GET(pvz_capture_delay), uint32_t(180));
  const auto count = std::clamp(REXCVAR_GET(pvz_capture_count), uint32_t(1), uint32_t(16));
  const auto interval = std::clamp(REXCVAR_GET(pvz_capture_interval), uint32_t(1), uint32_t(180));
  if (path.empty()) return {};
  return std::jthread([runtime, path, delay, count, interval](std::stop_token stop) {
    auto save = [&](const std::string& suffix) {
      rex::ui::RawImage frame;
      auto* graphics = runtime->graphics_system();
      auto* presenter = graphics ? graphics->presenter() : nullptr;
      if (!presenter || !presenter->CaptureGuestOutput(frame) || !frame.width || !frame.height) {
        REXLOG_WARN("Guest frame capture unavailable");
        return;
      }
      auto target = std::filesystem::path(path);
      if (!suffix.empty()) target.replace_filename(target.stem().string() + suffix + target.extension().string());
      std::ofstream out(target, std::ios::binary);
      out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
      for (uint32_t y = 0; y < frame.height; ++y)
        for (uint32_t x = 0; x < frame.width; ++x)
          out.write(reinterpret_cast<const char*>(frame.data.data() + y * frame.stride + x * 4), 3);
      if (out) REXLOG_INFO("Captured guest frame {}x{} to {}", frame.width, frame.height, target.string());
    };
    using Clock = std::chrono::steady_clock;
    auto next = Clock::now() + std::chrono::seconds(delay);
    uint32_t timed = 0, replayed = 0;
    while (!stop.stop_requested()) {
      if (timed < count && Clock::now() >= next) {
        save(count > 1 ? "-" + std::to_string(++timed) : (++timed, std::string()));
        next = Clock::now() + std::chrono::seconds(interval);
      }
      if (replay_frames.load() && replay_frames.fetch_sub(1)) {
        save("-r" + std::to_string(++replayed));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  });
}
