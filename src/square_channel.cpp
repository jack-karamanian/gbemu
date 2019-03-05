#include "square_channel.h"
namespace gb {
void SquareChannel::enable() {
  enabled = true;
  volume = starting_volume;
  envelope_timer = envelope_period;
  length_tracker.enable();
}
float SquareChannel::update() {
  if (!enabled) {
    return 0;
  }
  float sample = sample_tracker.update() ? 0.01f : 0.0f;
  return sample * (volume / 16.0f);
}

void SquareChannel::sequencer_clock(int cycle) {
  if (cycle % 2 == 0) {
    enabled = length_tracker.clock();
  }

  if (cycle == 7) {
    clock_envelope();
  }
}

void SquareChannel::clock_envelope() {
  if (--envelope_timer <= 0) {
    envelope_timer = envelope_period;

    if (increase_volume && volume < 16) {
      volume++;
    }

    if (!increase_volume && volume > 0) {
      volume--;
    }
  }
}
}  // namespace gb
#if 0
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

  if (wave_progress >= duty_cycle.size()) {
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
#endif
