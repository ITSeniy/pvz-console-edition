#include "bridge_mnk_driver.h"
#include <cstdlib>
#include <iostream>

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
  // Only Escape (Start) reaches the game from the keyboard.
  rex::ui::KeyEvent key(nullptr, rex::ui::VirtualKey::kX, 1, false, false, false, false, false);
  driver.OnKeyDown(key); check(!(state() & X_INPUT_GAMEPAD_X));
  driver.OnKeyUp(key); check(!(state() & X_INPUT_GAMEPAD_X));
  rex::ui::KeyEvent escape(nullptr, rex::ui::VirtualKey::kEscape, 1, false, false, false, false, false);
  driver.OnKeyDown(escape); check(state() & X_INPUT_GAMEPAD_START);
  driver.OnKeyUp(escape); check(!(state() & X_INPUT_GAMEPAD_START));
  rex::cvar::SetFlagByName("pvz_mouse_cursor", "false");
  driver.OnMouseDown(left); check(state() & X_INPUT_GAMEPAD_A);
  // A release must clear a legacy held press even if the bridge is enabled.
  rex::cvar::SetFlagByName("pvz_mouse_cursor", "true");
  driver.OnMouseUp(left); check(!(state() & X_INPUT_GAMEPAD_A));
}
