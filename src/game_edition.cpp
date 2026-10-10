#include "game_edition.h"
#include "game_content_lock.h"
#include <toml++/toml.hpp>
#include <algorithm>
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
std::string Sha256(const std::filesystem::path& path) {
  struct Handles {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<unsigned char> object;
    ~Handles() {
      if (hash) BCryptDestroyHash(hash);
      if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    }
  } handles;
  auto check = [](NTSTATUS status) {
    if (status < 0) throw std::runtime_error("SHA-256 initialization failed");
  };
  check(BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
  DWORD size = 0, written = 0;
  check(BCryptGetProperty(handles.algorithm, BCRYPT_OBJECT_LENGTH,
      reinterpret_cast<PUCHAR>(&size), sizeof(size), &written, 0));
  handles.object.resize(size);
  check(BCryptCreateHash(handles.algorithm, &handles.hash, handles.object.data(), size, nullptr, 0, 0));
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Imported game file is missing");
  std::array<char, 65536> buffer;
  while (input.read(buffer.data(), buffer.size()) || input.gcount())
    check(BCryptHashData(handles.hash, reinterpret_cast<PUCHAR>(buffer.data()),
        static_cast<ULONG>(input.gcount()), 0));
  if (!input.eof()) throw std::runtime_error("Cannot read imported game file");
  std::array<unsigned char, 32> digest;
  check(BCryptFinishHash(handles.hash, digest.data(), digest.size(), 0));
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
  return result;
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
