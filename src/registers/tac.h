#pragma once
#include <stdexcept>
#include "types.h"

namespace gb::Registers {
class Tac {
  u8 value;

 public:
  static constexpr u16 Address = 0xff07;
  explicit Tac(u8 val) : value{val} {}

  [[nodiscard]] bool enabled() const { return (value & 0x04) != 0; }

  [[nodiscard]] int clock_frequency() const {
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

  [[nodiscard]] u8 get_value() const { return value; }
};
}  // namespace gb::Registers
