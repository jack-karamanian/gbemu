#include <iostream>
#include "noise_source.h"

namespace gb {
void NoiseSource::enable() {
  lfsr_counter = 0xffff;
  timer = 0;

  update_timer();
}

}  // namespace gb
