#pragma once

#include "system.h"
namespace gb {
struct Lcd {
  const System* system;

  Lcd(const System* system) : system{system} {}
  void update(int ticks);
  void write_lcdc(Memory& memory);
};
}  // namespace gb
