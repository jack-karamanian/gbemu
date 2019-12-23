#pragma once
#include "error_handling.h"
#include "gba/hardware.h"
#include "gba/interrupts.h"
#include "gba/mmu.h"
#include "utils.h"

namespace gb::advance {
class Sound;

class Timer {
 public:
  static constexpr u32 Frequency = 16777216;
  class Control : public Integer<u16> {
   public:
    using Integer::Integer;

    [[nodiscard]] u32 cycles() const {
      switch (m_value & 0b11) {
        case 0:
          return 1;
        case 1:
          return 64;
        case 2:
          return 256;
        case 3:
          return 1024;
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

  Timer(Cpu& cpu, Sound& sound, int timer_number)
      : m_cpu{&cpu},
        m_sound{&sound},
        m_timer_number{timer_number},
        m_timer_interrupt{timer_interrupts[timer_number]} {}

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
  static constexpr std::array timer_interrupts = {
      Interrupt::Timer0Overflow,
      Interrupt::Timer1Overflow,
      Interrupt::Timer2Overflow,
      Interrupt::Timer3Overflow,
  };

  Cpu* m_cpu;
  Sound* m_sound;
  int m_timer_number;
  Interrupt m_timer_interrupt;
  u32 m_cycles = 0;
};

struct Timers {
  Timer timer0;
  Timer timer1;
  Timer timer2;
  Timer timer3;

  Timers(Cpu& cpu, Sound& sound)
      : timer0{cpu, sound, 0},
        timer1{cpu, sound, 1},
        timer2{cpu, sound, 2},
        timer3{cpu, sound, 3} {}

  void update(u32 cycles);
};
}  // namespace gb::advance
