#pragma once
#include "memory.h"
#include "registers/lcd_stat.h"

#define LCD_STAT_REGISTER 0xff41
#define LCDC_Y_COORD 0xff44

namespace gb {
class Cpu;
class Memory;
class Gpu;
class Lcd {
  enum class Mode : int {
    HBlank = 0,
    VBlank = 1,
    OAMRead = 2,
    OAMVramRead = 3,
  };
  Cpu* cpu;
  Memory* memory;
  Gpu* gpu;

  int mode = 2;
  unsigned int lcd_ticks = 0;

  u8 scanlines = 0;
  u8 lyc = 0;
  Registers::LcdStat stat{0x02};
  bool controller_enabled = true;

  void check_scanlines(Registers::LcdStat& lcd_stat) const;

  template <typename... Func>
  void change_mode(Mode next_mode, Func... callback) {
    mode = static_cast<int>(next_mode);

    stat.set_mode(mode);

    check_scanlines(stat);

    if constexpr (sizeof...(callback) > 0) {
      (callback(stat), ...);
    }

    lcd_ticks = 0;
  }

 public:
  Lcd(Cpu& cpu, Memory& memory, Gpu& gpu)
      : cpu{&cpu}, memory{&memory}, gpu{&gpu} {}

  [[nodiscard]] Registers::LcdStat get_lcd_stat() const { return stat; }

  void set_lcd_stat(Registers::LcdStat lcd_stat) { stat = lcd_stat; }

  [[nodiscard]] u8 get_ly() const { return scanlines; }
  void set_ly(u8 value) { scanlines = value; }

  [[nodiscard]] u8 get_lyc() const { return lyc; }
  void set_lyc(u8 value) { lyc = value; }

  void set_enabled(bool value) { controller_enabled = value; }

  bool update(unsigned int ticks);
};
}  // namespace gb
