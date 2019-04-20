#include <iostream>
#include "memory.h"
#include "registers/div.h"
#include "timers.h"

namespace gb {
void Timers::handle_memory_write(u16 addr, u8 value) {
  // const Registers::Tac
  // timer_control{memory->get_ram(Registers::Tac::Address)};
  switch (addr) {
    case Registers::Div::Address:
      internal_counter = 0;
      timer_ticks = 0;
      // memory->set_ram(Registers::Div::Address, 0);
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
  // const Registers::Tac
  // timer_control{memory->get_ram(Registers::Tac::Address)};
  bool request_interrupt = false;
  timer_ticks += ticks;
  internal_counter += ticks;

  // memory->set_ram(Registers::Div::Address, (internal_counter & 0xff00) >> 8);

  if (timer_control.enabled()) {
    while (timer_ticks >= timer_control.clock_frequency()) {
      timer_ticks -= timer_control.clock_frequency();
      // u8 timer_value = memory->get_ram(Registers::Tima::Address) + 1;
      timer_value++;

      // The timer overflowed
      if (timer_value == 0) {
        request_interrupt = true;
        timer_value = timer_reset;
        // timer_value = memory->get_ram(Registers::Tma::Address);
      }

      // memory->set_ram(Registers::Tima::Address, timer_value);
    }
  }

  return request_interrupt;
}
}  // namespace gb
