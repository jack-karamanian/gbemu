#pragma once
#include "utils.h"

namespace gb::advance {
class Cpu;
class Mmu;
class Dmas;
class Gpu;

class DispStat : public Integer<u16> {
 public:
  using Integer::Integer;

  void write_byte(unsigned int byte, u8 value) {
    // Don't allow the first 3 bits to be written
    Integer::write_byte(
        byte, byte == 0 ? ((value & ~0b111) | (m_value & 0b111)) : value);
  }

  [[nodiscard]] bool vblank() const { return test_bit(0); }

  void set_vblank(bool set) { set_bit(0, set); }

  void set_hblank(bool set) { set_bit(1, set); }

  void set_vcount_equals_lyc(bool set) { set_bit(2, set); }

  [[nodiscard]] bool enable_vblank_interrupt() const { return test_bit(3); }

  [[nodiscard]] bool enable_hblank_interrupt() const { return test_bit(4); }

  [[nodiscard]] bool enable_lyc_interrupt() const { return test_bit(5); }

  [[nodiscard]] unsigned int lyc() const { return (m_value >> 8) & 0xff; }
};
class Lcd {
 public:
  enum class Mode { Draw, HBlank, VBlank };

  Lcd(Cpu& cpu, Dmas& dmas, Gpu& gpu)
      : m_cpu{&cpu}, m_dmas{&dmas}, m_gpu{&gpu} {}

  DispStat dispstat{0};
  u32 vcount = 0;

  bool update(u32 cycles, int& next_event_cycles);

 private:
  void increment_vcount();

  int m_cycles = 0;
  int m_next_event_cycles = 960;
  Mode m_mode = Mode::Draw;
  Cpu* m_cpu;
  Dmas* m_dmas;
  Gpu* m_gpu;
};
}  // namespace gb::advance
