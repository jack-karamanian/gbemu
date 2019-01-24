#pragma once
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
}  // namespace gb
