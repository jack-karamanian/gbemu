#pragma once
#include <array>
#include <functional>
#include <utility>
#include "sound_length.h"

namespace gb {
class SquareChannel {
  bool enabled = false;

  int envelope_period = 0;
  int envelope_timer = 0;
  int starting_volume = 0;
  int volume = 0;
  bool increase_volume = false;

 public:
  SampleTracker<8, bool> sample_tracker;
  LengthTracker<64> length_tracker;

  void set_starting_volume(int value) { starting_volume = value; }
  void set_increase_volume(bool value) { increase_volume = value; }
  void set_envelope_period(int value) {
    envelope_period = value == 0 ? 8 : value;
  }

  bool is_enabled() const { return enabled; }

  void enable();

  void disable() { enabled = false; }

  float update();

  void sequencer_clock(int cycle);

  void clock_length() { enabled = length_tracker.clock(); }

  void clock_envelope();
};
}  // namespace gb
