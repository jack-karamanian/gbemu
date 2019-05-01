#pragma once
#include <utility>
#include "types.h"

namespace gb {
namespace Registers {
class Lcdc {
  u8 value;

 public:
  Lcdc(u8 value) : value{value} {}
  enum class SpriteMode { EightByEight, EightBySixteen };
  static constexpr u16 Address = 0xff40;

  // Bit 7
  bool controller_on() const { return value & 0x80; }

  // Bit 6
  std::pair<u16, u16> window_tile_map_range() const {
    bool high_area = value & 0x40;

    if (high_area) {
      return {0x9c00, 0x9fff};
    }

    return {0x9800, 0x9bff};
  }

  // Bit 5
  bool window_on() const { return value & 0x20; }

  // Bit 4
  std::pair<u16, u16> bg_tile_data_range() const {
    bool selection = value & 0x10;

    if (selection) {
      return {0x8000, 0x8fff};
    }

    return {0x8800, 0x97ff};
  }

  // Bit 3
  std::pair<u16, u16> bg_tile_map_range() const {
    bool selection = value & 0x8;

    if (selection) {
      return {0x9c00, 0x9fff};
    }

    return {0x9800, 0x9bff};
  }

  // Bit 2
  SpriteMode sprite_size() const {
    bool eight_by_sixteen = value & 0x4;
    return eight_by_sixteen ? SpriteMode::EightBySixteen
                            : SpriteMode::EightByEight;
  }

  // Bit 1
  bool obj_on() const { return value & 0x2; }

  // Bit 0
  bool bg_on() const { return value & 0x1; }

  // Extras
  u16 bg_tile_data_base() const {
    const auto [addr, _] = bg_tile_data_range();

    return addr == 0x8000 ? addr : 0x8800;
  }

  bool is_tile_map_signed() const { return bg_tile_data_base() == 0x8800; }
};
}  // namespace Registers
}  // namespace gb
