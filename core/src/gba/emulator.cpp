#include "emulator.h"
#include "gba/cpu.h"
#include "types.h"

namespace gb::advance {

void execute_hardware(Hardware hardware) {
  u32 cycles = hardware.cpu->execute();
  hardware.lcd->update(cycles);
}
}  // namespace gb::advance
