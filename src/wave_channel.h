#pragma once
#include "sound_length.h"
#include "types.h"

namespace gb {
class WaveChannel {
  bool enabled = false;
  int volume_shift = 0;

  nonstd::span<const u8> wave_buffer;

  // Timer/frequency
  int timer_base = 0;
  int timer = 0;

  int wave_progress = 0;

  u8 get_sample();

 public:
  WaveChannel(nonstd::span<const u8> wave_buffer) : wave_buffer{wave_buffer} {}
  LengthTracker<256> length_tracker;

  void set_timer_base(int frequency) { timer_base = (2048 - frequency) * 2; }

  void set_volume_shift(int code) {
    switch (code) {
      case 0:
        volume_shift = 4;
        break;
      case 1:
        volume_shift = 0;
        break;
      case 2:
        volume_shift = 1;
        break;
      case 3:
        volume_shift = 2;
        break;
    }
  }

  void enable() {
    enabled = true;
    length_tracker.enable();
    wave_progress = 0;
    timer = 0;
  }

  void disable() { enabled = false; }

  float update();

  void clock_length() { enabled = length_tracker.clock(); }
};
}  // namespace gb
