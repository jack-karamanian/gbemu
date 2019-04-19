#include <iostream>
#include "cpu.h"
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "registers/lyc.h"
#include "types.h"

namespace gb {
void Lcd::check_scanlines(Registers::LcdStat& lcd_stat) const {
  if (lcd_stat.ly_equals_lyc_enabled() && scanlines == lyc) {
    lcd_stat.set_ly_equals_lyc(true);
    cpu->request_interrupt(Cpu::Interrupt::LcdStat);
  } else {
    lcd_stat.set_ly_equals_lyc(false);
  }
}

bool Lcd::update(unsigned int ticks) {
  bool draw_frame = false;

  if (!controller_enabled) {
    return false;
  }

  lcd_ticks += ticks;
  switch (mode) {
    case 2: {
      if (lcd_ticks >= 80) {
        change_mode(Mode::OAMVramRead);
      }
      break;
    }
    case 3:
      if (lcd_ticks >= 172) {
        change_mode(Mode::HBlank, [this](const Registers::LcdStat& lcd_stat) {
          if (lcd_stat.hblank_check_enabled()) {
            cpu->request_interrupt(Cpu::Interrupt::LcdStat);
          }
        });
        gpu->render_scanline(scanlines);
      }
      break;

    case 0:
      if (lcd_ticks >= 204) {
        scanlines++;

        if (scanlines >= 144) {
          change_mode(Mode::VBlank, [this](const Registers::LcdStat& lcd_stat) {
            if (lcd_stat.vblank_check_enabled() ||
                lcd_stat.oam_check_enabled()) {
              cpu->request_interrupt(Cpu::Interrupt::LcdStat);
            }
          });
          cpu->request_interrupt(Cpu::Interrupt::VBlank);
          gpu->render();
          draw_frame = true;
        } else {
          change_mode(Mode::OAMRead,
                      [this](const Registers::LcdStat& lcd_stat) {
                        if (lcd_stat.oam_check_enabled()) {
                          cpu->request_interrupt(Cpu::Interrupt::LcdStat);
                        }
                      });
        }
      }
      break;
    case 1:
      if (lcd_ticks >= 456) {
        scanlines++;
        lcd_ticks = 0;

        if (scanlines > 153) {
          scanlines = 0;
          change_mode(Mode::OAMRead,
                      [this](const Registers::LcdStat& lcd_stat) {
                        if (lcd_stat.oam_check_enabled()) {
                          cpu->request_interrupt(Cpu::Interrupt::LcdStat);
                        }
                      });
        }
      }
      break;
  }

  return draw_frame;
}
}  // namespace gb
