#include <iostream>
#include "noise_source.h"

namespace gb {
void NoiseSource::enable() {
  lfsr_counter = 0xffff;
  timer = 0;
  timer_base = timers.at(clock_divisor).at(prescalar_divider);
}
u8 NoiseSource::update() {
  if (++timer >= timer_base) {
    u8 bit1 = lfsr_counter & 0x01;
    u8 bit2 = (lfsr_counter >> 1) & 1;

    u8 res = bit1 ^ bit2;

    lfsr_counter >>= 1;

    lfsr_counter |= res << 14;
    if (seven_stage) {
      lfsr_counter &= ~0x40;
      lfsr_counter |= res << 6;
    } else {
    }

    timer = 0;
  }

  return (lfsr_counter & 0x1) != 0 ? 0 : 15;
}
}  // namespace gb
