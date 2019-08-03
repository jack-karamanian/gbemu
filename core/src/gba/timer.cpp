#include "gba/cpu.h"
#include "timer.h"

namespace gb::advance {

bool Timer::increment_counter() {
  const u16 next_counter = ++counter;
  const bool did_overflow = next_counter == 0;

  if (did_overflow) {
    counter = reload_value;
    m_cpu->interrupts_requested.set_interrupt(m_interrupt, true);
  }

  return did_overflow;
}
bool Timer::update(u32 cycles) {
  if (control.enabled()) {
    m_cycles += cycles;

    const u32 overflow_cycles = control.cycles();
    if (m_cycles >= overflow_cycles) {
      m_cycles -= overflow_cycles;
      if (!control.count_up()) {
        if (m_interrupt == Interrupt::Timer0Overflow) {
        }
        return increment_counter();
      }
    }
  }
  return false;
}

static bool handle_count_up(Timer& timer, bool prev_did_overflow, u32 cycles) {
  return timer.control.count_up() && prev_did_overflow
             ? timer.increment_counter()
             : timer.update(cycles);
}

void Timers::update(u32 cycles) {
  bool did_overflow = handle_count_up(timer1, timer0.update(cycles), cycles);
  did_overflow = handle_count_up(timer2, did_overflow, cycles);
  handle_count_up(timer3, did_overflow, cycles);
}
}  // namespace gb::advance
