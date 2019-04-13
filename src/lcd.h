#pragma once
#include "registers/lcd_stat.h"

#define LCD_STAT_REGISTER 0xff41
#define LCDC_Y_COORD 0xff44

namespace gb {
class Cpu;
class Memory;
class Gpu;
class Lcd {
  enum class Mode {
    HBlank = 0,
    VBlank = 1,
    OAMRead = 2,
    OAMVramRead = 3,
  };
  Cpu* cpu;
  Memory* memory;
  Gpu* gpu;

  int mode = 2;
  int scanlines = 0;
  unsigned int lcd_ticks = 0;

  void check_scanlines(Registers::LcdStat& lcd_stat) const;

 public:
  Lcd(Cpu& cpu, Memory& memory, Gpu& gpu)
      : cpu{&cpu}, memory{&memory}, gpu{&gpu} {}
  bool update(unsigned int ticks);
};
}  // namespace gb
