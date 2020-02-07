#pragma once
#include "types.h"

namespace gb::Registers {
class LcdStat {
  u8 m_value;

 public:
  constexpr static u16 Address = 0xff41;

  explicit LcdStat(u8 value) : m_value{value} {}

  // Bit 6
  [[nodiscard]] bool ly_equals_lyc_enabled() const {
    return (m_value & 0x40) != 0;
  }

  // Bit 5
  [[nodiscard]] bool oam_check_enabled() const { return (m_value & 0x20) != 0; }

  // Bit 4
  [[nodiscard]] bool vblank_check_enabled() const {
    return (m_value & 0x10) != 0;
  }

  // Bit 3
  [[nodiscard]] bool hblank_check_enabled() const {
    return (m_value & 0x08) != 0;
  }

  // Bit 2
  [[nodiscard]] bool ly_equals_lyc() const { return (m_value & 0x04) != 0; }

  void set_ly_equals_lyc(bool equal) {
    const u8 val = equal ? 1 : 0;
    m_value |= (m_value & ~0x04) | (val << 2);
  }

  // Bits 1-0
  [[nodiscard]] u8 mode() const { return m_value & 0x03; }

  void set_mode(u8 mode) { m_value = (m_value & ~0x03) | (mode & 0x03); }

  [[nodiscard]] u8 get_value() const { return m_value; }

  [[nodiscard]] LcdStat write_value(u8 next_value) const {
    // Preserve mode and ly_equals_lyc when LcdStat is written to
    const u8 ly_eq_lyc = ly_equals_lyc() ? 1 : 0;
    const u8 clean_next_value = (next_value & ~0x7) | (ly_eq_lyc << 2) | mode();
    return LcdStat{clean_next_value};
  }
};
}  // namespace gb::Registers
