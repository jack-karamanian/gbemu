#pragma once

#include <nonstd/span.hpp>
#include "types.h"

namespace gb {
class WaveSource {
  nonstd::span<const u8> wave_buffer;
  int timer = 0;
  int timer_base = 0;

  int wave_progress = 0;

  bool enabled = false;
  u8 output = 0;

  u8 get_volume(int progress) const;

 public:
  WaveSource(nonstd::span<const u8> buffer) : wave_buffer{std::move(buffer)} {}

  void set_timer_base(int frequency) { timer_base = (2048 - frequency) * 2; }

  void enable();

  void update() {
    if (--timer <= 0) {
      timer = timer_base;
      if (++wave_progress >= 32) {
        wave_progress = 0;
      }
      output = get_volume(wave_progress);
    }
  }

  u8 volume() const { return enabled ? output : 0; }
};
}  // namespace gb
