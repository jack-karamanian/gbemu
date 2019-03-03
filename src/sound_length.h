#pragma once
#include <array>
#include <nonstd/span.hpp>

namespace gb {
template <int DefaultLength>
class LengthTracker {
  int length_counter = 0;
  bool length_enabled = false;

 public:
  void enable() {
    if (length_counter == 0) {
      length_counter = DefaultLength;
    }
  }

  void set_length(int length) { length_counter = DefaultLength - length; }

  void set_length_enabled(bool enabled) { length_enabled = enabled; }

  bool clock() {
    if (length_enabled && --length_counter <= 0) {
      length_counter = 0;
      return false;
    }
    return true;
  }
};

template <int NumSamples, typename SampleType>
class SampleTracker {
  // Timer/frequency
  int timer_base = 0;
  int timer = 0;

  int wave_progress = 0;

  std::array<SampleType, NumSamples> duty_cycle;

 public:
  int get_wave_progress() const { return wave_progress; }

  int get_timer_base() const { return -(timer_base / 4) + 2048; }

  void set_timer_base(int value) { timer_base = (2048 - value) * 4; }

  void set_duty_cycle(const std::array<SampleType, NumSamples>& value) {
    duty_cycle = value;
  }

  SampleType update() {
    if (--timer <= 0) {
      timer = timer_base;
      wave_progress++;
    }

    if (wave_progress >= NumSamples) {
      wave_progress = 0;
    }

    return duty_cycle.at(wave_progress);
  }
};
}  // namespace gb
