#pragma once
#include "error_handling.h"
#include "gba/hardware.h"
#include "gba/interrupts.h"
#include "gba/mmu.h"
#include "utils.h"

namespace gb::advance {
class Timer {
 public:
  class Control : public Integer<u16> {
   public:
    using Integer::Integer;

    [[nodiscard]] u32 cycles() const {
      static constexpr u32 Frequency = 16780000;
      switch (m_value & 0b11) {
        case 0:
          return Frequency;
        case 1:
          return Frequency / 64;
        case 2:
          return Frequency / 256;
        case 3:
          return Frequency / 1024;
      }
      GB_UNREACHABLE();
    }

    [[nodiscard]] bool count_up() const { return test_bit(2); }

    [[nodiscard]] bool interrupt() const { return test_bit(6); }

    [[nodiscard]] bool enabled() const { return test_bit(7); }
  };

  u16 counter = 0;
  u16 reload_value = 0;
  Control control{0};

  Timer(Cpu& cpu, Interrupt interrupt) : m_cpu{&cpu}, m_interrupt{interrupt} {}

  bool increment_counter();

  bool update(u32 cycles);

  u16& select_counter_register(Mmu::DataOperation op) {
    switch (op) {
      case Mmu::DataOperation::Read:
        return counter;
      case Mmu::DataOperation::Write:
        return reload_value;
    }
    GB_UNREACHABLE();
  }

 private:
  u32 m_cycles = 0;
  Cpu* m_cpu;  // TODO: Interrupts
  Interrupt m_interrupt;
};

struct Timers {
  Timer timer0;
  Timer timer1;
  Timer timer2;
  Timer timer3;

  Timers(Cpu& cpu)
      : timer0{cpu, Interrupt::Timer0Overflow},
        timer1{cpu, Interrupt::Timer1Overflow},
        timer2{cpu, Interrupt::Timer2Overflow},
        timer3{cpu, Interrupt::Timer3Overflow} {}

  void update(u32 cycles);
};
}  // namespace gb::advance
