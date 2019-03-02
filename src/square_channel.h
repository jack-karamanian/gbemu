#pragma once
#include <array>
#include <utility>

namespace gb {
class SquareChannel {
  // Timer/frequency
  int timer_base = 0;
  int timer = 0;

  int wave_progress = 0;

  int length_counter = 0;
  bool length_enabled = false;

  bool enabled = false;

  std::array<bool, 8> duty_cycle;

 public:
  void set_timer_base(int value) { timer_base = (2048 - value) * 4; }

  void set_duty_cycle(const std::array<bool, 8>& value) { duty_cycle = value; }

  void set_length(int length) { length_counter = length; }

  void set_length_enabled(bool enabled) { length_enabled = enabled; }

  bool is_enabled() const { return enabled; }
  void enable();

  float update();

  void clock_length();
};
}  // namespace gb
