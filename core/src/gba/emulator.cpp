#include "emulator.h"
#include "gba/cpu.h"
#include "gba/timer.h"
#include "types.h"

namespace gb::advance {

void execute_hardware(Hardware hardware) {
  const u32 cycles = hardware.cpu->execute();
  hardware.cpu->handle_interrupts();
  hardware.lcd->update(cycles);
  hardware.timers->update(cycles);
}
}  // namespace gb::advance
