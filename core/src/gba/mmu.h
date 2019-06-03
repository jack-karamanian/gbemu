#pragma once
#include <vector>
#include "types.h"
#include "utils.h"

namespace gb::advance {
inline std::size_t operator"" _kb(unsigned long long int kilobytes) {
  return kilobytes * 1024;
}

struct Mmu {
  static constexpr u32 IWramBegin = 0x03000000;
  static constexpr u32 IWramEnd = 0x03007fff;

  static constexpr u32 PaletteBegin = 0x05000000;
  static constexpr u32 PaletteEnd = 0x050003ff;

  static constexpr u32 VramBegin = 0x06000000;
  static constexpr u32 VramEnd = 0x06017fff;

  static constexpr u32 RomBegin = 0x08000000;
  static constexpr u32 Ime = 0x04000208;

  static constexpr u32 Dispcnt = 0x04000000;
  static constexpr u32 DispStat = 0x04000004;

  std::vector<u8> ewram;
  std::vector<u8> iwram;
  std::vector<u8> palette_ram;
  std::vector<u8> vram;
  std::vector<u8> oam_ram;
  std::vector<u8> rom;

  u32 dispcnt = 0;
  u32 dispstat = 0;
  u32 ime = 0;

  Mmu()
      : ewram(256_kb, 0),
        iwram(32_kb, 0),
        palette_ram(1_kb, 0),
        vram(96_kb, 0),
        oam_ram(1_kb, 0) {}

  std::tuple<nonstd::span<u8>, u32> select_storage(u32 addr) {
    if (addr >= IWramBegin && addr <= IWramEnd) {
      return {iwram, addr - IWramBegin};
    }
    if (addr >= VramBegin && addr <= VramEnd) {
      return {vram, addr - VramBegin};
    }

    if (addr >= RomBegin) {
      return {rom, addr - RomBegin};
    }

    if (addr >= PaletteBegin && addr <= PaletteEnd) {
      return {palette_ram, addr - PaletteBegin};
    }

    printf("unimplemented %08x\n", addr);
    throw std::runtime_error("unimplemented select storage");
  }

  template <typename T>
  void set(u32 addr, T value) {
    switch (addr) {
      case Dispcnt:
        dispcnt = value;
        return;
      case DispStat:
        dispstat = value;
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
      case DispStat:
        return dispstat;
      case Ime:
        return ime;
    }

    auto [selected_span, resolved_addr] = select_storage(addr);
    return convert_bytes_endian<T>(
        {&selected_span.at(resolved_addr), sizeof(T)});
  }
};

}  // namespace gb::advance
