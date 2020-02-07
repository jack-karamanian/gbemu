#pragma once
#include <utility>
#include "types.h"

namespace gb::Registers {
class Lcdc {
  u8 m_value;

 public:
  Lcdc(u8 value) : m_value{value} {}
  enum class SpriteMode { EightByEight, EightBySixteen };
  static constexpr u16 Address = 0xff40;

  // Bit 7
  [[nodiscard]] bool controller_on() const { return (m_value & 0x80) != 0; }

  // Bit 6
  [[nodiscard]] std::pair<u16, u16> window_tile_map_range() const {
    bool high_area = (m_value & 0x40) != 0;

    if (high_area) {
      return {0x9c00, 0x9fff};
    }

    return {0x9800, 0x9bff};
  }

  // Bit 5
  [[nodiscard]] bool window_on() const { return (m_value & 0x20) != 0; }

  // Bit 4
  [[nodiscard]] std::pair<u16, u16> bg_tile_data_range() const {
    bool selection = (m_value & 0x10) != 0;

    if (selection) {
      return {0x8000, 0x8fff};
    }

    return {0x8800, 0x97ff};
  }

  // Bit 3
  [[nodiscard]] std::pair<u16, u16> bg_tile_map_range() const {
    bool selection = (m_value & 0x8) != 0;

    if (selection) {
      return {0x9c00, 0x9fff};
    }

    return {0x9800, 0x9bff};
  }

  // Bit 2
  [[nodiscard]] SpriteMode sprite_size() const {
    bool eight_by_sixteen = (m_value & 0x4) != 0;
    return eight_by_sixteen ? SpriteMode::EightBySixteen
                            : SpriteMode::EightByEight;
  }

  // Bit 1
  [[nodiscard]] bool obj_on() const { return (m_value & 0x2) != 0; }

  // Bit 0
  [[nodiscard]] bool bg_on() const { return (m_value & 0x1) != 0; }

  // Extras
  [[nodiscard]] u16 bg_tile_data_base() const {
    const auto [addr, _] = bg_tile_data_range();

    return addr == 0x8000 ? addr : 0x8800;
  }

  [[nodiscard]] bool is_tile_map_signed() const {
    return bg_tile_data_base() == 0x8800;
  }
};
}  // namespace gb::Registers
