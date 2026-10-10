#pragma once
#include <bit>
#include <cstdint>
#include <cstring>

namespace pvz::guest {
inline uint32_t Read(uint8_t* base, uint32_t address) {
  uint32_t value;
  std::memcpy(&value, base + address, sizeof(value));
  return std::byteswap(value);
}
inline void Write(uint8_t* base, uint32_t address, uint32_t value) {
  value = std::byteswap(value);
  std::memcpy(base + address, &value, sizeof(value));
}
inline float Float(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(Read(base, address));
}
}
