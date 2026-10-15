#include "game_edition.h"
#include "game_content_lock.h"
#include <toml++/toml.hpp>
#include <algorithm>
#include <cctype>
#include <optional>
#include <functional>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#endif

namespace {
#ifdef _WIN32
class Hasher {
 public:
  explicit Hasher(LPCWSTR algorithm) {
    Check(BCryptOpenAlgorithmProvider(&algorithm_, algorithm, nullptr, 0));
    DWORD size = 0, written = 0;
    Check(BCryptGetProperty(algorithm_, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&size), sizeof(size), &written, 0));
    object_.resize(size);
    Check(BCryptCreateHash(algorithm_, &hash_, object_.data(), size, nullptr, 0, 0));
  }
  ~Hasher() {
    if (hash_) BCryptDestroyHash(hash_);
    if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_, 0);
  }
  Hasher(const Hasher&) = delete;
  Hasher& operator=(const Hasher&) = delete;
  void Update(const void* data, size_t size) {
    Check(BCryptHashData(hash_, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size), 0));
  }
  std::vector<unsigned char> Digest() {
    DWORD length = 0, written = 0;
    Check(BCryptGetProperty(algorithm_, BCRYPT_HASH_LENGTH,
        reinterpret_cast<PUCHAR>(&length), sizeof(length), &written, 0));
    std::vector<unsigned char> digest(length);
    Check(BCryptFinishHash(hash_, digest.data(), length, 0));
    return digest;
  }
  std::string Hex() {
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (auto byte : Digest()) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
  }

 private:
  static void Check(NTSTATUS status) {
    if (status < 0) throw std::runtime_error("hash initialization failed");
  }
  BCRYPT_ALG_HANDLE algorithm_ = nullptr;
  BCRYPT_HASH_HANDLE hash_ = nullptr;
  std::vector<unsigned char> object_;
};

std::string Sha256(const std::filesystem::path& path) {
  Hasher hasher(BCRYPT_SHA256_ALGORITHM);
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Imported game file is missing");
  std::array<char, 65536> buffer;
  while (input.read(buffer.data(), buffer.size()) || input.gcount())
    hasher.Update(buffer.data(), static_cast<size_t>(input.gcount()));
  if (!input.eof()) throw std::runtime_error("Cannot read imported game file");
  return hasher.Hex();
}
#endif
}

