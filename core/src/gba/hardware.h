#pragma once

namespace gb::advance {
class Lcd;
class Cpu;
class Input;
class Mmu;
struct Hardware {
  Cpu* cpu = nullptr;
  Lcd* lcd = nullptr;
  Input* input = nullptr;
  Mmu* mmu = nullptr;
};
}  // namespace gb::advance
