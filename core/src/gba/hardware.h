#pragma once

namespace gb::advance {
class Lcd;
class Cpu;
class Input;
class Mmu;
struct Timers;
class Dmas;
class Gpu;
class Sound;

struct Hardware {
  Cpu* cpu = nullptr;
  Lcd* lcd = nullptr;
  Input* input = nullptr;
  Mmu* mmu = nullptr;
  Timers* timers = nullptr;
  Dmas* dmas = nullptr;
  Gpu* gpu = nullptr;
  Sound* sound = nullptr;
};
}  // namespace gb::advance
