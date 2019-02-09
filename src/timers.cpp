#include <iostream>
#include "memory.h"
#include "registers/div.h"
#include "registers/tac.h"
#include "registers/tima.h"
#include "registers/tma.h"
#include "timers.h"

namespace gb {
bool Timers::update(int ticks) {
  const Registers::Tac timer_control{memory->at(Registers::Tac::Address)};
  bool request_interrupt = false;

  if (timer_control.enabled()) {
    // total_ticks = (total_ticks * 4) + ticks;
    // total_ticks /= 4;
    total_ticks += ticks;

    while (total_ticks >= timer_control.clock_frequency()) {
      total_ticks -= timer_control.clock_frequency();
      u8 timer_value = memory->at(Registers::Tima::Address);
      timer_value++;

      // The timer overflowed
      if (timer_value == 0 && prev_timer == 0xff) {
        request_interrupt = true;
        timer_value = memory->at(Registers::Tma::Address);
      }

      prev_timer = timer_value;

      memory->set(Registers::Tima::Address, timer_value);
    }
  }

  div_ticks += ticks;

  while (div_ticks >= 256) {
    div_ticks -= 256;
    u8 divider_value = memory->at(Registers::Div::Address);
    memory->set(Registers::Div::Address, divider_value + 1);
  }

  return request_interrupt;
}
}  // namespace gb
