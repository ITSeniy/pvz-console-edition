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
#include "hitch_profiler.h"
#include "game_settings.h"
#include "render_scale.h"
#include "pc_menu_dialog.h"
#include "achievement_toast.h"
#include "import_dialog.h"
#include "game_edition.h"
#include "pc_menus.h"
#include <rex/ui/windowed_app_context.h>
#include <atomic>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>
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

  std::unique_ptr<rex::ui::AchievementNotificationDialog> CreateAchievementNotificationDialog() override {
    if (!imgui_drawer() || !immediate_drawer() || !runtime()) return nullptr;
    return std::make_unique<AchievementToast>(app_context(), imgui_drawer(), immediate_drawer(), runtime());
  }
  // First launch without game data: import the player's own package before
  // the runtime loads default.xex. Existing data continues immediately.
  std::optional<rex::PathConfig> OnFinalizePaths(const rex::PathConfig& defaults,
      std::function<void(rex::PathConfig)> resume) override {
    // The GPU plugin (and its resolution_scale) is loaded now; the GPU reads
    // the scale when the runtime is constructed after this.
    pvz::ApplyRenderScale();
    if (std::filesystem::exists(defaults.game_data_root / "default.xex") || !imgui_drawer()) return defaults;
    REXLOG_INFO("PvZ first-run import: no game files in {}", defaults.game_data_root.string());
    import_dialog_ = std::make_unique<ImportDialog>(imgui_drawer(), app_context(), defaults.game_data_root,
        [this] { return ChooseGamePackageFile(); },
        [this, defaults, resume = std::move(resume)] {
          import_dialog_->Hide();
          import_dialog_.reset();
          resume(defaults);
        },
        [this] { if (window()) window()->RequestClose(); },
        rex::cvar::Query<std::string>("pvz_import_answer"));
    return std::nullopt;
  }
  void OnMouseMove(rex::ui::MouseEvent& event) override { CapturePointer(event); }
  void OnPostSetup() override {
    // The SDK titles the window with the internal app name and its build stamp.
    if (window()) window()->SetTitle("Plants vs. Zombies - Console Edition");
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
        if (menu == pvz::HostMenu::Unlock) { ChooseGamePackage(); return; }
        if (!pc_menu_) pc_menu_ = std::make_unique<PcMenuDialog>(imgui_drawer(), immediate_drawer(), runtime(), &achievements());
        pc_menu_->Show(menu);
        REXLOG_INFO("PvZ host menu opened: {}", menu == pvz::HostMenu::Achievements ? "Achievements" :
            menu == pvz::HostMenu::Keyboard ? "Keyboard" : "Cheats");
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

  void OnLoadXexImage(std::string&) override {
    pvz::ApplyGameLanguage(runtime()->game_data_root());
  }
  void OnPostLaunchModule(rex::system::XThread*) override {
    frame_capture_ = StartFrameCapture(runtime());
    input_replay_ = StartInputReplay();
  }
  void OnPreLaunchModule() override {
    pvz::ConfigureGameSettings(runtime()->user_data_root() / "pvz.toml", runtime()->game_data_root());
  }
  void OnShutdown() override {
    pvz::StopHitchProfiler();
    host_settings_alive_->store(false);
    pvz::SetHostSettingsCallback({});
    pvz::SetHostMenuCallback({});
    pc_menu_.reset();
    if (import_dialog_) { import_dialog_->Hide(); import_dialog_.reset(); }
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
  std::unique_ptr<ImportDialog> import_dialog_;
  // XBLA package file chosen by the player (or pvz_unlock_package in replays).
  std::filesystem::path ChooseGamePackageFile() {
    std::filesystem::path package = rex::cvar::Query<std::string>("pvz_unlock_package");
#ifdef _WIN32
    if (package.empty() && window()) {
      std::array<wchar_t, 4096> file{};
      OPENFILENAMEW dialog{};
      dialog.lStructSize = sizeof(dialog);
      dialog.hwndOwner = static_cast<HWND>(window()->GetNativeWindowHandle());
      dialog.lpstrFilter = L"Xbox LIVE Arcade package or Xbox 360 disc image\0*.*\0";
      dialog.lpstrFile = file.data();
      dialog.nMaxFile = DWORD(file.size());
      dialog.lpstrTitle = L"Choose your Plants vs. Zombies package or Xbox 360 disc image";
      dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
      // The game may hide the Windows cursor; the dialog needs it.
      window()->SetCursorVisibility(rex::ui::Window::CursorVisibility::kVisible);
      if (GetOpenFileNameW(&dialog)) package = file.data();
      if (runtime()) ApplyHostSettings();
    }
#endif
    return package;
  }
  // In-game "Unlock Full Game": the player already confirmed ownership in
  // the native prompt. Files imported at first launch were checked then, so
  // only the declaration changes. Without such an import (development
  // assets), the package is picked and checked here, off the UI thread.
  void ChooseGamePackage() {
    const auto root = runtime()->game_data_root();
    if (const auto imported = pvz::ImportedPackageSha256(root); !imported.empty()) {
      std::thread([root, imported] {
        const auto edition = pvz::RecordImport(root, imported, true);
        REXLOG_INFO("PvZ in-game unlock receipt: {} ({})", edition.full ? "full" : "trial", edition.reason);
        pvz::PostUnlockResult({edition.full, edition.full ? std::string{} :
            "The game files changed since they were imported. Import the game again."});
      }).detach();
      return;
    }
    const auto package = ChooseGamePackageFile();
    if (package.empty()) { pvz::PostUnlockResult({}); return; }
    REXLOG_INFO("PvZ checking game package for the in-game unlock");
    std::thread([package, root] {
      const auto check = pvz::InspectGamePackage(package);
      if (!check.ok) { pvz::PostUnlockResult({false, check.error}); return; }
      const auto edition = pvz::RecordFullEdition(root, check.package_sha256);
      REXLOG_INFO("PvZ in-game unlock receipt: {} ({})", edition.full ? "full" : "trial", edition.reason);
      pvz::PostUnlockResult({edition.full, edition.full ? std::string{} :
          "The game files of this installation do not match the package. Import the game again."});
    }).detach();
  }
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
      REXLOG_INFO("PvZ display synchronization: {} (DXGI tearing {}, frames paced by {})", vsync, !vsync,
          vsync ? std::string("display refresh") : std::to_string(rex::cvar::Query<int32_t>("pvz_frame_limit")) + " FPS timer");
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
        // With VSync the game draws one frame per refresh, up to its 100 FPS
        // limit: pick the fastest advertised rate up to 100 Hz for this size,
        // so each frame is one 10 ms simulation step at 100 Hz. Otherwise the
        // monitor keeps its rate.
        DEVMODEW candidate{}; candidate.dmSize = sizeof(candidate);
        for (DWORD index = 0; EnumDisplaySettingsW(monitor.szDevice, index, &candidate); ++index)
          if (candidate.dmPelsWidth == display.dmPelsWidth && candidate.dmPelsHeight == display.dmPelsHeight &&
              candidate.dmDisplayFrequency >= 60 && candidate.dmDisplayFrequency <= 100 &&
              (!(display.dmFields & DM_DISPLAYFREQUENCY) || candidate.dmDisplayFrequency > display.dmDisplayFrequency)) {
            display.dmFields |= DM_DISPLAYFREQUENCY; display.dmDisplayFrequency = candidate.dmDisplayFrequency;
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
    // ImGui updates WantCaptureMouse only while it draws a dialog. A closed
    // Cheats/Achievements menu would otherwise leave it set and block the game.
    if (!window()->HasFocus() || (imgui_drawer() && imgui_drawer()->HasDialogs() &&
        imgui_drawer()->GetIO().WantCaptureMouse)) {
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
