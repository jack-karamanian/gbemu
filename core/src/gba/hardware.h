#pragma once

namespace gb::advance {
class Lcd;
class Cpu;
class Input;
class Mmu;
struct Timers;
struct Dmas;

struct Hardware {
  Cpu* cpu = nullptr;
  Lcd* lcd = nullptr;
  Input* input = nullptr;
  Mmu* mmu = nullptr;
  Timers* timers = nullptr;
  Dmas* dmas = nullptr;
};
}  // namespace gb::advance
