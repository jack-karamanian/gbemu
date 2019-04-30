#include <iostream>
#include "noise_source.h"

namespace gb {
void NoiseSource::enable() {
  update_timer();
  lfsr_counter = seven_stage ? 0x7f : 0x7fff;
  counter = 0;
  timer = 0;
}

}  // namespace gb
