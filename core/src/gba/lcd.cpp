#include "lcd.h"

namespace gb::advance {
void Lcd::update(u32 cycles) {
  m_cycles += cycles;

  switch (m_mode) {
    case Mode::Draw:
      if (m_cycles >= 960) {
        m_mode = Mode::HBlank;
        m_cycles = 0;

        dispstat.set_hblank(true);
      }
      break;
    case Mode::HBlank:
      if (m_cycles >= 272) {
        m_cycles = 0;
        ++vcount;

        // m_mode = vcount > 160 ? Mode::VBlank : Mode::Draw;

        dispstat.set_hblank(false);
        if (vcount > 160) {
          m_mode = Mode::VBlank;
          dispstat.set_vblank(true);
        } else {
          m_mode = Mode::Draw;
        }
      }
      break;
    case Mode::VBlank:
      printf("VBLANK\n");
      if (m_cycles >= 1232) {
        m_cycles = 0;
        ++vcount;

        if (vcount >= 168) {
          dispstat.set_hblank(false);
          dispstat.set_vblank(false);
          m_mode = Mode::Draw;
          vcount = 0;
        }
      }
      break;
  }
}
}  // namespace gb::advance
