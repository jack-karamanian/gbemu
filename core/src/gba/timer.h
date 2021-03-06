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

    constexpr explicit Control(Timer& timer)
        : Integer::Integer{0}, m_timer{&timer} {}

    void write_byte(unsigned int byte, u8 value) {
      if (byte == 0 && !enabled() && gb::test_bit(value, 7)) {
        m_timer->counter = m_timer->reload_value;
      }
      Integer::write_byte(byte, value);
    }

    [[nodiscard]] u32 cycles() const {
      // 0 -> 1
      // 1 -> 64
      // 2 -> 256
      // 3 -> 1024
      const auto n = m_value & 0b11;
      return 1 << ((4 + n * 2) * static_cast<int>(n > 0));
    }

    [[nodiscard]] bool count_up() const { return test_bit(2); }

    [[nodiscard]] bool interrupt() const { return test_bit(6); }

    [[nodiscard]] bool enabled() const { return test_bit(7); }

   private:
    Timer* m_timer;
  };

  u16 counter = 0;
  u16 reload_value = 0;
  Control control{*this};

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
