#include "envelope_mod.h"

namespace gb {
void EnvelopeMod::enable() {
  volume = initial_starting_volume;
  timer = period;
}
void EnvelopeMod::clock(int step) {
  if (step == 7 && --timer <= 0) {
    timer = period;

    if (increase_volume && volume < 16) {
      volume++;
    }

    if (!increase_volume && volume > 0) {
      volume--;
    }
  }
}

float EnvelopeMod::update(float input_volume) const {
  float result = (volume / 16.0f) * input_volume;
  return result;
}
}  // namespace gb