#include "gba/cpu.h"
#include "gba/dma.h"
#include "gba/gpu.h"
#include "gba/mmu.h"
#include "lcd.h"

namespace gb::advance {
void Lcd::update(u32 cycles) {
  m_cycles += cycles;

  switch (m_mode) {
    case Mode::Draw:
      if (m_cycles >= 960) {
        m_mode = Mode::HBlank;
        m_cycles = 0;

        m_cpu->interrupts_requested.set_interrupt(Interrupt::HBlank, true);
        dispstat.set_hblank(true);

        for (Dma& dma : m_dmas->span()) {
          if (dma.control().start_timing() ==
              Dma::Control::StartTiming::HBlank) {
            fmt::print("START HBLANK DMA\n");
            dma.run();
          }
        }
      }
      break;
    case Mode::HBlank:
      if (m_cycles >= 272) {
        m_gpu->render_scanline(vcount);
        m_cycles = 0;
        ++vcount;

        // m_mode = vcount > 160 ? Mode::VBlank : Mode::Draw;

        dispstat.set_hblank(false);
        if (vcount > 160) {
          for (Dma& dma : m_dmas->span()) {
            if (dma.control().start_timing() ==
                Dma::Control::StartTiming::VBlank) {
              fmt::print("START VBLANK DMA\n");
              dma.run();
            }
          }
          m_mode = Mode::VBlank;
          dispstat.set_vblank(true);
          m_cpu->interrupts_requested.set_interrupt(Interrupt::VBlank, true);
        } else {
          m_mode = Mode::Draw;
        }
      }
      break;
    case Mode::VBlank:
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
