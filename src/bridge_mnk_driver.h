#pragma once
#include <rex/cvar.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <atomic>

namespace pvz {
// Escape was pressed: the SDK maps it to Start of the mouse's player, which
// the bridge then handles as PC's pause key.
inline std::atomic_bool& KeyboardStart() { static std::atomic_bool pressed{false}; return pressed; }
// F12 asks for a screenshot (frame_capture.cpp); the game never sees it.
inline std::atomic_bool& ScreenshotRequested() { static std::atomic_bool pressed{false}; return pressed; }
using namespace rex::input;
using rex::X_STATUS;
using rex::X_RESULT;
// Absolute mouse events belong to the PvZ bridge. Letting MnK map the same
// press to a held controller A also enables native drag/auto-collect logic.
class BridgeMnkDriver final : public InputDriver,
    public rex::ui::WindowInputListener, public rex::ui::WindowListener {
 public:
  BridgeMnkDriver() : InputDriver(nullptr, 0), inner_(nullptr, 0) {
    inner_.set_is_active_callback([this] { return is_active(); });
  }
  ~BridgeMnkDriver() override {
    if (auto* w = attached_.load()) w->app_context().CallInUIThreadSynchronous([this, w] {
      w->RemoveInputListener(this); w->RemoveListener(this); attached_ = nullptr;
    });
  }
  X_STATUS Setup() override { return inner_.Setup(); }
  void OnWindowAvailable(rex::ui::Window* w) override {
    inner_.OnWindowAvailable(w);
    w->RemoveInputListener(&inner_);
    attached_ = w;
    w->AddInputListener(this, window_z_order());
    w->AddListener(this);
  }
  void OnClosing(rex::ui::UIEvent&) override {
    if (auto* w = attached_.exchange(nullptr)) {
      w->RemoveInputListener(this); w->RemoveListener(this);
    }
  }
  void EnumerateDevices(std::vector<DeviceInfo>& out) override { inner_.EnumerateDevices(out); }
  X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out) override { return inner_.GetDeviceState(id, out); }
  X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t flags, X_INPUT_CAPABILITIES* out) override {
    return inner_.GetDeviceCapabilities(id, flags, out);
  }
  X_RESULT SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION* v) override { return inner_.SetDeviceVibration(id, v); }
  X_RESULT GetDeviceKeystroke(DeviceId id, uint32_t flags, X_INPUT_KEYSTROKE* out) override {
    return inner_.GetDeviceKeystroke(id, flags, out);
  }
  // The mouse and the controller cover every action; the keyboard keeps only
  // Escape (Start: pause and back) and F12 (screenshot). Key releases always pass, so a key held
  // across a settings change cannot stay pressed.
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (e.virtual_key() == rex::ui::VirtualKey::kEscape) { KeyboardStart().store(true); inner_.OnKeyDown(e); }
    if (e.virtual_key() == rex::ui::VirtualKey::kF12) ScreenshotRequested().store(true);
  }
  void OnKeyUp(rex::ui::KeyEvent& e) override { inner_.OnKeyUp(e); }
  void OnMouseDown(rex::ui::MouseEvent& e) override {
    if (!rex::cvar::Query<bool>("pvz_mouse_cursor")) inner_.OnMouseDown(e);
  }
  void OnMouseUp(rex::ui::MouseEvent& e) override { inner_.OnMouseUp(e); }
  void OnMouseMove(rex::ui::MouseEvent& e) override {
    if (!rex::cvar::Query<bool>("pvz_mouse_cursor")) inner_.OnMouseMove(e);
  }
 private:
  rex::input::mnk::MnkInputDriver inner_;
  std::atomic<rex::ui::Window*> attached_{nullptr};
};
}
