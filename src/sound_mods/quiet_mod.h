#pragma once

struct QuietMod {
  float update(float volume) const { return (volume / 16.0f) * 0.02f; }
  void enable() {}
  void clock(int step) { static_cast<void>(step); }
};
