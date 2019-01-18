#include "cpu.h"
#include "lcd.h"
#include "memory.h"
#include "system.h"

namespace gb {
System::System()
    : cpu{std::make_unique<Cpu>(this)},
      memory{std::make_unique<Memory>(this)},
      lcd{std::make_unique<Lcd>(this)} {}
}  // namespace gb
