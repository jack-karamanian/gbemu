#include "emulator.h"
#include "gba/cpu.h"
#include "gba/gpu.h"
#include "gba/timer.h"
#include "types.h"

namespace gb::advance {

bool execute_hardware(Hardware hardware) {
  bool draw_frame = false;
  u32 cycles = hardware.cpu->execute();
  cycles += hardware.cpu->handle_interrupts();
  draw_frame = hardware.lcd->update(cycles);
  hardware.timers->update(cycles);
  return draw_frame;
}
}  // namespace gb::advance
