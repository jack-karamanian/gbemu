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

namespace Hdma {
namespace SourceHigh {
constexpr u16 Address = 0xff51;
}
namespace SourceLow {
constexpr u16 Address = 0xff52;
}
namespace DestHigh {
constexpr u16 Address = 0xff53;
}
namespace DestLow {
constexpr u16 Address = 0xff54;
}
namespace Start {
constexpr u16 Address = 0xff55;
}
}  // namespace Hdma
}  // namespace gb::Registers::Cgb
