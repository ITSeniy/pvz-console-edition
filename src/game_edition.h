#pragma once
#include <filesystem>
#include <string>

namespace pvz {
struct GameEdition {
  bool full = false;
  std::string reason;
};
// This validates content plus an ownership declaration, not an Xbox license.
GameEdition EvaluateGameEdition(const std::filesystem::path& game_data_root, bool test_unlock = false);
}
