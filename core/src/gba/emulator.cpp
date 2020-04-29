#include "emulator.h"
#include "gba/cpu.h"
#include "gba/gpu.h"
#include "gba/sound.h"
#include "gba/timer.h"
#include "types.h"

namespace gb::advance {

static thread_local int g_next_event_cycles = 1;

bool execute_hardware(const Hardware& hardware) {
  int next_event = std::numeric_limits<int>::max();

  u32 total_cycles = 0;
  while (g_next_event_cycles > 0) {
    if (hardware.cpu->halted) {
      total_cycles = g_next_event_cycles;
      break;
    }
    const u32 cycles = hardware.cpu->execute();
    total_cycles += cycles;
    g_next_event_cycles -= cycles;
  }
  const bool draw_frame = hardware.lcd->update(total_cycles, next_event);

  hardware.timers->update(total_cycles);
  hardware.sound->update(total_cycles, next_event);
  hardware.cpu->handle_interrupts();
  g_next_event_cycles = next_event;

  return draw_frame;
}
}  // namespace gb::advance
