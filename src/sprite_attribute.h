#pragma once
#include "types.h"

namespace gb {
struct SpriteAttribute {
  u8 y;
  u8 x;
  u8 tile_index;
  u8 flags;

  [[nodiscard]] bool above_bg() const { return (flags & 0x80) == 0; }

  [[nodiscard]] bool flip_y() const { return (flags & 0x40) != 0; }

  [[nodiscard]] bool flip_x() const { return (flags & 0x20) != 0; }

  [[nodiscard]] int palette_number() const { return (flags & 0x10) >> 4; }

  [[nodiscard]] int vram_bank() const { return (flags & 0x8) >> 3; }

  [[nodiscard]] int cgb_palette_number() const { return flags & 0x7; }

  [[nodiscard]] int effective_palette_number() const {
    return palette_number() | cgb_palette_number();
  }

  static SpriteAttribute clear_dmg_palette(SpriteAttribute attribute) {
    attribute.flags &= ~0x10;
    return attribute;
  }

  static SpriteAttribute clear_cgb_flags(SpriteAttribute attribute) {
    attribute.flags &= ~0xf;
    return attribute;
  }
};
}  // namespace gb
