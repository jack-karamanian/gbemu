#pragma once
#include "types.h"

namespace gb {
class Memory;
class Timers {
  Memory* memory;
  u16 internal_counter = 0;

  int timer_ticks = 0;

 public:
  Timers(Memory& memory) : memory{&memory} {}

  void handle_memory_write(u16 addr, u8 value);
  bool update(int ticks);
};
}  // namespace gb
