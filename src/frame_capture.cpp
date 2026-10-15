#include <rex/cvar.h>
#include <rex/runtime.h>
#include <rex/ui/presenter.h>
#include <rex/logging.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <vector>
#include "replay_fixtures.h"
#include "bridge_mnk_driver.h"
#ifdef _WIN32
#include <wincodec.h>
#include <wrl/client.h>
#endif

REXCVAR_DEFINE_STRING(pvz_capture_frame, "", "Diagnostics", "Save guest frame(s) as PPM after startup");
REXCVAR_DEFINE_UINT32(pvz_capture_delay, 8, "Diagnostics", "Seconds before guest frame capture");
REXCVAR_DEFINE_UINT32(pvz_capture_count, 1, "Diagnostics", "Number of guest frames to capture (1-16)");
REXCVAR_DEFINE_UINT32(pvz_capture_interval, 4, "Diagnostics", "Seconds between diagnostic frame captures");
REXCVAR_DEFINE_BOOL(pvz_screenshots, true, "PvZ", "F12 saves the game picture as a PNG in the screenshots folder");

namespace {
// Frames requested by the input replay's capture action, taken ~50 ms apart.
std::atomic<uint32_t> replay_frames{0};

// Lossless PNG through the Windows Imaging Component: the game picture only,
// at its render resolution (see resolution_scale), without the system cursor.
bool SavePng(const rex::ui::RawImage& frame, const std::filesystem::path& target) {
#ifdef _WIN32
  using Microsoft::WRL::ComPtr;
  const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool saved = false;
  {
    // The PNG encoder takes BGR; the captured frame is RGBA.
    std::vector<uint8_t> rgb(size_t(frame.width) * frame.height * 3);
    for (uint32_t y = 0; y < frame.height; ++y)
      for (uint32_t x = 0; x < frame.width; ++x) {
        const auto* in = frame.data.data() + y * frame.stride + x * 4;
        auto* out = rgb.data() + (size_t(y) * frame.width + x) * 3;
        out[0] = in[2]; out[1] = in[1]; out[2] = in[0];
      }
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> image;
    WICPixelFormatGUID format = GUID_WICPixelFormat24bppBGR;
    saved = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
        SUCCEEDED(factory->CreateStream(&stream)) &&
        SUCCEEDED(stream->InitializeFromFilename(target.wstring().c_str(), GENERIC_WRITE)) &&
        SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
        SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
        SUCCEEDED(encoder->CreateNewFrame(&image, nullptr)) &&
        SUCCEEDED(image->Initialize(nullptr)) &&
        SUCCEEDED(image->SetSize(frame.width, frame.height)) &&
        SUCCEEDED(image->SetPixelFormat(&format)) && format == GUID_WICPixelFormat24bppBGR &&
        SUCCEEDED(image->WritePixels(frame.height, frame.width * 3, UINT(rgb.size()), rgb.data())) &&
        SUCCEEDED(image->Commit()) && SUCCEEDED(encoder->Commit());
  }
  if (SUCCEEDED(init)) CoUninitialize();
  return saved;
#else
  (void)frame; (void)target;
  return false;
#endif
}

std::filesystem::path ScreenshotPath() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  const int ms = int(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &time);
#else
  localtime_r(&time, &local);
#endif
  char name[64];
  std::snprintf(name, sizeof(name), "pvz-%04d%02d%02d-%02d%02d%02d-%03d.png", local.tm_year + 1900,
      local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, ms);
  std::error_code error;
  std::filesystem::create_directories("screenshots", error);
  return std::filesystem::absolute(std::filesystem::path("screenshots") / name, error);
}
}
void pvz::RequestReplayCapture(uint32_t frames) { replay_frames.fetch_add(std::min(frames, 32u)); }

std::jthread StartFrameCapture(rex::Runtime* runtime) {
  const auto path = REXCVAR_GET(pvz_capture_frame);
  const bool screenshots = REXCVAR_GET(pvz_screenshots);
  const auto delay = std::min(REXCVAR_GET(pvz_capture_delay), uint32_t(180));
  const auto count = std::clamp(REXCVAR_GET(pvz_capture_count), uint32_t(1), uint32_t(16));
  const auto interval = std::clamp(REXCVAR_GET(pvz_capture_interval), uint32_t(1), uint32_t(180));
  if (path.empty() && !screenshots) return {};
  return std::jthread([runtime, path, screenshots, delay, count, interval](std::stop_token stop) {
    auto grab = [&](rex::ui::RawImage& frame) {
      auto* graphics = runtime->graphics_system();
      auto* presenter = graphics ? graphics->presenter() : nullptr;
      if (!presenter || !presenter->CaptureGuestOutput(frame) || !frame.width || !frame.height) {
        REXLOG_WARN("Guest frame capture unavailable");
        return false;
      }
      return true;
    };
    auto save = [&](const std::string& suffix) {
      rex::ui::RawImage frame;
      if (!grab(frame)) return;
      auto target = std::filesystem::path(path);
      if (!suffix.empty()) target.replace_filename(target.stem().string() + suffix + target.extension().string());
      std::ofstream out(target, std::ios::binary);
      out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
      for (uint32_t y = 0; y < frame.height; ++y)
        for (uint32_t x = 0; x < frame.width; ++x)
          out.write(reinterpret_cast<const char*>(frame.data.data() + y * frame.stride + x * 4), 3);
      if (out) REXLOG_INFO("Captured guest frame {}x{} to {}", frame.width, frame.height, target.string());
    };
    auto screenshot = [&] {
      rex::ui::RawImage frame;
      if (!grab(frame)) return;
      const auto target = ScreenshotPath();
      if (SavePng(frame, target)) REXLOG_INFO("PvZ screenshot {}x{} saved to {}", frame.width, frame.height, target.string());
      else REXLOG_WARN("PvZ screenshot could not be saved to {}", target.string());
    };
    using Clock = std::chrono::steady_clock;
    auto next = Clock::now() + std::chrono::seconds(delay);
    uint32_t timed = 0, replayed = 0;
    while (!stop.stop_requested()) {
      if (screenshots && pvz::ScreenshotRequested().exchange(false)) screenshot();
      if (!path.empty() && timed < count && Clock::now() >= next) {
        save(count > 1 ? "-" + std::to_string(++timed) : (++timed, std::string()));
        next = Clock::now() + std::chrono::seconds(interval);
      }
      if (!path.empty() && replay_frames.load() && replay_frames.fetch_sub(1)) {
        save("-r" + std::to_string(++replayed));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  });
}
