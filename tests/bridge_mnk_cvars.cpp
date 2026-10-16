#include <rex/cvar.h>
#include <string_view>
#include <vector>
REXCVAR_DEFINE_BOOL(pvz_mouse_cursor, false, "Test", "Absolute mouse bridge");

// The key bindings save preferences and publish to the mouse bridge; the test
// records what reaches the bridge instead of linking the gameplay hooks.
std::vector<std::pair<int, bool>> published_keys;
namespace pvz {
bool SaveSetting(std::string_view, std::string_view) { return true; }
void PublishKeyAction(int action, bool down) { published_keys.emplace_back(action, down); }
}
