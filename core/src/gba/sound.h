#pragma once
#include <functional>
#include <vector>
#include "io_registers.h"
#include "ring_buffer.h"
#include "utils.h"

namespace gb::advance {
class SoundFifo {
 public:
  void write_byte([[maybe_unused]] unsigned int byte, u8 value) {
    if (m_sample_buffer.size() >= m_sample_buffer.capacity()) {
      m_sample_buffer.next();
    }
    m_sample_buffer.push_back(value);
  }

  [[nodiscard]] std::size_t size_bytes() const noexcept { return 4; }

  [[nodiscard]] nonstd::span<u8> byte_span() const noexcept {
    return nonstd::span<u8>{nullptr,
                            static_cast<nonstd::span<u8>::index_type>(0)};
  }

  void clear() noexcept { m_sample_buffer.clear(); }

  void on_after_write() const noexcept {}

  void read_sample() { m_current_sample = m_sample_buffer.next(); }

  [[nodiscard]] s8 current_sample() const noexcept { return m_current_sample; }

  [[nodiscard]] std::size_t queued_samples() const noexcept {
    return m_sample_buffer.size();
  }

 private:
  s8 m_current_sample = 0;
  RingBuffer<s8, 32> m_sample_buffer;
};

class SoundcntLow : public Integer<u16> {
 public:
  constexpr SoundcntLow() : Integer::Integer{0} {}
};

class SoundcntHigh : public Integer<u16> {
 public:
  constexpr SoundcntHigh(SoundFifo& fifo_a, SoundFifo& fifo_b)
      : Integer::Integer{0}, m_fifo_a{&fifo_a}, m_fifo_b{&fifo_b} {}

  void write_byte(unsigned int byte, u8 value) {
    if (byte == 1) {
      if (gb::test_bit(value, 3)) {
        m_fifo_a->clear();
      }
      if (gb::test_bit(value, 7)) {
        m_fifo_b->clear();
      }
    }
    Integer::write_byte(byte, value);
  }

  [[nodiscard]] int dma_sound_a_timer() const { return (m_value >> 10) & 1; }

  [[nodiscard]] int dma_sound_b_timer() const { return (m_value >> 14) & 1; }

 private:
  SoundFifo* m_fifo_a;
  SoundFifo* m_fifo_b;
};

class Dmas;

class Sound {
 public:
  using SampleType = float;
  Sound(std::function<void(nonstd::span<SampleType>)> sample_callback,
        Dmas& dmas)
      : m_sample_callback{std::move(sample_callback)}, m_dmas{&dmas} {
    m_sample_buffer.reserve(1024);
  }
  SoundFifo fifo_a;
  SoundFifo fifo_b;
  u32 soundbias = 0x200;

  SoundcntHigh soundcnt_high{fifo_a, fifo_b};

  void run_dma_transfer(u32 fifo_addr);

  void read_fifo_a_sample() { read_fifo_sample(fifo_a, hardware::FIFO_A); }
  void read_fifo_b_sample() { read_fifo_sample(fifo_b, hardware::FIFO_B); }

  void update(u32 cycles, int& next_event_cycles);

 private:
  void read_fifo_sample(SoundFifo& sound_fifo, u32 addr);
  std::vector<SampleType> m_sample_buffer{};
  std::function<void(nonstd::span<SampleType>)> m_sample_callback;
  Dmas* m_dmas;
  u32 m_fifo_timer = 0;
  u32 m_master_timer = 0;
};
}  // namespace gb::advance
