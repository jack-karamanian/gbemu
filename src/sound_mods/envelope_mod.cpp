#include "envelope_mod.h"

namespace gb {
void EnvelopeMod::enable() {
  volume = starting_volume;
  timer = period;
}
void EnvelopeMod::clock(int step) {
  if (step == 7 && --timer <= 0) {
    timer = period;
    if (period != 0) {
      if (increase_volume && volume < 16) {
        volume++;
      }

      if (!increase_volume && volume > 0) {
        volume--;
      }
    }
  }
}

}  // namespace gb
