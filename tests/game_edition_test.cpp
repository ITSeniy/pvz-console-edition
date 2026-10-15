#include "game_edition.h"
#include "game_content_lock.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  if (argc < 2 || argc > 4) return 1;
  const auto root = std::filesystem::temp_directory_path() /
      ("pvz-edition-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  struct Cleanup {
    std::filesystem::path root;
    ~Cleanup() {
      std::error_code error;
      for (const auto& [name, hash] : pvz::kGameContentHashes) std::filesystem::remove(root / name, error);
      std::filesystem::remove(root / ".pvz-import.toml", error);
      std::filesystem::remove(root / "not-a-package.bin", error);
      std::filesystem::remove(root, error);
    }
  } cleanup{root};
  auto check = [&](bool expected, const char* description, bool test_unlock = false) {
    const auto result = pvz::EvaluateGameEdition(root, test_unlock);
    if (result.full != expected) throw std::runtime_error(std::string(description) + ": " + result.reason);
    std::cout << description << ": passed\n";
  };
  auto receipt = [&](bool owned, std::string title = std::string(pvz::kTitleId)) {
    std::ofstream file(root / ".pvz-import.toml");
    file << "schema_version = 1\ntitle_id = \"" << title << "\"\npackage_sha256 = \""
         << std::string(64, 'a') << "\"\nowner_confirmed_full_copy = " << (owned ? "true" : "false") << "\n[files]\n";
    for (const auto& [name, hash] : pvz::kGameContentHashes) file << '"' << name << "\" = \"" << hash << "\"\n";
  };
  try {
    check(false, "No import stays Trial");
    check(true, "Explicit test unlock enables Full without receipt", true);
    check(false, "Disabling test unlock restores Trial");
    receipt(false);
    check(false, "No ownership declaration stays Trial");
    receipt(true);
    check(false, "Receipt alone without content stays Trial");
    for (const auto& [name, hash] : pvz::kGameContentHashes)
      std::filesystem::copy_file(std::filesystem::path(argv[1]) / name, root / name);
    check(true, "Validated content plus ownership enables Full");
    receipt(true, "00000000");
    check(false, "Other title stays Trial");
    receipt(true);
    { std::ofstream file(root / "default.xex", std::ios::binary | std::ios::app); file << 'X'; }
    check(false, "Changed game content stays Trial");
    std::filesystem::copy_file(std::filesystem::path(argv[1]) / "default.xex", root / "default.xex",
                               std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(root / "main.pak");
    check(false, "Missing resource pack stays Trial");
    { std::ofstream file(root / ".pvz-import.toml"); file << "invalid ["; }
    check(false, "Broken receipt stays Trial");
    check(true, "Explicit test unlock works independently of receipt", true);
    { std::ofstream file(root / "not-a-package.bin", std::ios::binary); file << "CON not a package"; }
    if (pvz::InspectGamePackage(root / "not-a-package.bin").ok) throw std::runtime_error("Other file accepted as package");
    if (pvz::InspectGamePackage(root / "missing.bin").ok) throw std::runtime_error("Missing file accepted as package");
    std::cout << "Non-packages are rejected: passed\n";
    std::filesystem::copy_file(std::filesystem::path(argv[1]) / "main.pak", root / "main.pak");
    std::filesystem::remove(root / ".pvz-import.toml");
    const auto recorded = pvz::RecordFullEdition(root, std::string(64, 'b'));
    if (!recorded.full) throw std::runtime_error("In-game unlock receipt: " + recorded.reason);
    std::cout << "In-game unlock receipt enables Full: passed\n";
    if (argc == 3 && std::filesystem::exists(argv[2])) {
      const auto package = pvz::InspectGamePackage(argv[2]);
      if (!package.ok || package.package_sha256.size() != 64)
        throw std::runtime_error("Supported XBLA package rejected: " + package.error);
      std::cout << "Supported XBLA package accepted: passed\n";
      const auto imported = root / "first-run";
      struct ImportCleanup {
        std::filesystem::path path;
        ~ImportCleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
      } import_cleanup{imported};
      if (pvz::ImportGamePackage(root / "not-a-package.bin", imported).ok || std::filesystem::exists(imported / "default.xex"))
        throw std::runtime_error("Rejected package wrote game files");
      const auto extracted = pvz::ImportGamePackage(argv[2], imported);
      if (!extracted.ok || extracted.package_sha256 != package.package_sha256)
        throw std::runtime_error("First-run import failed: " + extracted.error);
      if (pvz::RecordImport(imported, extracted.package_sha256, false).full ||
          pvz::ImportedPackageSha256(imported) != extracted.package_sha256)
        throw std::runtime_error("Trial answer did not record a trial import");
      if (!pvz::RecordImport(imported, pvz::ImportedPackageSha256(imported), true).full)
        throw std::runtime_error("Unlock after a trial import did not enable Full");
      std::cout << "First-run import extracts files; trial then full receipt: passed\n";
    }
    if (argc == 4 && std::filesystem::exists(argv[3])) {
      const auto disc = pvz::InspectGamePackage(argv[3]);
      if (!disc.ok || !disc.disc) throw std::runtime_error("Xbox 360 disc image rejected: " + disc.error);
      std::cout << "Xbox 360 disc image accepted as full: passed\n";
    }
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
