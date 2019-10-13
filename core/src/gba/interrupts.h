#pragma once
#include "utils.h"

namespace gb::advance {
enum class Interrupt : u32 {
  VBlank = 0,
  HBlank,
  VCountMatch,
  Timer0Overflow,
  Timer1Overflow,
  Timer2Overflow,
  Timer3Overflow,
  SerialCommunication,
  Dma0,
  Dma1,
  Dma2,
  Dma3,
  Keypad,
  GamePak,
};

class InterruptBucket : public Integer<u16> {
 public:
  using Integer::Integer;

  [[nodiscard]] bool interrupt_set(Interrupt interrupt) const {
    return test_bit(static_cast<u32>(interrupt));
  }

  void set_interrupt(Interrupt interrrupt, bool set) {
    set_bit(static_cast<u32>(interrrupt), set);
  }
};

class InterruptsRequested : public InterruptBucket {
 public:
  using InterruptBucket::InterruptBucket;
  constexpr void write_byte(unsigned int byte, u8 value) {
    const u32 shift = byte * 8;
    m_value = (m_value) & ~(value << shift);
  }
};
}  // namespace gb::advance
