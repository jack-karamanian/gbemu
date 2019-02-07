#include <iostream>
#include "cpu.h"
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "registers/lcd_stat.h"
#include "registers/lyc.h"
#include "types.h"

namespace gb {
void Lcd::update(unsigned int ticks) {
  Registers::LcdStat lcd_stat{memory->at(Registers::LcdStat::Address)};
  const u8 lyc = memory->at(Registers::Lyc::Address);

  lcd_ticks += ticks;
  switch (mode) {
    case 2: {
      if (lcd_ticks >= 80) {
        mode = 3;
        lcd_ticks = 0;
      }
      break;
    }
    case 3: {
      if (lcd_ticks >= 172) {
        mode = 0;
        lcd_ticks = 0;
        if (lcd_stat.hblank_check_enabled()) {
          cpu->request_interrupt(Cpu::Interrupt::LcdStat);
        }
        // Render scanline
        gpu->render_scanline(scanlines);
      }
      break;
    }
    case 0: {
      if (lcd_ticks >= 204) {
        scanlines++;
        lcd_ticks = 0;
        if (scanlines >= 144) {
          mode = 1;
          if (lcd_stat.vblank_check_enabled() || lcd_stat.oam_check_enabled()) {
            cpu->request_interrupt(Cpu::Interrupt::LcdStat);
          }
          // Render image
          gpu->render();
          cpu->request_interrupt(Cpu::Interrupt::VBlank);
        } else {
          mode = 2;
          if (lcd_stat.oam_check_enabled()) {
            cpu->request_interrupt(Cpu::Interrupt::LcdStat);
          }
        }
      }
      break;
    }
    case 1: {
      if (lcd_ticks > 456) {
        scanlines++;
        lcd_ticks = 0;

        if (scanlines > 153) {
          mode = 2;
          scanlines = 0;
        }
      }
      break;
    }
  }

  if (lcd_stat.ly_equals_lyc_enabled() && scanlines == lyc) {
    lcd_stat.set_ly_equals_lyc(true);
    cpu->request_interrupt(Cpu::Interrupt::LcdStat);
  } else {
    lcd_stat.set_ly_equals_lyc(false);
  }

  lcd_stat.set_mode(mode);

  memory->set(Registers::LcdStat::Address, lcd_stat.get_value());
  memory->set(LCDC_Y_COORD, scanlines);
}
void Lcd::write_lcd_stat() const {
  // memory->set(LCD_STAT_REGISTER, mode);
}
}  // namespace gb
