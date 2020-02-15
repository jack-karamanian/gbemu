#include "emulator.h"
#include "gba/cpu.h"
#include "gba/gpu.h"
#include "gba/sound.h"
#include "gba/timer.h"
#include "types.h"

namespace gb::advance {

bool execute_hardware(Hardware hardware) {
  u32 cycles = hardware.cpu->execute();
  hardware.cpu->handle_interrupts();
  const bool draw_frame = hardware.lcd->update(cycles);
  hardware.timers->update(cycles);
  hardware.sound->update(cycles);
  return draw_frame;
}
}  // namespace gb::advance
