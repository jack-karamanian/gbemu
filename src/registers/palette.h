#pragma once
#include "types.h"

namespace gb {
struct Palette {
  u8 value;

  int get_color(int index) const { return value & (0x3 << (2 * index)); }
};
namespace Registers {
namespace Palette {
namespace Background {
constexpr u16 Address = 0xff47;
}
namespace Obj0 {
constexpr u16 Address = 0xff48;
}

namespace Obj1 {
constexpr u16 Address = 0xff49;
}

}  // namespace Palette
}  // namespace Registers
}  // namespace gb
