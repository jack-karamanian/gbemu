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

inline bool is_hardware_addr(u32 addr) {
  return (addr & 0xff000000) == 0x04000000;
}

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

  static constexpr u32 SramBegin = 0x0e000000;
  static constexpr u32 SramEnd = 0x0e00ffff;

  static constexpr std::array<std::pair<u32, u32>, 3> rom_regions{
      {{RomRegion0Begin, RomRegion0End},
       {RomRegion1Begin, RomRegion1End},
       {RomRegion2Begin, 0xffffffff}}};

  static constexpr u32 Ime = 0x04000208;

  static constexpr u32 Dispcnt = 0x04000000;
  static constexpr u32 DispStatAddr = 0x04000004;
  static constexpr u32 WaitcntAddr = 0x04000204;

  static constexpr u32 KeyInputAddr = 0x04000130;

  Waitcnt waitcnt{0};

  Hardware hardware;

  Mmu()
      : m_bios(16_kb, 0),
        m_ewram(256_kb, 0),
        m_iwram(32_kb, 0),
        m_palette_ram(1_kb, 0),
        m_vram(96_kb, 0),
        m_oam_ram(1_kb, 0),
        m_sram(64_kb, 0) {}

  [[nodiscard]] u32 wait_cycles(u32 addr, Cycles cycles);

  void load_rom(std::vector<u8>&& data) { m_rom = std::move(data); }

  nonstd::span<u8> bios() { return m_bios; }

  nonstd::span<u8> ewram() { return m_ewram; }

  nonstd::span<u8> iwram() { return m_iwram; }

  nonstd::span<u8> palette_ram() { return m_palette_ram; }

  nonstd::span<u8> vram() { return m_vram; }

  nonstd::span<u8> oam_ram() { return m_oam_ram; }

  nonstd::span<u8> sram() { return m_sram; }

  nonstd::span<u8> rom() { return m_rom; }

  template <typename T>
  void set(u32 addr, T value) {
    const auto converted = to_bytes(value);
    if (is_hardware_addr(addr)) {
      set_hardware_bytes(addr, converted);
    } else {
      set_bytes(addr, converted);
    }
  }

  void set_bytes(u32 addr, nonstd::span<const u8> bytes);
  void set_hardware_bytes(u32 addr, nonstd::span<const u8> bytes);

  template <typename T>
  T at(u32 addr) {
    const auto selected_span = [addr, this] {
      if (is_hardware_addr(addr)) {
        const auto [io_addr, resolved_addr] = select_io_register(addr);
        const auto selected_hardware =
            select_hardware(io_addr, DataOperation::Read);
        return selected_hardware.byte_span().subspan(resolved_addr);
      }
      const auto [span, resolved_addr] = select_storage(addr);
      return span.subspan(resolved_addr);
    }();
    return convert_bytes_endian<T>(
        nonstd::span<const u8, sizeof(T)>{selected_span.data(), sizeof(T)});
  }

  enum class AddrOp : int {
    Increment = 1,
    Decrement = -1,
    Fixed = 0,
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

  [[nodiscard]] std::tuple<nonstd::span<u8>, u32> select_storage(u32 addr);

  template <typename Func>
  void set_write_handler(Func func) {
    m_write_handler = func;
  }

 private:
  std::vector<u8> m_bios;
  std::vector<u8> m_ewram;
  std::vector<u8> m_iwram;
  std::vector<u8> m_palette_ram;
  std::vector<u8> m_vram;
  std::vector<u8> m_oam_ram;
  std::vector<u8> m_rom;
  std::vector<u8> m_sram;

  [[nodiscard]] IntegerRef select_hardware(u32 addr, DataOperation op);

  std::function<void(u32, u32)> m_write_handler;
};

}  // namespace gb::advance
