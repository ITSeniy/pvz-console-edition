// pvz - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/filesystem.h>
#include <rex/embedded_metadata.h>
#include "mouse_bridge.h"
#include "input_backend.h"
#include "display_sync.h"
#include "game_settings.h"
#include "pc_menu_dialog.h"
#include <rex/ui/windowed_app_context.h>
#include <atomic>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

std::jthread StartFrameCapture(rex::Runtime* runtime);
std::jthread StartInputReplay();

class PvzApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<PvzApp>(new PvzApp(ctx, "pvz-console-edition",
        PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    config.gpu_plugin = "xenos";
    config.input_factory = CreatePvzInputSystem;
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    std::filesystem::create_directories(paths.user_data_root);
    const auto config = paths.user_data_root / "pvz.toml";
    const auto defaults = rex::filesystem::GetExecutableFolder() / "pc_defaults.toml";
    if (!std::filesystem::exists(config) && std::filesystem::exists(defaults))
      std::filesystem::copy_file(defaults, config);
    paths.config_path = config;
  }

  void OnMouseMove(rex::ui::MouseEvent& event) override { CapturePointer(event); }
  void OnPostSetup() override {
    pvz::SetHostSettingsCallback([this, alive = host_settings_alive_] {
      if (!alive->load()) return;
      app_context().CallInUIThreadDeferred([this, alive] {
        if (alive->load()) ApplyHostSettings();
      });
    });
    pvz::SetHostMenuCallback([this, alive = host_settings_alive_](pvz::HostMenu menu) {
      if (!alive->load()) return;
      app_context().CallInUIThreadDeferred([this, alive, menu] {
        if (!alive->load()) return;
        if (menu == pvz::HostMenu::Quit) { window()->RequestClose(); return; }
        if (!pc_menu_) pc_menu_ = std::make_unique<PcMenuDialog>(imgui_drawer(), immediate_drawer(), runtime(), &achievements());
        pc_menu_->Show(menu);
        REXLOG_INFO("PvZ host menu opened: {}", menu == pvz::HostMenu::Achievements ? "Achievements" : "Cheats");
      });
    });
  }
  void OnMouseDown(rex::ui::MouseEvent& event) override { CaptureButton(event, true); }
  void OnMouseUp(rex::ui::MouseEvent& event) override { CaptureButton(event, false); }
  void OnMouseWheel(rex::ui::MouseEvent& event) override {
    if (auto point = CapturePointer(event)) pvz::PublishWheel(*point, event.scroll_y());
  }
  void OnWindowFocusChanged(bool focused) override {
    if (!focused && rex::cvar::Query<std::string>("pvz_input_replay").empty()) {
      pvz::PublishPointer(std::nullopt);
      pvz::PublishFocusLost();
    }
  }

  void OnPostLaunchModule(rex::system::XThread*) override {
    frame_capture_ = StartFrameCapture(runtime());
    input_replay_ = StartInputReplay();
  }
  void OnPreLaunchModule() override {
    pvz::ConfigureGameSettings(runtime()->user_data_root() / "pvz.toml", runtime()->game_data_root());
  }
  void OnShutdown() override {
    host_settings_alive_->store(false);
    pvz::SetHostSettingsCallback({});
    pvz::SetHostMenuCallback({});
    pc_menu_.reset();
#ifdef _WIN32
    if (!display_device_.empty()) ChangeDisplaySettingsExW(display_device_.c_str(), nullptr, nullptr, 0, nullptr);
#endif
    input_replay_.request_stop();
    if (input_replay_.joinable()) input_replay_.join();
    frame_capture_.request_stop();
    if (frame_capture_.joinable()) frame_capture_.join();
    pvz::PublishPointer(std::nullopt);
    pvz::RemoveDisplaySync();
  }

 private:
  std::shared_ptr<std::atomic_bool> host_settings_alive_ = std::make_shared<std::atomic_bool>(true);
  std::string applied_display_;
  std::wstring display_device_;
  std::optional<bool> applied_vsync_;
  bool icon_applied_ = false;
  std::unique_ptr<PcMenuDialog> pc_menu_;
  void ApplyHostSettings() {
    if (!window()) return;
    if (!icon_applied_) {
      if (const auto icon = rex::FindEmbeddedMetadataAsset("icons/app.ico"))
        window()->SetIcon(icon->bytes.data(), icon->bytes.size());
      icon_applied_ = true;
    }
    window()->SetCursorVisibility((pvz::HostMenuOpen() || pvz::HostCursorVisible()) ? rex::ui::Window::CursorVisibility::kVisible :
        rex::ui::Window::CursorVisibility::kHidden);
    const bool vsync = rex::cvar::Query<bool>("pvz_vsync");
    if (applied_vsync_ != vsync) {
      // Recreate the swap chain with the matching tearing capability. The
      // display-sync binding also supplies the actual DXGI Present interval.
      rex::cvar::SetFlagByName("d3d12_allow_variable_refresh_rate_and_tearing", vsync ? "false" : "true");
      // Keep the same presentation path for both settings. The native swap
      // hook supplies the 60 FPS clock; SDK guest VSync waits are disabled.
      rex::cvar::SetFlagByName("host_present_from_non_ui_thread", "true");
      auto* graphics = runtime()->graphics_system();
      const bool installed = graphics && pvz::InstallDisplaySync(graphics->provider(), window()->GetNativeWindowHandle(), vsync);
      if (auto* presenter = graphics ? graphics->presenter() : nullptr) {
        window()->SetPresenter(nullptr);
        window()->SetPresenter(presenter);
      }
      if (!installed) REXLOG_WARN("PvZ DXGI synchronization binding unavailable for this presenter");
      applied_vsync_ = vsync;
      REXLOG_INFO("PvZ display synchronization: {} (DXGI tearing {}, presentation thread guest output, {} FPS cap)",
          vsync, !vsync, rex::cvar::Query<int32_t>("pvz_frame_limit"));
    }
    const auto mode = rex::cvar::Query<std::string>("pvz_display_mode");
    if (applied_display_ == mode) return;
#ifdef _WIN32
    if (!display_device_.empty()) {
      ChangeDisplaySettingsExW(display_device_.c_str(), nullptr, nullptr, 0, nullptr);
      display_device_.clear();
    }
    if (mode == "fullscreen") {
      const auto handle = static_cast<HWND>(window()->GetNativeWindowHandle());
      MONITORINFOEXW monitor{}; monitor.cbSize = sizeof(monitor);
      if (handle && GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor)) {
        DEVMODEW display{}; display.dmSize = sizeof(display);
        display.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
        display.dmPelsWidth = rex::cvar::Query<int32_t>("window_width");
        display.dmPelsHeight = rex::cvar::Query<int32_t>("window_height");
        if (!display.dmPelsWidth || !display.dmPelsHeight) {
          display.dmPelsWidth = window()->GetActualPhysicalWidth();
          display.dmPelsHeight = window()->GetActualPhysicalHeight();
        }
        // A 60 Hz exclusive mode avoids the unavoidable 60-on-75 cadence.
        // Use only an advertised mode; otherwise retain the monitor rate.
        DEVMODEW candidate{}; candidate.dmSize = sizeof(candidate);
        for (DWORD index = 0; EnumDisplaySettingsW(monitor.szDevice, index, &candidate); ++index)
          if (candidate.dmPelsWidth == display.dmPelsWidth && candidate.dmPelsHeight == display.dmPelsHeight &&
              candidate.dmDisplayFrequency == 60) {
            display.dmFields |= DM_DISPLAYFREQUENCY; display.dmDisplayFrequency = 60; break;
          }
        if (ChangeDisplaySettingsExW(monitor.szDevice, &display, nullptr, CDS_FULLSCREEN, nullptr) == DISP_CHANGE_SUCCESSFUL)
          display_device_ = monitor.szDevice;
      }
    }
#endif
    window()->SetFullscreen(mode != "windowed");
    applied_display_ = mode;
  }
  std::jthread frame_capture_;
  std::jthread input_replay_;
  std::optional<pvz::Point> CapturePointer(const rex::ui::MouseEvent& event) {
    // A replay owns the mouse snapshot, so physical clicks cannot alter the
    // diagnostic scenario while it runs. Keyboard/gamepad still work.
    if (!rex::cvar::Query<std::string>("pvz_input_replay").empty()) return std::nullopt;
    if (!window()->HasFocus() || (imgui_drawer() && imgui_drawer()->GetIO().WantCaptureMouse)) {
      pvz::PublishPointer(std::nullopt);
      return std::nullopt;
    }
    auto point = pvz::MapPointer(float(event.x()), float(event.y()),
        float(window()->GetActualPhysicalWidth()), float(window()->GetActualPhysicalHeight()),
        rex::cvar::Query<bool>("present_letterbox"),
        float(rex::cvar::Query<double>("pvz_mouse_canvas_width")),
        float(rex::cvar::Query<double>("pvz_mouse_canvas_height")));
    pvz::PublishPointer(point);
    return point;
  }
  void CaptureButton(rex::ui::MouseEvent& event, bool down) {
    auto point = CapturePointer(event);
    if (!point || !rex::cvar::Query<bool>("pvz_mouse_cursor")) return;
    int click = 0;
    if (event.button() == rex::ui::MouseEvent::Button::kLeft) click = 1;
    if (event.button() == rex::ui::MouseEvent::Button::kRight) click = -1;
    if (event.button() == rex::ui::MouseEvent::Button::kMiddle) click = 2;
    if (click) pvz::PublishButton(*point, click, down);
  }

};
