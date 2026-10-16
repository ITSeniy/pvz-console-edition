#include "bridge_mnk_driver.h"
#include "render_scale.h"
#include "mouse_input.h"
#include <cstdlib>
#include <iostream>

extern std::vector<std::pair<int, bool>> published_keys;
void check(bool condition) {
  if (!condition) { std::cerr << "Mouse/controller input isolation failed\n"; std::exit(1); }
}
int main() {
  using namespace rex::input;
  using rex::X_RESULT;
  using rex::ui::MouseEvent;
  rex::cvar::SetFlagByName("mnk_mode", "true");
  rex::cvar::SetFlagByName("keybind_a", "LMB");
  rex::cvar::SetFlagByName("keybind_b", "RMB");
  rex::cvar::SetFlagByName("keybind_x", "X");
  rex::cvar::SetFlagByName("keybind_start", "Escape");
  pvz::BridgeMnkDriver driver;
  check(driver.Setup() == 0);
  std::vector<DeviceInfo> devices;
  driver.EnumerateDevices(devices);
  check(devices.size() == 1);
  const auto state = [&] {
    X_INPUT_STATE out{};
    check(driver.GetDeviceState(devices.front().id, &out) == X_ERROR_SUCCESS);
    return uint16_t(out.gamepad.buttons);
  };
  MouseEvent left(nullptr, MouseEvent::Button::kLeft, 0, 0);
  MouseEvent right(nullptr, MouseEvent::Button::kRight, 0, 0);
  rex::cvar::SetFlagByName("pvz_mouse_cursor", "true");
  driver.OnMouseDown(left); driver.OnMouseDown(right);
  check(!(state() & (X_INPUT_GAMEPAD_A | X_INPUT_GAMEPAD_B)));
  driver.OnMouseUp(left); driver.OnMouseUp(right);
  // Keys never become controller buttons: X is the shovel (held, then
  // released), digits pick seed packets, and only Pause (Escape) is Start.
  const auto key_event = [](rex::ui::VirtualKey vk) {
    return rex::ui::KeyEvent(nullptr, vk, 1, false, false, false, false, false);
  };
  auto key = key_event(rex::ui::VirtualKey::kX);
  driver.OnKeyDown(key); check(!(state() & X_INPUT_GAMEPAD_X));
  driver.OnKeyDown(key); // Windows' auto-repeat of a held key.
  driver.OnKeyUp(key); check(!(state() & X_INPUT_GAMEPAD_X));
  check(published_keys == std::vector<std::pair<int, bool>>{{pvz::kKeyShovel, true}, {pvz::kKeyShovel, false}});
  published_keys.clear();
  auto one = key_event(rex::ui::VirtualKey::k1), zero = key_event(rex::ui::VirtualKey::k0);
  driver.OnKeyDown(one); driver.OnKeyUp(one); driver.OnKeyDown(zero); driver.OnKeyUp(zero);
  check(published_keys == std::vector<std::pair<int, bool>>{{0, true}, {9, true}});
  published_keys.clear();
  auto escape = key_event(rex::ui::VirtualKey::kEscape);
  driver.OnKeyDown(escape); check(state() & X_INPUT_GAMEPAD_START);
  driver.OnKeyUp(escape); check(!(state() & X_INPUT_GAMEPAD_START));
  check(published_keys.empty());
  // Rebinding moves a key from the action that had it; the captured key
  // reaches nothing else.
  pvz::BeginKeyCapture();
  driver.OnKeyDown(one); driver.OnKeyUp(one);
  check(published_keys.empty() && pvz::TakeCapturedKey() == rex::ui::VirtualKey::k1);
  pvz::BindKey(pvz::KeyAction::Shovel, pvz::KeyName(rex::ui::VirtualKey::k1));
  check(pvz::KeyBinding(pvz::KeyAction::Shovel) == "1" && pvz::KeyBinding(pvz::KeyAction::Seed1).empty());
  driver.OnKeyDown(one); driver.OnKeyUp(one);
  check(published_keys == std::vector<std::pair<int, bool>>{{pvz::kKeyShovel, true}, {pvz::kKeyShovel, false}});
  pvz::ResetKeyBindings();
  check(pvz::KeyBinding(pvz::KeyAction::Shovel) == "X" && pvz::KeyBinding(pvz::KeyAction::Seed1) == "1");
  check(pvz::KeyName(rex::ui::VirtualKey::kReturn) == "Return");
  // Render scale: auto renders at least as tall as the output.
  check(pvz::ResolveRenderScale("auto", 720) == 1 && pvz::ResolveRenderScale("auto", 768) == 1);
  check(pvz::ResolveRenderScale("auto", 1080) == 2 && pvz::ResolveRenderScale("auto", 1440) == 2);
  check(pvz::ResolveRenderScale("auto", 1600) == 3 && pvz::ResolveRenderScale("auto", 2160) == 3);
  check(pvz::ResolveRenderScale("auto", 4320) == 4 && pvz::ResolveRenderScale("3", 720) == 3);
  rex::cvar::SetFlagByName("pvz_mouse_cursor", "false");
  driver.OnMouseDown(left); check(state() & X_INPUT_GAMEPAD_A);
  // A release must clear a legacy held press even if the bridge is enabled.
  rex::cvar::SetFlagByName("pvz_mouse_cursor", "true");
  driver.OnMouseUp(left); check(!(state() & X_INPUT_GAMEPAD_A));
}
