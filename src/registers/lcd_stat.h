#pragma once
#include "types.h"

namespace gb {
namespace Registers {
class LcdStat {
  u8 value;

 public:
  constexpr static u16 Address = 0xff41;

  explicit LcdStat(u8 value) : value{value} {}

  // Bit 6
  bool ly_equals_lyc_enabled() const { return (value & 0x40) != 0; }

  // Bit 5
  bool oam_check_enabled() const { return (value & 0x20) != 0; }

  // Bit 4
  bool vblank_check_enabled() const { return (value & 0x10) != 0; }

  // Bit 3
  bool hblank_check_enabled() const { return (value & 0x08) != 0; }

  // Bit 2
  bool ly_equals_lyc() const { return (value & 0x04) != 0; }

  void set_ly_equals_lyc(bool equal) {
    const u8 val = equal ? 1 : 0;
    value |= (value & ~0x04) | (val << 2);
  }

  // Bits 1-0
  u8 mode() const { return value & 0x03; }

  void set_mode(u8 mode) { value = (value & ~0x03) | (mode & 0x03); }

  u8 get_value() const { return value; }
};
}  // namespace Registers
}  // namespace gb
