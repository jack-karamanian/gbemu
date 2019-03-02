#include <iostream>
#include "square_channel.h"

namespace gb {
void SquareChannel::enable() {
  enabled = true;
  if (length_counter == 0) {
    length_counter = 64;
  }
}
float SquareChannel::update() {
  if (!enabled) {
    return 0;
  }
  if (--timer <= 0) {
    timer = timer_base;
    wave_progress++;
  }

  if (wave_progress > 7) {
    wave_progress = 0;
  }

  return duty_cycle.at(wave_progress) ? 0.1f : 0.0f;
}

void SquareChannel::clock_length() {
  if (--length_counter <= 0) {
    enabled = false;
    length_counter = 0;
  }
}
}  // namespace gb
