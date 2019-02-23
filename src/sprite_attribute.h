#pragma once
#include "types.h"

namespace gb {
struct SpriteAttribute {
  u8 y;
  u8 x;
  u8 tile_index;
  u8 flags;

  bool above_bg() const { return (flags & 0x80) == 0; }

  bool flip_y() const { return (flags & 0x40) != 0; }

  bool flip_x() const { return (flags & 0x20) != 0; }

  int palette_number() const { return (flags & 0x10) >> 4; }
};
}  // namespace gb
