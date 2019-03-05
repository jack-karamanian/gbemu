#pragma once
#include <array>
#include "types.h"

namespace gb {
class SquareSource {
  static constexpr std::array<std::array<bool, 8>, 5> DUTY_CYCLES = {
      {{false, false, false, false, false, false, false, true},
       {true, false, false, false, false, false, false, true},
       {true, false, false, false, false, true, true, true},
       {false, true, true, true, true, true, true, false},
       {false, false, false, false, false, false, false, false}}};

  // Timer/frequency
  int timer_base = 0;
  int timer = 0;

  int wave_progress = 0;

  int duty_cycle = 4;

 public:
  void enable() { timer = timer_base; }
  void set_timer_base(int value) { timer_base = (2048 - value) * 4; }

  void set_duty_cycle(int value) { duty_cycle = value; }

  u8 update();
};
}  // namespace gb