pvz::GameEdition pvz::EvaluateGameEdition(const std::filesystem::path& root, bool test_unlock) {
  if (test_unlock) return {true, "explicit pvz_test_unlock"};
  try {
    std::ifstream input(root / ".pvz-import.toml", std::ios::binary);
    if (!input) return {false, "no own-copy import"};
    const auto receipt = toml::parse(input);
    if (receipt["schema_version"].value<int>() != 1 ||
        receipt["title_id"].value<std::string>() != kTitleId ||
        receipt["owner_confirmed_full_copy"].value<bool>() != true)
      return {false, "import has no full-copy ownership confirmation"};
    const auto package = receipt["package_sha256"].value_or(std::string{});
    if (package.size() != 64 || !std::all_of(package.begin(), package.end(),
        [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
      return {false, "invalid import provenance"};
    for (const auto& [name, expected] : kGameContentHashes) {
      if (receipt["files"][name].value<std::string>() != expected)
        return {false, "unsupported import content"};
#ifdef _WIN32
      if (Sha256(root / name) != expected)
        return {false, "imported content changed: " + std::string(name)};
#else
      return {false, "content verification is unavailable on this platform"};
#endif
    }
    return {true, "validated import and owner's full-copy declaration"};
  } catch (const std::exception& error) {
    return {false, std::string("invalid import: ") + error.what()};
  }
}

#ifdef _WIN32
namespace {
struct PackageFailure { std::string message; };
constexpr const char* kDamaged = "This game package is damaged.";
constexpr const char* kUnsupportedType = "This package type is not supported.";
[[noreturn]] void Fail(const char* message) { throw PackageFailure{message}; }

// Read-only LIVE/PIRS STFS, as scripts/extract_stfs.py (layout reference:
// rexglue-sdk stfs_xbox.h / stfs_container_device.cpp). Every data block is
// SHA-1 checked. Visits directories (content empty) and files in table order.
struct StfsEntry {
  std::filesystem::path path;
  bool directory = false;
  std::vector<unsigned char> content;
};
struct StfsPackage {
  std::vector<unsigned char> data;
  std::string sha256;
  bool disc = false; // read from an Xbox 360 disc image
};
// Xbox 360 disc image (XDVDFS, as scripts/extract_xiso.py). A retail disc
// carries the game's XBLA package in Content/<profile>/584109FF/000D0000/
// (for example the PopCap compilation disc); its files match the
// download, while the package header (licence) differs.
std::optional<std::vector<unsigned char>> ReadDiscPackage(std::ifstream& input) {
  constexpr uint64_t kSector = 2048;
  constexpr std::array<uint64_t, 4> kPartitions{0x2080000, 0xFD90000, 0x18300000, 0};
  const auto read = [&](uint64_t at, size_t size) {
    std::vector<unsigned char> bytes(size);
    input.clear();
    input.seekg(std::streamoff(at));
    input.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(size));
    if (size_t(input.gcount()) != size) bytes.clear();
    return bytes;
  };
  uint64_t base = UINT64_MAX;
  for (const auto partition : kPartitions) {
    const auto magic = read(partition + 32 * kSector, 20);
    if (magic.size() == 20 && !std::memcmp(magic.data(), "MICROSOFT*XBOX*MEDIA", 20)) { base = partition; break; }
  }
  if (base == UINT64_MAX) return std::nullopt;
  const auto descriptor = read(base + 32 * kSector + 20, 8);
  if (descriptor.size() != 8) Fail(kDamaged);
  const auto le32 = [](const unsigned char* at) { return uint32_t(at[0]) | at[1] << 8 | at[2] << 16 | uint32_t(at[3]) << 24; };
  struct Found { uint32_t sector = 0, size = 0; } found;
  int tables = 0;
  // Directory tables are binary trees of 4-byte aligned entries.
  std::function<void(uint32_t, uint32_t, std::string, int)> walk = [&](uint32_t sector, uint32_t size, std::string prefix, int depth) {
    if (found.size || !size || size > (1u << 20) || depth > 8 || ++tables > 4096) return;
    const auto table = read(base + uint64_t(sector) * kSector, size);
    if (table.size() != size) Fail(kDamaged);
    std::vector<bool> seen(size / 4 + 1);
    std::function<void(uint32_t)> node = [&](uint32_t offset) {
      if (found.size || offset + 14 > table.size() || seen[offset / 4]) return;
      seen[offset / 4] = true;
      const auto* entry = table.data() + offset;
      const uint32_t left = entry[0] | entry[1] << 8, right = entry[2] | entry[3] << 8;
      if (left == 0xFFFF && right == 0xFFFF) return;
      const auto start = le32(entry + 4), length = le32(entry + 8);
      const auto attributes = entry[12], name_length = entry[13];
      if (offset + 14 + name_length > table.size()) return;
      std::string name(reinterpret_cast<const char*>(entry + 14), name_length);
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::toupper(c)); });
      if (left) node(left * 4);
      const auto path = prefix + "/" + name;
      if (attributes & 0x10) walk(start, length, path, depth + 1);
      else if (path.find("/584109FF/000D0000/") != std::string::npos && length >= 0xA000) found = {start, length};
      if (right) node(right * 4);
    };
    node(0);
  };
  walk(le32(descriptor.data()), le32(descriptor.data() + 4), {}, 0);
  if (!found.size) Fail("This disc does not contain Plants vs. Zombies.");
  auto package = read(base + uint64_t(found.sector) * kSector, found.size);
  if (package.size() != found.size) Fail(kDamaged);
  return package;
}
StfsPackage ReadPackage(const std::filesystem::path& package) {
  std::ifstream input(package, std::ios::binary);
  if (!input) Fail("This file cannot be opened.");
  StfsPackage result;
  if (auto disc = ReadDiscPackage(input)) {
    result.data = std::move(*disc);
    result.disc = true;
  } else {
    input.clear();
    input.seekg(0);
    result.data.assign(std::istreambuf_iterator<char>(input), {});
  }
  const auto& data = result.data;
  if (data.size() < 4 || (std::memcmp(data.data(), "LIVE", 4) && std::memcmp(data.data(), "PIRS", 4)))
    Fail("This file is not an Xbox LIVE Arcade game package or an Xbox 360 disc image.");
  if (data.size() < 0xA000) Fail(kDamaged);
  char title[9];
  std::snprintf(title, sizeof(title), "%02X%02X%02X%02X", data[0x360], data[0x361], data[0x362], data[0x363]);
  if (title != pvz::kTitleId) Fail("This package is not Plants vs. Zombies.");
  Hasher hash(BCRYPT_SHA256_ALGORITHM);
  hash.Update(data.data(), data.size());
  result.sha256 = hash.Hex();
  return result;
}
template <class Visit>
void VisitPackage(const std::vector<unsigned char>& data, Visit&& visit) {
  const auto be = [&](size_t at, int size) {
    uint32_t value = 0;
    for (int i = 0; i < size; ++i) value = value << 8 | data[at + i];
    return value;
  };
  const auto le = [&](size_t at, int size) {
    uint32_t value = 0;
    for (int i = size - 1; i >= 0; --i) value = value << 8 | data[at + i];
    return value;
  };
  if (be(0x3A9, 4) != 0 || data[0x379] != 0x24 || !(data[0x37B] & 1)) Fail(kUnsupportedType);
  const size_t header_size = (be(0x340, 4) + 0xFFF) & ~size_t(0xFFF);
  const uint32_t total_blocks = be(0x395, 4);
  if (total_blocks > 170u * 170u) Fail(kUnsupportedType);
  const auto read_block = [&](uint32_t block, uint32_t& next) -> const unsigned char* {
    if (block >= total_blocks) Fail(kDamaged);
    size_t physical = block + (block + 170) / 170;
    if (block >= 170) physical += (block + 170 * 170) / (170 * 170);
    const size_t start = header_size + physical * 0x1000;
    const size_t hash_block = block < 170 ? 0 : (block / 170) * 171 + 1;
    const size_t record = header_size + hash_block * 0x1000 + (block % 170) * 0x18;
    if (start + 0x1000 > data.size() || record + 0x18 > data.size()) Fail(kDamaged);
    Hasher sha1(BCRYPT_SHA1_ALGORITHM);
    sha1.Update(data.data() + start, 0x1000);
    if (std::memcmp(sha1.Digest().data(), data.data() + record, 20)) Fail(kDamaged);
    next = be(record + 21, 3);
    return data.data() + start;
  };
  std::vector<std::filesystem::path> paths;
  std::vector<bool> seen_table(total_blocks);
  uint32_t table_block = le(0x37E, 3);
  for (uint32_t t = 0, tables = le(0x37C, 2); t < tables; ++t) {
    if (table_block >= total_blocks || seen_table[table_block]) Fail(kDamaged);
    seen_table[table_block] = true;
    uint32_t next_table = 0;
    const auto* table = read_block(table_block, next_table);
    for (size_t at = 0; at < 0x1000; at += 0x40) {
      const auto* entry = table + at;
      if (!entry[0]) break;
      const std::string name(reinterpret_cast<const char*>(entry), entry[40] & 0x3F);
      if (name.empty() || name == "." || name == ".." || name.find_first_of(std::string("\\/:\0", 4)) != std::string::npos)
        Fail(kDamaged);
      const uint32_t parent = (entry[50] << 8) | entry[51];
      if (parent != 0xFFFF && parent >= paths.size()) Fail(kDamaged);
      StfsEntry item;
      item.path = parent == 0xFFFF ? std::filesystem::path(name) : paths[parent] / name;
      paths.push_back(item.path);
      item.directory = entry[40] & 0x80;
      if (!item.directory) {
        uint32_t remaining = (uint32_t(entry[52]) << 24) | (entry[53] << 16) | (entry[54] << 8) | entry[55];
        uint32_t block = entry[47] | (entry[48] << 8) | (entry[49] << 16);
        std::vector<bool> seen(total_blocks);
        item.content.reserve(remaining);
        while (remaining) {
          if (block >= total_blocks || seen[block]) Fail(kDamaged);
          seen[block] = true;
          uint32_t next = 0;
          const auto* chunk = read_block(block, next);
          const uint32_t size = std::min<uint32_t>(remaining, 0x1000);
          item.content.insert(item.content.end(), chunk, chunk + size);
          remaining -= size;
          block = next;
        }
      }
      visit(item);
    }
    table_block = next_table;
  }
}
std::string Sha256(const std::vector<unsigned char>& content) {
  Hasher hash(BCRYPT_SHA256_ALGORITHM);
  hash.Update(content.data(), content.size());
  return hash.Hex();
}
// Checks the supported game files among the visited entries.
void CheckContent(const std::array<std::string, pvz::kGameContentHashes.size()>& found) {
  if (std::all_of(found.begin(), found.end(), [](const auto& hash) { return hash.empty(); }))
    Fail("This package does not contain the game.");
  for (size_t i = 0; i < found.size(); ++i)
    if (found[i] != pvz::kGameContentHashes[i].second) Fail("This version of Plants vs. Zombies is not supported.");
}
void RecordContent(std::array<std::string, pvz::kGameContentHashes.size()>& found, const StfsEntry& item) {
  if (item.directory) return;
  for (size_t i = 0; i < found.size(); ++i)
    if (item.path == std::filesystem::path(pvz::kGameContentHashes[i].first)) found[i] = Sha256(item.content);
}
}

