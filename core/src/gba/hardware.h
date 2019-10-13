#pragma once

namespace gb::advance {
class Lcd;
class Cpu;
class Input;
class Mmu;
struct Timers;
class Dmas;
class Gpu;

struct Hardware {
  Cpu* cpu = nullptr;
  Lcd* lcd = nullptr;
  Input* input = nullptr;
  Mmu* mmu = nullptr;
  Timers* timers = nullptr;
  Dmas* dmas = nullptr;
  Gpu* gpu = nullptr;
};
}  // namespace gb::advance
