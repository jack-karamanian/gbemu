#pragma once
#include <vector>
#include "gba/hardware.h"
#include "gba/input.h"
#include "gba/lcd.h"
#include "types.h"
#include "utils.h"

namespace gb::advance {
inline std::size_t operator"" _kb(unsigned long long int kilobytes) {
  return kilobytes * 1024;
}

struct Cycles {
  u32 sequential = 0;
  u32 nonsequential = 0;
  u32 internal = 0;

  [[nodiscard]] constexpr u32 sum() const {
    return sequential + nonsequential + internal;
  }
};

inline constexpr Cycles operator"" _seq(unsigned long long int cycles) {
  return {static_cast<u32>(cycles)};
}

inline constexpr Cycles operator"" _nonseq(unsigned long long int cycles) {
  return {0, static_cast<u32>(cycles)};
}

inline constexpr Cycles operator"" _intern(unsigned long long int cycles) {
  return {0, 0, static_cast<u32>(cycles)};
}

inline constexpr Cycles operator+(Cycles lhs, Cycles rhs) {
  return {lhs.sequential + rhs.sequential,
          lhs.nonsequential + rhs.nonsequential, lhs.internal + rhs.internal};
}

class Waitcnt : public Integer<u32> {
 public:
  using Integer::Integer;
  [[nodiscard]] u32 sram_wait_control() const {
    return decode_cycles(m_value & 0b11);
  }

  [[nodiscard]] u32 wait_zero_nonsequential() const {
    return decode_cycles((m_value >> 2) & 0b11);
  }

  [[nodiscard]] u32 wait_zero_sequential() const { return test_bit(4) ? 1 : 2; }

  [[nodiscard]] u32 wait_one_nonsequential() const {
    return decode_cycles((m_value >> 5) & 0b11);
  }

  [[nodiscard]] u32 wait_one_sequential() const { return test_bit(7) ? 1 : 4; }

  [[nodiscard]] u32 wait_two_nonsequential() const {
    return decode_cycles((m_value >> 8) & 0b11);
  }

  [[nodiscard]] u32 wait_two_sequential() const { return test_bit(10) ? 1 : 8; }

  [[nodiscard]] bool enable_prefetch_buffer() const { return test_bit(14); }

 private:
  static u32 decode_cycles(u32 value) {
    switch (value) {
      case 0:
        return 4;
      case 1:
        return 3;
      case 2:
        return 2;
      case 3:
        return 8;
      default:
        throw std::runtime_error("expected 0, 1, 2, or 3 for decode_cycles");
    }
  }
};

struct Mmu {
  static constexpr u32 BiosBegin = 0x00000000;
  static constexpr u32 BiosEnd = 0x00003fff;

  static constexpr u32 EWramBegin = 0x02000000;
  static constexpr u32 EWramEnd = 0x0203ffff;

  static constexpr u32 IWramBegin = 0x03000000;
  static constexpr u32 IWramEnd = 0x03007fff;

  static constexpr u32 IoRegistersBegin = 0x04000000;
  static constexpr u32 IoRegistersEnd = 0x040003fe;

  static constexpr u32 PaletteBegin = 0x05000000;
  static constexpr u32 PaletteEnd = 0x050003ff;

  static constexpr u32 VramBegin = 0x06000000;
  static constexpr u32 VramEnd = 0x06017fff;

  static constexpr u32 RomRegion0Begin = 0x08000000;
  static constexpr u32 RomRegion0End = 0x09ffffff;

  static constexpr u32 RomRegion1Begin = 0x0a000000;
  static constexpr u32 RomRegion1End = 0x0bffffff;

  static constexpr u32 RomRegion2Begin = 0x0c000000;

  static constexpr std::array<std::pair<u32, u32>, 3> rom_regions{
      {{RomRegion0Begin, RomRegion0End},
       {RomRegion1Begin, RomRegion1End},
       {RomRegion2Begin, 0xffffffff}}};

  static constexpr u32 Ime = 0x04000208;

  static constexpr u32 Dispcnt = 0x04000000;
  static constexpr u32 DispStatAddr = 0x04000004;
  static constexpr u32 WaitcntAddr = 0x04000204;

  static constexpr u32 KeyInputAddr = 0x04000130;

  std::vector<u8> ewram;
  std::vector<u8> iwram;
  std::vector<u8> palette_ram;
  std::vector<u8> vram;
  std::vector<u8> oam_ram;
  std::vector<u8> rom;

  u32 dispcnt = 0;
  u32 dispstat = 0;
  u32 ime = 0;

  Waitcnt waitcnt{0};

  Hardware hardware;

  Mmu()
      : ewram(256_kb, 0),
        iwram(32_kb, 0),
        palette_ram(1_kb, 0),
        vram(96_kb, 0),
        oam_ram(1_kb, 0) {}

  [[nodiscard]] u32 wait_cycles(u32 addr, Cycles cycles);

  std::tuple<nonstd::span<u8>, u32> select_storage(u32 addr);

  template <typename T>
  void set(u32 addr, T value) {
    switch (addr) {
      case Dispcnt:
        dispcnt = value;
        return;
      case WaitcntAddr:
        waitcnt = Waitcnt{value};
        return;
      case DispStatAddr:
        hardware.lcd->dispstat = DispStat{static_cast<u32>(value)};
        return;
      case Ime:
        ime = value;
        return;
    }

    auto [selected_span, resolved_addr] = select_storage(addr);
    const auto converted = to_bytes(value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
      selected_span[resolved_addr + i] = converted[i];
    }
  }

  template <typename T>
  T at(u32 addr) {
    switch (addr) {
      case Dispcnt:
        return dispcnt;
      case DispStatAddr:
        return hardware.lcd->dispstat.data();
      case WaitcntAddr:
        return waitcnt.data();
      case KeyInputAddr:
        return hardware.input->data();
      case Ime:
        return ime;
    }

    auto [selected_span, resolved_addr] = select_storage(addr);
    return convert_bytes_endian<T>(
        {&selected_span.at(resolved_addr), sizeof(T)});
  }
};

}  // namespace gb::advance
