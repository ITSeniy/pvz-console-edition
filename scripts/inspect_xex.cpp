// Decode an XEX with the same SDK loader used by the game, without executing it.
#include <rex/runtime.h>
#include <rex/logging.h>
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: pvz_inspect <asset-directory> <output-image.bin>\n";
    return 2;
  }
  const auto root = std::filesystem::absolute(argv[1]);
  std::ifstream input(root / "default.xex", std::ios::binary);
  std::vector<unsigned char> header(0x1000);
  input.read(reinterpret_cast<char*>(header.data()), header.size());
  auto be32 = [&](size_t at) -> uint32_t {
    if (at + 4 > header.size()) throw std::runtime_error("Header outside buffer");
    return (uint32_t(header[at]) << 24) | (uint32_t(header[at+1]) << 16) |
           (uint32_t(header[at+2]) << 8) | header[at+3];
  };
  if (!input || be32(0) != 0x58455832) return 3;
  const auto security = be32(0x10);
  const auto size = be32(security + 4);
  const auto base = be32(security + 0x110);
  rex::InitLogging();
  rex::Runtime runtime(root, root.parent_path() / ".cache/inspect-user");
  rex::RuntimeConfig config;
  config.tool_mode = true;
  if (runtime.Setup(std::move(config)) != 0 ||
      runtime.LoadXexImage("game:\\default.xex") != 0) return 4;
  std::ofstream output(argv[2], std::ios::binary);
  output.write(reinterpret_cast<char*>(runtime.virtual_membase() + base), size);
  if (!output) return 5;
  std::cout << "base=0x" << std::hex << base << " size=0x" << size << '\n';
  runtime.Shutdown();
}
