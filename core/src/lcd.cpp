#include "cpu.h"
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "registers/lyc.h"
#include "types.h"

namespace gb {
void Lcd::check_scanlines() {
  if (scanlines == lyc) {
    stat.set_ly_equals_lyc(true);
    if (controller_enabled && stat.ly_equals_lyc_enabled()) {
      cpu->request_interrupt(Cpu::Interrupt::LcdStat);
    }
  } else {
    stat.set_ly_equals_lyc(false);
  }
}

std::tuple<bool, std::optional<Lcd::Mode>> Lcd::update(unsigned int ticks) {
  bool draw_frame = false;

  // FIXME: Some games freeze when not ticking the lcd when it's disabled.
  // For now, don't trigger any interrupts, but still tick the lcd

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
        return {draw_frame, Mode::HBlank};
      }
      break;

    case 0:
      if (lcd_ticks >= 204) {
        scanlines++;
        check_scanlines();

        if (scanlines >= 144) {
          change_mode(Mode::VBlank, [this](const Registers::LcdStat& lcd_stat) {
            if (lcd_stat.vblank_check_enabled() ||
                lcd_stat.oam_check_enabled()) {
              cpu->request_interrupt(Cpu::Interrupt::LcdStat);
            }
          });
          if (controller_enabled) {
            cpu->request_interrupt(Cpu::Interrupt::VBlank);
            gpu->render();
            draw_frame = true;
          }
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
        check_scanlines();
      }
      break;
  }

  return {draw_frame, {}};
}
}  // namespace gb
