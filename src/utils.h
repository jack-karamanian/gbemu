#pragma once
#include <array>
#include "types.h"
namespace gb {
template <typename... Args>
constexpr u8 get_bits(Args... args) {
  u8 res{0x00};
  int shift = sizeof...(args) - 1;

  for (bool set : {args...}) {
    if (set) {
      res |= 0x1 << shift;
    }
    --shift;
  }
  return res;
}

template <typename T>
T convert_bytes(const std::array<u8, sizeof(T)>& bytes) {
  T res{0};

  int shift = (sizeof(T) * 8) - 8;

  for (u8 byte : bytes) {
    res |= (byte << shift);
    shift -= 8;
  }

  return res;
}

}  // namespace gb
