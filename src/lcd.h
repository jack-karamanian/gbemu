#pragma once

#define LCDC_REGISTER 0xff41
#define LCDC_Y_COORD 0xff44

namespace gb {
struct Cpu;
struct Memory;
class Lcd {
  enum class Mode {
    HBlank = 0,
    VBlank = 1,
    OAMRead = 2,
    OAMVramRead = 3,
  };
  Cpu* cpu;
  Memory* memory;

  int mode = 2;
  int scanlines = 0;
  unsigned int lcd_ticks = 0;

 public:
  Lcd(Cpu& cpu, Memory& memory) : cpu{&cpu}, memory{&memory} {}
  void update(unsigned int ticks);
  void write_lcdc() const;
};
}  // namespace gb
