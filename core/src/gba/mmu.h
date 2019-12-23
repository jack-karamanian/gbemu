#pragma once
#include <fmt/format.h>
#include <fmt/printf.h>
#include <cstring>
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

constexpr Cycles operator"" _seq(unsigned long long int cycles) {
  return {static_cast<u32>(cycles)};
}
constexpr Cycles operator"" _nonseq(unsigned long long int cycles) {
  return {0, static_cast<u32>(cycles)};
}

constexpr Cycles operator"" _intern(unsigned long long int cycles) {
  return {0, 0, static_cast<u32>(cycles)};
}

constexpr Cycles operator+(Cycles lhs, Cycles rhs) {
  return {lhs.sequential + rhs.sequential,
          lhs.nonsequential + rhs.nonsequential, lhs.internal + rhs.internal};
}

struct FlashMemory {
  enum class CommandType : u32 {
    None,
    ErasePrefix,
  };

  int bank = 0;
  int count = 0;
  std::array<u8, 2> m_device_id = {0x62, 0x13};
  CommandType m_command_type = CommandType::None;
  nonstd::span<u8> m_sram;

  FlashMemory(nonstd::span<u8> sram) : m_sram{sram} {}

  bool handle_command(u32 addr, u8 command) {
    if (m_command_type == CommandType::None) {
      switch (command) {
        case 0xf0:
        case 0x90:
          toggle_device_id();
          return true;
        case 0x80:
          m_command_type = CommandType::ErasePrefix;
          return true;
      }
    } else {
      if (m_command_type == CommandType::ErasePrefix) {
        switch (command) {
          // Erase all
          case 0x10:
            if (addr == 0x0e005555) {
              std::fill(m_sram.begin(), m_sram.begin() + 0x10000, 0);
              m_sram[0] = 0xff;
              m_command_type = CommandType::None;
              return true;
            }
            break;
          // Erase sector
          case 0x30: {
            auto begin = m_sram.begin() + (addr & ~0xff000000);
            std::fill(begin, begin + 0x1000, 0);
            m_command_type = CommandType::None;
            return true;
          }
        }
      }
    }
    return false;
  }

  void toggle_device_id() {
    std::swap(m_sram[0], m_device_id[0]);
    std::swap(m_sram[1], m_device_id[1]);
  }

  bool push_byte(u32 addr, u8 value) {
    const auto expect_value = [this](u8 expected, u8 value) {
      count = expected == value ? count + 1 : 0;
    };
    switch (count) {
      case 0:
        if (addr == 0xe005555) {
          expect_value(0xaa, value);
          return true;
        }
        break;
      case 1:
        if (addr == 0xe002aaa) {
          expect_value(0x55, value);
          return true;
        }
        break;
      case 2:
        count = 0;
        return handle_command(addr, value);
        break;
      default:
        throw std::logic_error("unexpected count value");
    }
    return false;
  }
};

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

constexpr u32 memory_region(u32 addr) noexcept {
  return addr & 0xff000000;
}