pvz::PackageCheck pvz::InspectGamePackage(const std::filesystem::path& package) {
  try {
    const auto read = ReadPackage(package);
    std::array<std::string, kGameContentHashes.size()> found;
    VisitPackage(read.data, [&](const StfsEntry& item) { RecordContent(found, item); });
    CheckContent(found);
    return {true, {}, read.sha256, read.disc};
  } catch (const PackageFailure& failure) {
    return {false, failure.message};
  } catch (const std::exception&) {
    return {false, "This file cannot be read."};
  }
}

pvz::PackageCheck pvz::ImportGamePackage(const std::filesystem::path& package, const std::filesystem::path& root) {
  try {
    const auto read = ReadPackage(package);
    // Verify everything in memory first; the destination is touched only
    // for a supported, undamaged package.
    std::vector<StfsEntry> entries;
    std::array<std::string, kGameContentHashes.size()> found;
    VisitPackage(read.data, [&](const StfsEntry& item) {
      RecordContent(found, item);
      entries.push_back(item);
    });
    CheckContent(found);
    std::error_code error;
    std::filesystem::create_directories(root, error);
    const auto destination = std::filesystem::weakly_canonical(root, error);
    if (error) Fail("The game folder cannot be created.");
    for (const auto& item : entries) {
      const auto target = (destination / item.path).lexically_normal();
      const auto relative = target.lexically_relative(destination);
      if (relative.empty() || *relative.begin() == "..") Fail(kDamaged);
      if (item.directory) {
        std::filesystem::create_directories(target, error);
        if (error) Fail("The game files cannot be written to the game folder.");
        continue;
      }
      std::filesystem::create_directories(target.parent_path(), error);
      std::ofstream output(target, std::ios::binary | std::ios::trunc);
      output.write(reinterpret_cast<const char*>(item.content.data()), std::streamsize(item.content.size()));
      if (!output.flush()) Fail("The game files cannot be written to the game folder.");
    }
    return {true, {}, read.sha256, read.disc};
  } catch (const PackageFailure& failure) {
    return {false, failure.message};
  } catch (const std::exception&) {
    return {false, "This file cannot be read."};
  }
}

