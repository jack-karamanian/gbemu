#pragma once

namespace gb {
class EnvelopeMod {
  int period = 0;
  int timer = 0;
  int starting_volume = 0;
  int volume = 0;
  bool increase_volume = false;

 public:
  void enable();

  void set_starting_volume(int value) { starting_volume = value; }

  void set_increase_volume(bool value) { increase_volume = value; }

  void set_period(int value) { period = value == 0 ? 8 : value; }

  void clock(int step);

  float update(float input_volume) const;
};
}  // namespace gb
