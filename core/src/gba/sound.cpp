#include "gba/sound.h"
#include <SDL2/SDL_audio.h>
#include <fmt/printf.h>
#include "gba/dma.h"

namespace gb::advance {

void Sound::run_dma_transfer(u32 fifo_addr) {
  const auto dmas = m_dmas->span();
  const auto end = dmas.begin() + 3;
  const auto found_dma =
      std::find_if(dmas.begin() + 1, end, [fifo_addr](const auto& dma) {
        return dma.dest == fifo_addr && dma.control().start_timing() ==
                                            Dma::Control::StartTiming::Special;
      });

  if (found_dma != end) {
    // Set fixed dest and 4 * 4 byte transfer
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
  if (m_fifo_timer >= 16777216 / 32768) {
    m_fifo_sample = fifo_a.current_sample();
    m_fifo_timer -= 16777216 / 32768;
  }

  if (m_master_timer >= 16777216 / 44100) {
    m_master_timer -= 16777216 / 44100;
    float mixed_sample = 0;
    const float sample_a = fifo_a.current_sample() / 1024.0f;
    const float sample_b = fifo_b.current_sample() / 1024.0f;
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<const Uint8*>(&sample_a), AUDIO_F32SYS,
                       sizeof(float), 50);
    SDL_MixAudioFormat(reinterpret_cast<Uint8*>(&mixed_sample),
                       reinterpret_cast<const Uint8*>(&sample_b), AUDIO_F32SYS,
                       sizeof(float), 50);
    m_sample_buffer.push_back(sample_a);
    m_sample_buffer.push_back(sample_b);
  }
  if (m_sample_buffer.size() >= 1024) {
    m_sample_callback(m_sample_buffer);
    m_sample_buffer.clear();
  }
}

void Sound::read_fifo_sample(SoundFifo& sound_fifo, u32 addr) {
  if (sound_fifo.queued_samples() <= 16) {
    run_dma_transfer(addr);
  }
  if (sound_fifo.queued_samples() > 0) {
    sound_fifo.read_sample();
  }
}

}  // namespace gb::advance
