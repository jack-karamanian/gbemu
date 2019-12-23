#include "gba/sound.h"
#include "gba/dma.h"

namespace gb::advance {

void Sound::run_dma_transfer(u32 fifo_addr) {
  const auto dmas = m_dmas->span();
  const auto end = dmas.begin() + 3;
  const auto found_dma = std::find_if(
      dmas.begin() + 1, end,
      [fifo_addr](const auto& dma) { return dma.dest == fifo_addr; });

  if (found_dma != end) {
    // Set fixed dest
    found_dma->count = 4;
    found_dma->control().write_byte(0, 2 << 5);
    found_dma->control().write_byte(
        1, ((found_dma->control().data() >> 8) & 0xfa) | 0b1000'0100);
    found_dma->run();
  }
}

void Sound::update(u32 cycles) {
  m_fifo_timer += cycles;
  m_master_timer += cycles;

  // TODO: different sample rates
  if (m_fifo_timer >= 16777216 / 65536) {
    m_fifo_sample = fifo_a.current_sample();
    m_fifo_timer -= 16777216 / 65536;
  }

  if (m_master_timer >= 16777216 / 44100) {
    m_master_timer -= 16777216 / 44100;
    m_sample_buffer.push_back(m_fifo_sample / 1024.0F);
    m_sample_buffer.push_back(m_fifo_sample / 1024.0F);
  }
  if (m_sample_buffer.size() >= 1024 * 2) {
    m_sample_callback(m_sample_buffer);
    m_sample_buffer.clear();
  }
}

void Sound::read_fifo_sample(SoundFifo& sound_fifo, u32 addr) {
  if (sound_fifo.queued_samples() <= 16) {
    run_dma_transfer(addr);
  }
  sound_fifo.read_sample();
}

}  // namespace gb::advance
