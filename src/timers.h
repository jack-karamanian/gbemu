#pragma once
#include "types.h"

namespace gb {
class Memory;
class Timers {
  Memory* memory;
  u8 prev_timer = 0;
  int total_ticks = 0;
  int div_ticks = 0;

 public:
  Timers(Memory& memory) : memory{&memory} {}

  bool update(int ticks);
};
}  // namespace gb
