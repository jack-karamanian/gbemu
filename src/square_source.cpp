#include "square_source.h"

namespace gb {
void SquareSource::enable() {
  timer = timer_base;
}

u8 SquareSource::update() {
  if (--timer <= 0) {
    timer = timer_base;
    wave_progress++;
    if (wave_progress > 7) {
      wave_progress = 0;
    }
  }

  return (duty_cycle & (1 << wave_progress)) != 0 ? 15 : 0;
}
}  // namespace gb
