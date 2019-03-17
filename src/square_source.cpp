#include "square_source.h"

namespace gb {
int SquareSource::calculate_next_frequency(int freq) const {
  freq += (freq >> sweep_shift) * (sweep_negate ? -1 : 1);
  return freq;
}

void SquareSource::overflow_check(int freq) {
  enabled = !is_overflowed(freq) &&
            sweep_shift != 0;  // TODO: disable if period == 0?;
}

void SquareSource::sweep_frequency() {
  int next_frequency = calculate_next_frequency(frequency);
  set_frequency(next_frequency);
  overflow_check(next_frequency);

  next_frequency = calculate_next_frequency(next_frequency);
  overflow_check(next_frequency);
}

void SquareSource::clock_sweep() {
  if (enabled && sweep_period != 0 && --sweep_timer <= 0) {
    sweep_timer = sweep_period;
    sweep_frequency();
  }
}

void SquareSource::enable() {
  enabled = true;
  set_frequency(staged_timer_base);
  timer = timer_base;

  sweep_timer = sweep_period;
  if (sweep_enabled && sweep_shift != 0) {
    int next_frequency = calculate_next_frequency(frequency);
    // enabled = !is_overflowed(next_frequency);
    overflow_check(next_frequency);
  }
}

u8 SquareSource::update() {
  if (--timer <= 0) {
    timer = timer_base;
    wave_progress++;
    if (wave_progress > 7) {
      wave_progress = 0;
    }
    output = enabled && (duty_cycle & (1 << wave_progress)) != 0 ? 15 : 0;
  }

  return output;
}
}  // namespace gb
