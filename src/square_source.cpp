#include "square_source.h"

namespace gb {
u8 SquareSource::update() {
  if (--timer <= 0) {
    timer = timer_base;
    wave_progress++;
  }

  if (wave_progress > 7) {
    wave_progress = 0;
  }

  return DUTY_CYCLES.at(duty_cycle).at(wave_progress) ? 15 : 0;
}
}  // namespace gb
