#pragma once
#include <memory>

namespace gb {
struct Cpu;
struct Memory;
struct Lcd;
struct Gpu;

struct System {
  std::unique_ptr<Cpu> cpu;
  std::unique_ptr<Memory> memory;
  std::unique_ptr<Lcd> lcd;
  std::unique_ptr<Gpu> gpu;

  System();
};
}  // namespace gb
