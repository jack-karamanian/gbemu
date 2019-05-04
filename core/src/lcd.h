#pragma once
#include <optional>
#include "memory.h"
#include "registers/lcd_stat.h"

namespace gb {
class Cpu;
class Gpu;
class Lcd {
 public:
  enum class Mode : int {
    HBlank = 0,
    VBlank = 1,
    OAMRead = 2,
    OAMVramRead = 3,
  };
  Lcd(Cpu& cpu, Gpu& gpu) : cpu{&cpu}, gpu{&gpu} {}

  [[nodiscard]] Registers::LcdStat get_lcd_stat() const { return stat; }

  void set_lcd_stat(Registers::LcdStat lcd_stat) { stat = lcd_stat; }

  [[nodiscard]] u8 get_ly() const {
    return !controller_enabled ? 0 : scanlines;
  }
  void set_ly(u8 value) { scanlines = value; }

  [[nodiscard]] u8 get_lyc() const { return lyc; }
  void set_lyc(u8 value) {
    lyc = value;
    if (controller_enabled) {
      check_scanlines();
    }
  }

  void set_enabled(bool value) {
    controller_enabled = value;
    if (!controller_enabled) {
      scanlines = 0;
      mode = 2;
      lcd_ticks = 0;
      stat.set_mode(2);
      check_scanlines();
    }
  }

  std::tuple<bool, std::optional<Lcd::Mode>> update(unsigned int ticks);

 private:
  Cpu* cpu;
  Gpu* gpu;

  int mode = 2;
  unsigned int lcd_ticks = 0;

  u8 scanlines = 0;
  u8 lyc = 0;
  Registers::LcdStat stat{0x02};
  bool controller_enabled = true;

  void check_scanlines();

  template <typename... Func>
  void change_mode(Mode next_mode, Func... callback) {
    mode = static_cast<int>(next_mode);

    stat.set_mode(mode);

    if constexpr (sizeof...(callback) > 0) {
      if (controller_enabled) {
        (callback(stat), ...);
      }
    }

    lcd_ticks = 0;
  }
};
}  // namespace gb
