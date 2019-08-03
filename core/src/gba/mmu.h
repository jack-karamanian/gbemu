#pragma once
#include <fmt/format.h>
#include <fmt/printf.h>
#include <functional>
#include <variant>
#include <vector>
#include "error_handling.h"
#include "gba/dma.h"
#include "gba/hardware.h"
#include "gba/input.h"
#include "gba/lcd.h"
#include "io_registers.h"
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
    switch (value & 0b11) {
      case 0:
        return 4;
      case 1:
        return 3;
      case 2:
        return 2;
      case 3:
        return 8;
      default:
        GB_UNREACHABLE();
    }
  }
};

class Mmu {
 public:
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

  static constexpr u32 OamBegin = 0x07000000;
  static constexpr u32 OamEnd = 0x070003ff;

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

  std::vector<u8> bios;
  std::vector<u8> ewram;
  std::vector<u8> iwram;
  std::vector<u8> palette_ram;
  std::vector<u8> vram;
  std::vector<u8> oam_ram;
  std::vector<u8> rom;

  std::vector<u8> io_registers;

  u32 dispcnt = 0;
  u32 dispstat = 0;
  u32 ime = 0;

  Waitcnt waitcnt{0};

  Hardware hardware;

  Mmu()
      : bios(16_kb, 0),
        ewram(256_kb, 0),
        iwram(32_kb, 0),
        palette_ram(1_kb, 0),
        vram(96_kb, 0),
        oam_ram(1_kb, 0),
        io_registers(522, 0) {}

  [[nodiscard]] u32 wait_cycles(u32 addr, Cycles cycles);

  template <typename T>
  T handle_pre_write_side_effects(u32 addr, T value) {}

  template <typename T>
  void set(u32 addr, T value) {
    const auto converted = to_bytes(value);
    set_bytes(addr, converted);
  }

  void set_bytes(u32 addr, nonstd::span<const u8> bytes) {
    auto [selected_span, resolved_addr] =
        select_storage(addr, DataOperation::Write);
    auto subspan = selected_span.subspan(resolved_addr);

    const bool is_overrunning = bytes.size() > selected_span.size();
    const long difference = bytes.size() - subspan.size();

    const std::size_t copy_size = is_overrunning ? difference : bytes.size();
    // std::copy(converted.begin(), converted.end(), subspan.begin());

    for (std::size_t i = 0; i < copy_size; ++i) {
      if (addr >= hardware::IF && addr < hardware::IF + 2) {
        subspan[i] ^= bytes[i];
      } else {
        subspan[i] = bytes[i];
      }
    }

    handle_write_side_effects(addr);

    if (is_overrunning) {
      set_bytes(addr + difference, bytes.subspan(difference));
    }

    if (m_write_handler) {
      m_write_handler(addr, 0);
    }
  }

  template <typename T>
  T at(u32 addr) {
    const auto [selected_span, resolved_addr] =
        select_storage(addr, DataOperation::Read);
    return convert_bytes_endian<T>(nonstd::span<const u8, sizeof(T)>{
        &selected_span.at(resolved_addr), sizeof(T)});
  }

  enum class AddrOp {
    Increment,
    Decrement,
    Fixed,
  };

  struct AddrParam {
    u32 addr;
    AddrOp op;
  };

  void copy_memory(AddrParam source, AddrParam dest, u32 count, u32 type_size);

  enum class DataOperation {
    Read,
    Write,
  };

  [[nodiscard]] std::tuple<nonstd::span<u8>, u32> select_storage(
      u32 addr,
      DataOperation op);

  template <typename Func>
  void set_write_handler(Func func) {
    m_write_handler = func;
  }

 private:
  void handle_write_side_effects(u32 addr);

  [[nodiscard]] std::tuple<nonstd::span<u8>, u32> select_hardware(
      u32 addr,
      DataOperation op);

  std::function<void(u32, u32)> m_write_handler;
};

}  // namespace gb::advance
