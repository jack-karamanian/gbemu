#pragma once
#include "registers/tac.h"
#include "registers/tima.h"
#include "registers/tma.h"
#include "types.h"

namespace gb {
class Memory;
class Timers {
  int timer_ticks = 0;
  u16 internal_counter = 0;

  Registers::Tac timer_control{0};
  u8 timer_value = 0;
  u8 timer_reset = 0;

 public:
  [[nodiscard]] Registers::Tac get_tac() const { return timer_control; }

  [[nodiscard]] u8 get_tima() const { return timer_value; }

  [[nodiscard]] u8 get_tma() const { return timer_reset; }

  [[nodiscard]] u8 get_div() const {
    return static_cast<u8>((internal_counter & 0xff00) >> 8);
  }

  void handle_memory_write(u16 addr, u8 value);
  bool update(int ticks);
};
}  // namespace gb
