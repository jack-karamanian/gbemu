#include <iostream>
#include "memory.h"
#include "registers/div.h"
#include "registers/tac.h"
#include "registers/tima.h"
#include "registers/tma.h"
#include "timers.h"

namespace gb {
void Timers::handle_memory_write(u16 addr, u8 value) {
  static_cast<void>(value);
  const Registers::Tac timer_control{memory->at(Registers::Tac::Address)};
  switch (addr) {
    case Registers::Div::Address:
      internal_counter = 0;
      timer_ticks = 0;
      memory->set_ram(Registers::Div::Address, 0);
      break;
  }
}

bool Timers::update(int ticks) {
  const Registers::Tac timer_control{memory->at(Registers::Tac::Address)};
  bool request_interrupt = false;
  timer_ticks += ticks;
  internal_counter += ticks;

  memory->set_ram(Registers::Div::Address, (internal_counter & 0xff00) >> 8);

  if (timer_control.enabled()) {
    while (timer_ticks >= timer_control.clock_frequency()) {
      timer_ticks -= timer_control.clock_frequency();
      u8 timer_value = memory->get_ram(Registers::Tima::Address) + 1;

      // The timer overflowed
      if (timer_value == 0) {
        request_interrupt = true;
        timer_value = memory->get_ram(Registers::Tma::Address);
      }

      memory->set_ram(Registers::Tima::Address, timer_value);
    }
  }

  return request_interrupt;
}
}  // namespace gb
