#include "gba/timer.h"
#include <doctest/doctest.h>
#include "gba/cpu.h"
#include "gba/sound.h"

namespace gb::advance {

bool Timer::increment_counter() {
  const u16 next_counter = ++counter;
  const bool did_overflow = next_counter == 0;

  if (did_overflow) {
    counter = reload_value;
    if (control.interrupt()) {
      m_cpu->interrupts_requested.set_interrupt(m_timer_interrupt, true);
    }

    if (m_sound->soundcnt_high.dma_sound_a_timer() == m_timer_number) {
      m_sound->read_fifo_a_sample();
    }
    if (m_sound->soundcnt_high.dma_sound_b_timer() == m_timer_number) {
      m_sound->read_fifo_b_sample();
    }
  }

  return did_overflow;
}
bool Timer::update(u32 cycles) {
  if (control.enabled()) {
    bool did_overflow = false;
    const u32 overflow_cycles = control.cycles();
    m_cycles += cycles;

    while (m_cycles >= overflow_cycles) {
      m_cycles -= overflow_cycles;
      if (!control.count_up()) {
        const bool overflow = increment_counter();
        did_overflow = did_overflow || overflow;
      }
    }
    return did_overflow;
    //}
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

TEST_CASE("Timer::Control::cycles() should produce the correct values") {
  Timer::Control control{0};

  CHECK(control.cycles() == 1);

  control.set_data(1);
  CHECK(control.cycles() == 64);

  control.set_data(2);
  CHECK(control.cycles() == 256);

  control.set_data(3);
  CHECK(control.cycles() == 1024);
}

}  // namespace gb::advance
