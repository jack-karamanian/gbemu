#pragma once
#include <array>
#include "types.h"

namespace gb {
class SquareSource {
  static constexpr std::array<u8, 5> DUTY_CYCLES = {0b00000001, 0b10000001,
                                                    0b10000111, 0b01111110, 0};

  // Timer/frequency
  int staged_timer_base = 0;
  int frequency = 0;
  int timer_base = 0;
  int timer = 0;

  int wave_progress = 0;

  int duty_cycle = 4;

  int sweep_shift = 0;
  int sweep_period = 0;

  int sweep_timer = 0;

  u8 output = 0;

  bool sweep_enabled;

  bool sweep_negate = false;

  bool enabled = false;

  bool is_overflowed(int freq) const { return freq > 2047; }

  void sweep_frequency();

  void set_frequency(int value) {
    frequency = value;
    timer_base = (2048 - value) * 4;
  }

  int calculate_next_frequency(int freq) const;

  void overflow_check(int freq);

 public:
  SquareSource(bool enabled) : sweep_enabled{enabled} {}

  void set_timer_base(int value) {
    staged_timer_base = value;
    set_frequency(value);
  }

  void set_duty_cycle(int value) { duty_cycle = DUTY_CYCLES.at(value); }

  void set_sweep_negate(bool negate) { sweep_negate = negate; }

  void set_sweep_shift(int shift) { sweep_shift = shift; }

  void set_sweep_period(int period) { sweep_period = period; }

  void clock_sweep();

  void enable();

  void update(int ticks) {
    timer -= ticks;
    if (timer <= 0) {
      timer += timer_base;
      if (++wave_progress > 7) {
        wave_progress = 0;
      }
      output = (duty_cycle & (0x80 >> wave_progress)) != 0 ? 15 : 0;
    }
  }

  u8 volume() const { return enabled ? output : 0; }
};
}  // namespace gb
