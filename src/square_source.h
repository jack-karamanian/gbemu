#pragma once
#include <array>
#include "types.h"

namespace gb {
class SquareSource {
  static constexpr std::array<u8, 5> DUTY_CYCLES = {0b00000001, 0b10000001,
                                                    0b10000111, 0b01111110, 0};

  // Timer/frequency
  int timer_base = 0;
  int timer = 0;

  int wave_progress = 0;

  int duty_cycle = 4;

 public:
  void enable() { timer = timer_base; }
  void set_timer_base(int value) { timer_base = (2048 - value) * 4; }

  void set_duty_cycle(int value) { duty_cycle = DUTY_CYCLES.at(value); }

  u8 update();
};
}  // namespace gb
