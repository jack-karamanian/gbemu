#include "cpu.h"
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "system.h"

namespace gb {
System::System()
    : cpu{std::make_unique<Cpu>(this)},
      memory{std::make_unique<Memory>(this)},
      lcd{std::make_unique<Lcd>(this)},
      gpu{std::make_unique<Gpu>(this)} {}
}  // namespace gb
