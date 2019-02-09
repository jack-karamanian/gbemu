#pragma once
#include "types.h"

namespace gb {
namespace Registers {
class Tac {
  u8 value;

 public:
  static constexpr u16 Address = 0xff07;
  Tac(u8 val) : value{val} {}

  bool enabled() const { return (value & 0x04) != 0; }

  int clock_frequency() const {
    switch (value & 0x03) {
      case 0x00:
        return 1024;
      case 0x01:
        return 16;
      case 0x02:
        return 64;
      case 0x03:
        return 256;
      default:
        throw std::runtime_error("invalid tac clock frequency");
    }
  }
};
}  // namespace Registers
}  // namespace gb
