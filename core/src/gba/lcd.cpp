#include "gba/lcd.h"
#include "gba/cpu.h"
#include "gba/dma.h"
#include "gba/gpu.h"
#include "gba/mmu.h"

namespace gb::advance {
void Lcd::increment_vcount() {
  ++vcount;

  if (dispstat.enable_lyc_interrupt() && vcount == dispstat.lyc()) {
    m_cpu->interrupts_requested.set_interrupt(Interrupt::VCountMatch, true);
  }
}

bool Lcd::update(u32 cycles) {
  bool draw_frame = false;
  m_cycles += cycles;

  switch (m_mode) {
    case Mode::Draw:
      if (m_cycles >= 960) {
        m_mode = Mode::HBlank;
        m_cycles -= 960;

        if (dispstat.enable_hblank_interrupt()) {
          m_cpu->interrupts_requested.set_interrupt(Interrupt::HBlank, true);
        }
        dispstat.set_hblank(true);

        for (Dma& dma : m_dmas->span()) {
          if (dma.control().start_timing() ==
              Dma::Control::StartTiming::HBlank) {
            dma.run();
          }
        }
      }
      break;
    case Mode::HBlank:
      if (m_cycles >= 272) {
        m_cycles -= 272;
        if (vcount <= 159) {
          m_gpu->render_scanline(vcount);
        }

        dispstat.set_hblank(false);
        if (vcount > 159) {
          for (Dma& dma : m_dmas->span()) {
            if (dma.control().start_timing() ==
                Dma::Control::StartTiming::VBlank) {
              dma.run();
            }
          }
          m_mode = Mode::VBlank;
          dispstat.set_vblank(true);
          if (dispstat.enable_vblank_interrupt()) {
            m_cpu->interrupts_requested.set_interrupt(Interrupt::VBlank, true);
          }
          draw_frame = true;

        } else {
          m_mode = Mode::Draw;
        }
        increment_vcount();
      }
      break;
    case Mode::VBlank:
      if (m_cycles >= 1232) {
        m_cycles -= 1232;
        increment_vcount();

        if (vcount > 227) {
          dispstat.set_hblank(false);
          dispstat.set_vblank(false);
          m_mode = Mode::Draw;
          vcount = 0;
        }
      }
      break;
  }
  return draw_frame;
}
}  // namespace gb::advance
