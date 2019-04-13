#include <iostream>
#include "cpu.h"
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "registers/lyc.h"
#include "types.h"

namespace gb {
void Lcd::check_scanlines(Registers::LcdStat& lcd_stat) const {
  const u8 lyc = memory->at(Registers::Lyc::Address);
  if (lcd_stat.ly_equals_lyc_enabled() && scanlines == lyc) {
    lcd_stat.set_ly_equals_lyc(true);
    cpu->request_interrupt(Cpu::Interrupt::LcdStat);
  } else {
    lcd_stat.set_ly_equals_lyc(false);
  }
}
bool Lcd::update(unsigned int ticks) {
  bool draw_frame = false;

  Registers::LcdStat lcd_stat{memory->get_lcd_stat()};
  lcd_stat.set_mode(mode);

  lcd_ticks += ticks;
  switch (mode) {
    case 2: {
      if (lcd_ticks >= 80) {
        check_scanlines(lcd_stat);
        mode = 3;
        lcd_stat.set_mode(mode);
        lcd_ticks = 0;
      }
      break;
    }
    case 3: {
      if (lcd_ticks >= 172) {
        mode = 0;
        lcd_stat.set_mode(mode);
        lcd_ticks = 0;
        if (lcd_stat.hblank_check_enabled()) {
          cpu->request_interrupt(Cpu::Interrupt::LcdStat);
        }
        check_scanlines(lcd_stat);
        // Render scanline
        gpu->render_scanline(scanlines);
      }
      break;
    }
    case 0: {
      if (lcd_ticks >= 204) {
        scanlines++;
        memory->set_ly(scanlines);
        lcd_ticks = 0;
        check_scanlines(lcd_stat);
        if (scanlines >= 144) {
          mode = 1;
          lcd_stat.set_mode(mode);
          if (lcd_stat.vblank_check_enabled() || lcd_stat.oam_check_enabled()) {
            cpu->request_interrupt(Cpu::Interrupt::LcdStat);
          }
          // Render image
          gpu->render();
          cpu->request_interrupt(Cpu::Interrupt::VBlank);
          draw_frame = true;
        } else {
          mode = 2;
          lcd_stat.set_mode(mode);
          if (lcd_stat.oam_check_enabled()) {
            cpu->request_interrupt(Cpu::Interrupt::LcdStat);
          }
        }
      }
      break;
    }
    case 1: {
      if (lcd_ticks >= 456) {
        scanlines++;
        lcd_ticks = 0;

        if (scanlines > 153) {
          mode = 2;
          lcd_stat.set_mode(mode);
          scanlines = 0;
          check_scanlines(lcd_stat);
          if (lcd_stat.oam_check_enabled()) {
            cpu->request_interrupt(Cpu::Interrupt::LcdStat);
          }
        }
        memory->set_ly(scanlines);
      }
      break;
    }
  }

  memory->set_lcd_stat(lcd_stat.get_value());
  return draw_frame;
}
}  // namespace gb
