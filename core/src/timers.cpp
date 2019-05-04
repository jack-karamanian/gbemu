#include <iostream>
#include "memory.h"
#include "registers/div.h"
#include "timers.h"

namespace gb {
void Timers::handle_memory_write(u16 addr, u8 value) {
  switch (addr) {
    case Registers::Div::Address:
      internal_counter = 0;
      timer_ticks = 0;
      break;
    case Registers::Tac::Address:
      timer_control = Registers::Tac{value};
      break;
    case Registers::Tma::Address:
      timer_reset = value;
      break;
    case Registers::Tima::Address:
      timer_value = value;
      break;
  }
}

bool Timers::update(int ticks) {
  bool request_interrupt = false;
  timer_ticks += ticks;
  internal_counter += ticks;

  if (timer_control.enabled()) {
    while (timer_ticks >= timer_control.clock_frequency()) {
      timer_ticks -= timer_control.clock_frequency();
      timer_value++;

      // The timer overflowed
      if (timer_value == 0) {
        request_interrupt = true;
        timer_value = timer_reset;
      }
    }
  }

  return request_interrupt;
}
}  // namespace gb
