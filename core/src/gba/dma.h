#pragma once
#include "utils.h"

namespace gb::advance {
class Mmu;
class Cpu;
class Dma {
 public:
  enum class DmaNumber : u32 {
    Dma0 = 0,
    Dma1,
    Dma2,
    Dma3,
  };

  template <Dma::DmaNumber dma_number>
  struct Addresses {
    static constexpr u32 DmaOffset = 0xc * static_cast<u32>(dma_number);
    static constexpr u32 Source = 0x040000b0 + DmaOffset;
    static constexpr u32 Dest = 0x040000b4 + DmaOffset;
    static constexpr u32 Count = 0x040000b8 + DmaOffset;
    static constexpr u32 Control = 0x040000ba + DmaOffset;
  };

  enum class AddressType {
    Source,
    Dest,
    Count,
    Control,
  };

  [[nodiscard]] static std::tuple<AddressType, DmaNumber, bool> select_dma(
      u32 addr);

  class Control : public Integer<u16> {
   public:
    Control(u16 value, Dma& dma) : Integer::Integer(value), m_dma{&dma} {}

    void on_after_write() const { m_dma->handle_side_effects(); }

    enum class AddrControl : u32 {
      Increment = 0,
      Decrement = 1,
      Fixed = 2,
      IncrementAndReload = 3,
    };

    enum class StartTiming : u32 {
      Immediately = 0,
      VBlank = 1,
      HBlank = 2,
      Special = 3,
    };

    [[nodiscard]] AddrControl dest_addr_control() const {
      return static_cast<AddrControl>((m_value >> 5) & 0b11);
    }

    [[nodiscard]] AddrControl source_addr_control() const {
      return static_cast<AddrControl>((m_value >> 7) & 0b11);
    }

    [[nodiscard]] bool repeat() const { return test_bit(9); }

    [[nodiscard]] bool word_transfer() const { return test_bit(10); }

    [[nodiscard]] bool game_pack_drq() const { return test_bit(11); }

    [[nodiscard]] StartTiming start_timing() const {
      return static_cast<StartTiming>((m_value >> 12) & 0b11);
    }

    [[nodiscard]] bool interrupt_at_end() const { return test_bit(14); }

    [[nodiscard]] bool enabled() const { return test_bit(15); }

    void set_enabled(bool set) { set_bit(15, set); }

   private:
    Dma* m_dma;
  };

  u32 source = 0;
  u32 dest = 0;
  u16 count = 0;

  Dma(Mmu& mmu, Cpu& cpu, DmaNumber dma_number)
      : m_mmu{&mmu},
        m_cpu{&cpu},
        m_dma_number{dma_number},
        m_source_mask{static_cast<u32>(
            dma_number == DmaNumber::Dma0 ? 0x07ffffff : 0x0fffffff)},
        m_dest_mask{static_cast<u32>(
            dma_number == DmaNumber::Dma3 ? 0x0fffffff : 0x07ffffff)} {}

  [[nodiscard]] Control& control() { return m_control; }
  [[nodiscard]] const Control& control() const { return m_control; }

  [[nodiscard]] DmaNumber number() const { return m_dma_number; }

  void handle_side_effects() {
    if (m_control.enabled() &&
        m_control.start_timing() == Control::StartTiming::Immediately) {
      m_internal_dest = dest;
      run();
    }
  }

  void run();

 private:
  Mmu* m_mmu;
  Cpu* m_cpu;
  Control m_control{0, *this};
  DmaNumber m_dma_number;
  u32 m_source_mask;
  u32 m_dest_mask;
  u32 m_internal_dest = 0;
};

class Dmas {
 public:
  Dmas(Mmu& mmu, Cpu& cpu)
      : m_dmas{Dma{mmu, cpu, Dma::DmaNumber::Dma0},
               Dma{mmu, cpu, Dma::DmaNumber::Dma1},
               Dma{mmu, cpu, Dma::DmaNumber::Dma2},
               Dma{mmu, cpu, Dma::DmaNumber::Dma3}} {}

  [[nodiscard]] Dma& dma(Dma::DmaNumber dma_number) {
    return m_dmas[static_cast<u32>(dma_number)];
  }

  [[nodiscard]] nonstd::span<Dma> span() { return m_dmas; }

 private:
  std::array<Dma, 4> m_dmas;
};
}  // namespace gb::advance
