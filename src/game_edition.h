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
// Checks an XBLA LIVE/PIRS package: TitleID and the supported content hashes.
// Errors are short sentences for the in-game dialog.
// An Xbox 360 disc image is accepted too: the game's package is read from
// it, and owning the disc implies the full game (`disc`).
struct PackageCheck {
  bool ok = false;
  std::string error;
  std::string package_sha256;
  bool disc = false;
};
PackageCheck InspectGamePackage(const std::filesystem::path& package);
// First-run import: checks the package, then extracts all of its files into
// game_data_root. Nothing is written for an unsupported or damaged package.
PackageCheck ImportGamePackage(const std::filesystem::path& package, const std::filesystem::path& game_data_root);
// Writes the import receipt (same format as scripts/import_game.py) and
// re-evaluates the edition. Full needs the owner's full-copy declaration.
GameEdition RecordImport(const std::filesystem::path& game_data_root, const std::string& package_sha256,
    bool owner_confirmed_full, bool disc = false);
GameEdition RecordFullEdition(const std::filesystem::path& game_data_root, const std::string& package_sha256);
// Package hash from an existing receipt, or empty without a valid import.
std::string ImportedPackageSha256(const std::filesystem::path& game_data_root);
}
