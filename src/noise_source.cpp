#include <iostream>
#include "noise_source.h"

namespace gb {
void NoiseSource::enable() {
  lfsr_counter = 0xffff;
  timer = 0;
  timer_base = timers.at(clock_divisor).at(prescalar_divider);
}

}  // namespace gb