constexpr bool is_hardware_addr(u32 addr) noexcept {
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

  /*
    .code 32
    stmfd r13!, {r0,r1,r2,r3,r12,r14}
    mov r0, #0x40000000
    add r14, r15, #0
    ldr r15, [r0, #-4]
    ldmfd r13!, {r0, r1, r2, r3, r12, r14}
    subs r15, r14, #4
  */
  static constexpr std::array<u8, 24> bios_interrupt = {
      0x0f, 0x50, 0x2d, 0xe9, 0x01, 0x03, 0xa0, 0xe3, 0x00, 0xe0, 0x8f, 0xe2,
      0x04, 0xf0, 0x10, 0xe5, 0x0f, 0x50, 0xbd, 0xe8, 0x04, 0xf0, 0x5e, 0xe2};

  Waitcnt waitcnt{0};

  Hardware hardware;

  Mmu() {
    std::copy(bios_interrupt.begin(), bios_interrupt.end(),
              m_bios.begin() + 0x128);
  }

  [[nodiscard]] u32 wait_cycles(u32 addr, Cycles cycles);

  void load_rom(std::vector<u8> data) {
    m_rom = std::move(data);
    std::fill(m_memory_region_table.begin() + 0x08,
              m_memory_region_table.begin() + 0x0e, nonstd::span<u8>{m_rom});

    std::transform(
        m_memory_region_table.begin() + 0x08,
        m_memory_region_table.begin() + 0x0e,
        m_memory_region_table.begin() + 0x08,
        [i = 0, rom_span = nonstd::span<u8>(m_rom)](auto region) mutable {
          if (rom_span.size() > 0x01000000) {
            const auto new_span =
                rom_span.subspan(0x01000000 * (i % 2 == 0 ? 0 : 1));
            i = (i + 1) % 2;
            return new_span;
          }
          return region;
        });
  }

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
    if constexpr (std::is_same_v<T, u8>) {
      if (memory_region(addr) == 0x0e000000) {
        if (m_flash_memory.push_byte(addr, value)) {
          return;
        }
      }
    }
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
    if (m_eeprom_enabled && (addr & 0xff000000) == 0x0d000000) {
      return 1;
    }
    if (is_hardware_addr(addr)) {
      T ret{};

      // Account for reads larger than the size of an io register.
      alignas(T) u8 storage[sizeof(T)];

      auto num_bytes = sizeof(T);
      while (num_bytes > 0) {
        const auto [io_addr, resolved_addr] =
            select_io_register(addr + (sizeof(T) - num_bytes));
        const auto selected_hardware =
            select_hardware(io_addr, DataOperation::Read);
        const auto copy_size =
            std::min(sizeof(T), selected_hardware.size_bytes());
        assert((copy_size & (copy_size - 1)) == 0);

        const auto data =
            selected_hardware.byte_span().subspan(resolved_addr).data();
        std::copy_n(data, copy_size, &storage[sizeof(T) - num_bytes]);

        num_bytes -= copy_size;
      }

      std::memcpy(&ret, storage, sizeof(T));

      return ret;
    }

    const auto selected_span = [addr, this]() -> nonstd::span<const u8> {
      const auto [span, resolved_addr] = select_storage(addr);

      if (resolved_addr > span.size()) {
        return get_prefetched_opcode();
      }

      return span.subspan(resolved_addr);
    }();

    T ret{};
    std::memcpy(&ret, selected_span.data(), sizeof(T));
    return ret;
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

  void print_bios_warning() const;

  struct SelectStorageResult {
    nonstd::span<u8> storage;
    u32 resolved_addr;
  };

  [[nodiscard]] SelectStorageResult select_storage(u32 addr) {
    if (addr >= 0x03ffff00 && addr < 0x04000000) {
      const u32 offset = addr & 0xff;
      return {m_iwram, 0x7f00 + offset};
    }

    if (addr >= BiosBegin && addr <= BiosEnd &&
        !(addr >= 0x128 && addr <= 0x13c)) {
      print_bios_warning();
    }

    if (const u32 addr_region = addr >> 24;
        addr_region < m_memory_region_table.size()) {
      return {m_memory_region_table[addr_region], addr - (addr_region << 24)};
    }

    // fmt::printf("unimplemented select_storage addr %08x\n", addr);
    throw std::runtime_error("unimplemented select storage");
  }

  template <typename Func>
  void set_write_handler(Func&& func) {
    m_write_handler = std::forward<Func>(func);
  }

 private:
  [[nodiscard]] IntegerRef select_hardware(u32 addr, DataOperation op);

  void eeprom_send_command(nonstd::span<const u8> source, u32 count);
  void eeprom_read(nonstd::span<u8> dest);

  nonstd::span<const u8> get_prefetched_opcode() const noexcept;

  std::vector<u8> m_bios = std::vector<u8>(16_kb, 0);
  std::vector<u8> m_ewram = std::vector<u8>(256_kb, 0);
  std::vector<u8> m_iwram = std::vector<u8>(32_kb, 0);
  std::vector<u8> m_palette_ram = std::vector<u8>(1_kb, 0);
  std::vector<u8> m_vram = std::vector<u8>(96_kb, 0);
  std::vector<u8> m_oam_ram = std::vector<u8>(1_kb, 0);
  std::vector<u8> m_rom;
  std::vector<u8> m_sram = std::vector<u8>(64_kb, 0xff);
  std::vector<u8> m_eeprom = std::vector<u8>(8_kb, 0xff);

  FlashMemory m_flash_memory{m_sram};

  u64 m_eeprom_buffer = 0;

  std::function<void(u32, u32)> m_write_handler;

  std::array<nonstd::span<u8>, 16> m_memory_region_table = {{
      {m_bios},  // 00
      nonstd::span<u8>(nullptr,
                       static_cast<nonstd::span<u8>::index_type>(0)),  // 01
      {m_ewram},                                                       // 02
      {m_iwram},                                                       // 03
      nonstd::span<u8>(nullptr,
                       static_cast<nonstd::span<u8>::index_type>(0)),  // 04
      {m_palette_ram},                                                 // 05
      {m_vram},                                                        // 06
      {m_oam_ram},                                                     // 07
      {m_rom},                                                         // 08
      {m_rom},                                                         // 09
      {m_rom},                                                         // 0a
      {m_rom},                                                         // 0b
      {m_rom},                                                         // 0c
      {m_rom},                                                         // 0d
      {m_sram},                                                        // 0e
  }};

  bool m_eeprom_enabled = false;
};

}  // namespace gb::advance
