#pragma once

#include "system.h"

#define LCDC_REGISTER 0xff41
#define LCDC_Y_COORD 0xff44

namespace gb {
struct Lcd {
  enum class Mode {
    HBlank = 0,
    VBlank = 1,
    OAMRead = 2,
    OAMVramRead = 3,
  };
  const System* system;

  int mode = 2;
  int scanlines = 0;
  unsigned int lcd_ticks = 0;

  Lcd(const System* system) : system{system} {}
  void update(unsigned int ticks);
  void write_lcdc();
};
}  // namespace gb