pvz::GameEdition pvz::RecordImport(const std::filesystem::path& root, const std::string& package_sha256,
    bool owner_confirmed_full, bool disc) {
  // Same receipt as scripts/import_game.py: hashes only, no private path.
  std::string receipt = "schema_version = 1\n";
  receipt += "title_id = \"" + std::string(kTitleId) + "\"\n";
  receipt += "package_sha256 = \"" + package_sha256 + "\"\n";
  if (disc) receipt += "source = \"disc\"\n";
  receipt += std::string("owner_confirmed_full_copy = ") + (owner_confirmed_full ? "true" : "false") + "\n\n[files]\n";
  for (const auto& [name, hash] : kGameContentHashes)
    receipt += "\"" + std::string(name) + "\" = \"" + std::string(hash) + "\"\n";
  std::error_code error;
  const auto marker = root / ".pvz-import.toml", pending = root / ".pvz-import.toml.pending";
  if (std::filesystem::is_symlink(marker, error)) return {false, "import receipt must not be a symbolic link"};
  std::filesystem::remove(pending, error);
  {
    std::ofstream output(pending, std::ios::binary | std::ios::trunc);
    if (!(output << receipt) || !output.flush()) return {false, "cannot write the import receipt"};
  }
  std::filesystem::rename(pending, marker, error);
  if (error) return {false, "cannot write the import receipt"};
  return EvaluateGameEdition(root);
}

pvz::GameEdition pvz::RecordFullEdition(const std::filesystem::path& root, const std::string& package_sha256) {
  return RecordImport(root, package_sha256, true);
}

std::string pvz::ImportedPackageSha256(const std::filesystem::path& root) {
  try {
    std::ifstream input(root / ".pvz-import.toml", std::ios::binary);
    if (!input) return {};
    const auto receipt = toml::parse(input);
    const auto package = receipt["package_sha256"].value_or(std::string{});
    return package.size() == 64 ? package : std::string{};
  } catch (const std::exception&) {
    return {};
  }
}
#endif
