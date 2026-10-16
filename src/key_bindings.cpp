#include "key_bindings.h"
#include "game_settings.h"
#include "mouse_bridge.h"
#include <rex/cvar.h>
#include <rex/logging.h>
#include <array>
#include <bitset>
#include <mutex>

REXCVAR_DEFINE_STRING(pvz_key_screenshot, "F12", "PvZ/Keyboard", "Key: save a screenshot");
REXCVAR_DEFINE_STRING(pvz_key_shovel, "X", "PvZ/Keyboard", "Key: shovel");
REXCVAR_DEFINE_STRING(pvz_key_seed1, "1", "PvZ/Keyboard", "Key: seed packet 1");
REXCVAR_DEFINE_STRING(pvz_key_seed2, "2", "PvZ/Keyboard", "Key: seed packet 2");
REXCVAR_DEFINE_STRING(pvz_key_seed3, "3", "PvZ/Keyboard", "Key: seed packet 3");
REXCVAR_DEFINE_STRING(pvz_key_seed4, "4", "PvZ/Keyboard", "Key: seed packet 4");
REXCVAR_DEFINE_STRING(pvz_key_seed5, "5", "PvZ/Keyboard", "Key: seed packet 5");
REXCVAR_DEFINE_STRING(pvz_key_seed6, "6", "PvZ/Keyboard", "Key: seed packet 6");
REXCVAR_DEFINE_STRING(pvz_key_seed7, "7", "PvZ/Keyboard", "Key: seed packet 7");
REXCVAR_DEFINE_STRING(pvz_key_seed8, "8", "PvZ/Keyboard", "Key: seed packet 8");
REXCVAR_DEFINE_STRING(pvz_key_seed9, "9", "PvZ/Keyboard", "Key: seed packet 9");
REXCVAR_DEFINE_STRING(pvz_key_seed10, "0", "PvZ/Keyboard", "Key: seed packet 10");

namespace {
using rex::ui::VirtualKey;
constexpr std::array<pvz::KeyActionInfo, pvz::kKeyActionCount> kActions{{
    {"keybind_start", "Escape"}, {"pvz_key_screenshot", "F12"}, {"pvz_key_shovel", "X"},
    {"pvz_key_seed1", "1"}, {"pvz_key_seed2", "2"}, {"pvz_key_seed3", "3"}, {"pvz_key_seed4", "4"},
    {"pvz_key_seed5", "5"}, {"pvz_key_seed6", "6"}, {"pvz_key_seed7", "7"}, {"pvz_key_seed8", "8"},
    {"pvz_key_seed9", "9"}, {"pvz_key_seed10", "0"}}};
std::mutex key_mutex;
std::bitset<256> held;
bool capturing = false;
std::optional<VirtualKey> captured;

// keybind_start may still hold a list from older configs ("Escape,Return");
// every listed key counts.
bool Bound(std::string_view value, VirtualKey key) {
  while (!value.empty()) {
    const auto comma = value.find(',');
    auto token = value.substr(0, comma);
    value = comma == std::string_view::npos ? std::string_view{} : value.substr(comma + 1);
    while (!token.empty() && token.front() == ' ') token.remove_prefix(1);
    while (!token.empty() && token.back() == ' ') token.remove_suffix(1);
    if (!token.empty() && rex::ui::ParseVirtualKey(token) == key) return true;
  }
  return false;
}
std::optional<pvz::KeyAction> BoundAction(VirtualKey key) {
  for (int i = 0; i < pvz::kKeyActionCount; ++i)
    if (Bound(rex::cvar::Query<std::string>(std::string(kActions[i].cvar)), key)) return pvz::KeyAction(i);
  return std::nullopt;
}
}

const pvz::KeyActionInfo& pvz::KeyActionDefinition(KeyAction action) { return kActions[size_t(action)]; }

std::string pvz::KeyBinding(KeyAction action) {
  return rex::cvar::Query<std::string>(std::string(kActions[size_t(action)].cvar));
}

std::string pvz::KeyName(VirtualKey key) {
  return key == VirtualKey::kReturn ? "Return" : rex::ui::VirtualKeyToString(key);
}

bool pvz::ReservedKey(VirtualKey key) {
  for (const char* bind : {"bind_debug_overlay", "bind_console", "bind_settings", "bind_achievements"}) {
    try {
      if (Bound(rex::cvar::Query<std::string>(bind), key)) return true;
    } catch (const std::exception&) {}
  }
  return false;
}

void pvz::BindKey(KeyAction action, std::string_view key) {
  const auto save = [](KeyAction which, std::string_view value) {
    const auto cvar = std::string(kActions[size_t(which)].cvar);
    rex::cvar::SetFlagByName(cvar, std::string(value));
    const bool saved = SaveSetting(cvar, "\"" + std::string(value) + "\"");
    REXLOG_INFO("PvZ key {} = {} (saved {})", cvar, value, saved);
  };
  if (const auto parsed = rex::ui::ParseVirtualKey(key); parsed != VirtualKey::kNone)
    for (int i = 0; i < kKeyActionCount; ++i)
      if (KeyAction(i) != action && Bound(KeyBinding(KeyAction(i)), parsed)) save(KeyAction(i), "");
  save(action, key);
}

void pvz::ResetKeyBindings() {
  for (int i = 0; i < kKeyActionCount; ++i)
    if (KeyBinding(KeyAction(i)) != kActions[i].default_key) BindKey(KeyAction(i), kActions[i].default_key);
}

std::optional<pvz::KeyAction> pvz::HandleGameKey(VirtualKey key, bool down) {
  const auto code = size_t(key);
  if (code >= 256) return std::nullopt;
  {
    std::lock_guard lock(key_mutex);
    // Windows repeats a held key's press; a seed packet would toggle.
    if (down && held[code]) return std::nullopt;
    if (!down && !held[code]) return std::nullopt;
    held[code] = down;
  }
  const auto action = BoundAction(key);
  if (!action) return std::nullopt;
  if (*action == KeyAction::Shovel) PublishKeyAction(kKeyShovel, down);
  else if (*action >= KeyAction::Seed1 && down) PublishKeyAction(int(*action) - int(KeyAction::Seed1), true);
  return action;
}

void pvz::BeginKeyCapture() { std::lock_guard lock(key_mutex); capturing = true; captured.reset(); }
void pvz::CancelKeyCapture() { std::lock_guard lock(key_mutex); capturing = false; }
bool pvz::KeyCaptureActive() { std::lock_guard lock(key_mutex); return capturing; }
bool pvz::CaptureKey(VirtualKey key) {
  std::lock_guard lock(key_mutex);
  if (!capturing) return false;
  // Modifiers alone cannot be bound: the SDK matches modifier masks exactly.
  if (key == VirtualKey::kShift || key == VirtualKey::kControl || key == VirtualKey::kMenu) return true;
  capturing = false;
  captured = key;
  return true;
}
std::optional<rex::ui::VirtualKey> pvz::TakeCapturedKey() {
  std::lock_guard lock(key_mutex);
  return std::exchange(captured, std::nullopt);
}
