#pragma once
#include <array>
#include "types.h"

namespace gb {
class SquareSource {
  static constexpr std::array<u8, 5> DUTY_CYCLES = {0b00000001, 0b10000001,
                                                    0b10000111, 0b01111110, 0};
  bool enabled = false;

  // Timer/frequency
  int staged_timer_base = 0;
  int frequency = 0;
  int timer_base = 0;
  int timer = 0;

  int wave_progress = 0;

  int duty_cycle = 4;

  bool sweep_enabled;

  bool sweep_negate = false;
  int sweep_shift = 0;
  int sweep_period = 0;

  int sweep_timer = 0;

  bool is_overflowed(int freq) const { return freq > 2047; }

  void sweep_frequency();

  void set_frequency(int value) {
    frequency = value;
    timer_base = (2048 - value) * 4;
  }

  int calculate_next_frequency(int freq) const;

  u8 output = 0;

 public:
  SquareSource(bool enabled) : sweep_enabled{enabled} {}

  void set_timer_base(int value) { staged_timer_base = value; }

  void set_duty_cycle(int value) { duty_cycle = DUTY_CYCLES.at(value); }

  void set_sweep_negate(bool negate) { sweep_negate = negate; }

  void set_sweep_shift(int shift) { sweep_shift = shift; }

  void set_sweep_period(int period) { sweep_period = period; }

  void clock_sweep();

  void enable();

  u8 update();
};
}  // namespace gb
