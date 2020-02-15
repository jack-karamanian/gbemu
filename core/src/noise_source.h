#pragma once
#include <array>
#include "types.h"

namespace gb {

class NoiseSource {
  static constexpr int FREQUENCY = 4194304 / 8;
  static constexpr std::array<int, 8> clock_divisors = {
      FREQUENCY * 2, FREQUENCY,     FREQUENCY / 2, FREQUENCY / 3,
      FREQUENCY / 4, FREQUENCY / 5, FREQUENCY / 6, FREQUENCY / 7,
  };
  template <typename T>
  static constexpr auto pow(T i, T n) {
    if (n == 0) {
      return 1;
    } else {
      return i * pow(i, n - 1);
    }
  }
  constexpr static std::array<std::array<int, 14>, 8> timers = [] {
    std::array<std::array<int, 14>, 8> res = {};

    for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 14; j++) {
        res[i][j] = 4194304 / (clock_divisors[i] / (1 << (j + 1)));
      }
    }

    return res;
  }();

  // 0 - 7
  int clock_divisor = 0;

  // 0 - 13
  int prescalar_divider = 0;

  int timer_base = 0;

  int timer = 0;

  u16 lfsr_counter = 0;

  u8 output = 0;

  bool seven_stage = false;

  std::size_t counter = 0;

  void update_timer() {
    if (prescalar_divider < 14) {
      timer_base = timers.at(clock_divisor).at(prescalar_divider);
    }
  }

 public:
  void set_num_stages(bool value) { seven_stage = value; }
  void set_clock_divisor(int value) {
    clock_divisor = value;
    update_timer();
  }
  void set_prescalar_divider(int value) {
    prescalar_divider = value;
    update_timer();
  }
  void enable();
  void update(int ticks) {
    if (prescalar_divider < 14) {
      timer += ticks;
      if (timer >= timer_base) {
        timer -= timer_base;

        u8 bit1 = lfsr_counter & 0x01;
        u8 bit2 = (lfsr_counter >> 1) & 1;

        u8 res = bit1 ^ bit2;

        lfsr_counter >>= 1;

        if (seven_stage) {
          lfsr_counter &= 0x3f;
          lfsr_counter |= res << 6;
        } else {
          lfsr_counter &= 0x3fff;
          lfsr_counter |= res << 14;
        }

        output = (lfsr_counter & 0x1) * 15;
      }
    }
  }
  u8 volume() const { return output; }
};
}  // namespace gb
