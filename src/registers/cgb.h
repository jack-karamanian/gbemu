#include "types.h"
namespace gb::Registers::Cgb {
namespace Svbk {
constexpr u16 Address = 0xff70;
}
namespace Vbk {
constexpr u16 Address = 0xff4f;
}

namespace BgPaletteIndex {
constexpr u16 Address = 0xff68;
}

namespace BgPaletteColor {
constexpr u16 Address = 0xff69;
}

namespace SpritePaletteIndex {
constexpr u16 Address = 0xff6a;
}

namespace SpritePaletteColor {
constexpr u16 Address = 0xff6b;
}
}  // namespace gb::Registers::Cgb
