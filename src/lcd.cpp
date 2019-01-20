#include <iostream>
#include "cpu.h"
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "types.h"

namespace gb {
void Lcd::update(unsigned int ticks) {
  write_lcdc();
  lcd_ticks += ticks;
  std::cout << "mode: " << mode << std::endl;
  switch (mode) {
    case 2: {
      std::cout << "ticks: " << std::dec << lcd_ticks << std::endl;
      if (lcd_ticks >= 80) {
        mode = 3;
        std::cout << "to mode 3" << std::endl;
        lcd_ticks = 0;
      }
      break;
    }
    case 3: {
      if (lcd_ticks >= 172) {
        mode = 0;
        std::cout << "to mode 0" << std::endl;
        lcd_ticks = 0;
        gpu->render_scanline(scanlines);
      }
      // Render scanline

      break;
    }
    case 0: {
      if (lcd_ticks >= 204) {
        scanlines++;
        lcd_ticks = 0;
        if (scanlines >= 144) {
          mode = 1;
          std::cout << "VBLANK" << std::endl;
          gpu->render();
          cpu->request_interrupt(Cpu::Interrupt::VBlank);
        } else {
          mode = 2;
          // Render image
        }
      }
      break;
    }
    case 1: {
      if (lcd_ticks > 456) {
        scanlines++;
        lcd_ticks = 0;

        if (scanlines > 154) {
          mode = 2;
          scanlines = 0;
        }
      }
      break;
    }
  }
}
void Lcd::write_lcdc() const {
  u8* lcdc = memory->at(LCDC_REGISTER);
  *lcdc = (u8)mode;

  u8* lcdc_y = memory->at(LCDC_Y_COORD);
  *lcdc_y = scanlines;
}
}  // namespace gb
