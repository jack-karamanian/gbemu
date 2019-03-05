#pragma once

#include <nonstd/span.hpp>
#include "types.h"

namespace gb {
class WaveSource {
  bool enabled = false;
  int timer = 0;
  int timer_base = 0;

  int wave_progress = 0;

  nonstd::span<const u8> wave_buffer;

 public:
  WaveSource(nonstd::span<const u8> buffer) : wave_buffer{std::move(buffer)} {}

  void set_timer_base(int frequency) { timer_base = (2048 - frequency) * 2; }

  void enable();

  u8 get_sample();

  u8 update();
};
}  // namespace gb
