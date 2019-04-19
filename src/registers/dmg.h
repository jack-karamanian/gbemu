#pragma once
#include "div.h"
#include "interrupt_enabled.h"
#include "interrupt_request.h"
#include "lcd_stat.h"
#include "lcdc.h"
#include "lyc.h"
#include "palette.h"
#include "sound.h"
#include "tac.h"
#include "tima.h"
#include "tma.h"

namespace gb::Registers {
namespace Ly {
constexpr u16 Address = 0xff44;
}
}  // namespace gb::Registers
