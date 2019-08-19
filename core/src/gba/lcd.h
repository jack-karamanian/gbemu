#pragma once
#include "utils.h"

namespace gb::advance {
class Cpu;
class Mmu;
class Dmas;
class Gpu;
class DispStat : public Integer<u32> {
 public:
  using Integer::Integer;

  [[nodiscard]] bool vblank() const { return test_bit(0); }

  void set_vblank(bool set) { set_bit(0, set); }

  void set_hblank(bool set) { set_bit(1, set); }
};
class Lcd {
 public:
  enum class Mode { Draw, HBlank, VBlank };

  Lcd(Cpu& cpu, Mmu& mmu, Dmas& dmas, Gpu& gpu)
      : m_cpu{&cpu}, m_mmu{&mmu}, m_dmas{&dmas}, m_gpu{&gpu} {}

  DispStat dispstat{0};
  u32 vcount = 0;

  void update(u32 cycles);

 private:
  int m_cycles = 0;
  Mode m_mode = Mode::Draw;
  Cpu* m_cpu;
  Mmu* m_mmu;
  Dmas* m_dmas;
  Gpu* m_gpu;
};
}  // namespace gb::advance
