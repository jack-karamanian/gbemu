#pragma once
#include <array>
#include "types.h"

constexpr int FREQUENCY = 4194304 / 8;

namespace gb {
template <typename T>
constexpr auto pow(T i, T n) {
  if (n == 0) {
    return 1;
  } else {
    return i * pow(i, n - 1);
  }
}

constexpr std::array<int, 8> clock_divisors = {
#if 1
    FREQUENCY * 2, FREQUENCY,     FREQUENCY / 2, FREQUENCY / 3,
    FREQUENCY / 4, FREQUENCY / 5, FREQUENCY / 6, FREQUENCY / 7,
#else
    8,  16, 32, 48, 64,
    80, 96, 112
#endif
};

constexpr std::array<std::array<int, 14>, 8> gen_timers() {
  std::array<std::array<int, 14>, 8> res = {};

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 14; j++) {
      res[i][j] = 4194304 / (clock_divisors[i] / pow(2, j + 1));
    }
  }

  return res;
}

class NoiseSource {
  constexpr static std::array<std::array<int, 14>, 8> timers = gen_timers();

  // 0 - 7
  int clock_divisor = 0;

  // 0 - 13
  int prescalar_divider = 0;

  int timer = 0;

  int timer_base = 0;

  u16 lfsr_counter = 0;

  bool seven_stage = true;

  u8 output = 0;

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
        u8 bit1 = lfsr_counter & 0x01;
        u8 bit2 = (lfsr_counter >> 1) & 1;

        u8 res = bit1 ^ bit2;

        lfsr_counter >>= 1;

        lfsr_counter |= res << 14;
        if (seven_stage) {
          lfsr_counter &= ~0x40;
          lfsr_counter |= res << 6;
        }

        output = (lfsr_counter & 0x1) != 0 ? 0 : 15;
        timer -= timer_base;
      }
    }
  }
  u8 volume() const { return output; }
};
}  // namespace gb
