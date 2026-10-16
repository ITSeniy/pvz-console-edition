#pragma once
#include <rex/ui/keybinds.h>
#include <optional>
#include <string>
#include <string_view>

namespace pvz {
// Keyboard actions the player can rebind (PC Settings > Keyboard). Each is a
// string cvar holding a key name in the SDK's keybind spelling ("1", "X",
// "F12"), empty when unbound. Pause is the SDK's own keybind_start: the MnK
// driver turns it into Start of the mouse's player.
enum class KeyAction : int {
  Pause, Screenshot, Shovel,
  Seed1, Seed2, Seed3, Seed4, Seed5, Seed6, Seed7, Seed8, Seed9, Seed10,
  Count
};
inline constexpr int kKeyActionCount = int(KeyAction::Count);
struct KeyActionInfo { std::string_view cvar, default_key; };
const KeyActionInfo& KeyActionDefinition(KeyAction action);
// The key name bound to an action, "" when unbound.
std::string KeyBinding(KeyAction action);
// The SDK's name for a key; Enter is "Return", not its numpad alias.
std::string KeyName(rex::ui::VirtualKey key);
// Keys the SDK overlays keep for themselves (F3, F4, F7, Backtick).
bool ReservedKey(rex::ui::VirtualKey key);
// Binds a key name ("" clears) and saves it. An action already on that key
// loses it, so one key never does two things.
void BindKey(KeyAction action, std::string_view key);
void ResetKeyBindings();
// A key from the window or a replay. Repeats of a held key are dropped. The
// seed packets and the shovel go to the mouse player's lawn here; the
// returned action tells the caller about Pause and Screenshot.
std::optional<KeyAction> HandleGameKey(rex::ui::VirtualKey key, bool down);
// Rebinding: while active, the next pressed key is kept for the Keyboard
// menu and reaches nothing else.
void BeginKeyCapture();
void CancelKeyCapture();
bool KeyCaptureActive();
bool CaptureKey(rex::ui::VirtualKey key);
std::optional<rex::ui::VirtualKey> TakeCapturedKey();
}
