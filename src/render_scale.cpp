#include "render_scale.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <algorithm>
#include <cmath>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

REXCVAR_DEFINE_STRING(pvz_render_scale, "auto", "PvZ",
    "Render resolution: auto, or 1-4 times 1280x720 (restart required)")
    .allowed({"auto", "1", "2", "3", "4"});

namespace {
int running_scale = 1;
std::string running_setting = "auto";

#ifdef _WIN32
// The current mode's height of the display the window opens on (the SDK's
// monitor cvar: 0 or 1 the primary display, n the n-th). Display modes are
// physical pixels whatever the DPI awareness.
int MonitorHeight() {
  struct Search { int wanted, index = 0; HMONITOR found = nullptr; } search{
      std::max(rex::cvar::Query<int32_t>("monitor"), 1)};
  EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
    auto& s = *reinterpret_cast<Search*>(data);
    if (++s.index != s.wanted) return TRUE;
    s.found = monitor;
    return FALSE;
  }, reinterpret_cast<LPARAM>(&search));
  const auto monitor = search.found && search.wanted > 1 ? search.found :
      MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFOEXW info{}; info.cbSize = sizeof(info);
  DEVMODEW mode{}; mode.dmSize = sizeof(mode);
  if (!GetMonitorInfoW(monitor, &info) || !EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode))
    return 720;
  return int(mode.dmPelsHeight);
}
#else
int MonitorHeight() { return 720; }
#endif
}

int pvz::ResolveRenderScale(std::string_view setting, int output_height) {
  if (setting != "auto") {
    const int scale = setting.size() == 1 ? setting[0] - '0' : 1;
    return std::clamp(scale, 1, 4);
  }
  // A few lines over 720 (1366x768) stay at 1x; from 900 lines up the next
  // whole multiple renders, scaled down to the output.
  const double ratio = double(std::max(output_height, 720)) / 720.0;
  return std::clamp(int(std::ceil(ratio - 0.1)), 1, 4);
}

void pvz::ApplyRenderScale() {
  const auto setting = REXCVAR_GET(pvz_render_scale);
  // Fullscreen switches the display to window_width x window_height; windowed
  // and borderless show the frame on the display at its own mode (a window
  // can be maximised).
  const auto display = rex::cvar::Query<std::string>("pvz_display_mode");
  const int window_height = rex::cvar::Query<int32_t>("window_height");
  const int height = display == "fullscreen" ? (window_height ? window_height : 720) : MonitorHeight();
  running_scale = ResolveRenderScale(setting, height);
  running_setting = setting;
  const bool applied = rex::cvar::SetFlagByName("resolution_scale", std::to_string(running_scale));
  REXLOG_INFO("PvZ render scale: {} for {} lines ({}x{}, applied {}, now {})", setting, height, 1280 * running_scale,
      720 * running_scale, applied, rex::cvar::Query<int32_t>("resolution_scale"));
}

int pvz::RunningRenderScale() { return running_scale; }
std::string pvz::RunningRenderScaleSetting() { return running_setting; }
