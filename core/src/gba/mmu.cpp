#include "mmu.h"

namespace gb::advance {
u32 Mmu::wait_cycles(u32 addr, Cycles cycles) {
  const auto [wait_nonsequential,
              wait_sequential] = [&]() -> std::pair<u32, u32> {
    if (addr >= EWramBegin && addr <= EWramEnd) {
      return {2, 2};
    }

    if (addr >= RomRegion0Begin && addr <= RomRegion0End) {
      return {waitcnt.wait_zero_nonsequential(),
              waitcnt.wait_zero_sequential()};
    }

    if (addr >= RomRegion1Begin && addr <= RomRegion1End) {
      return {waitcnt.wait_one_nonsequential(), waitcnt.wait_one_sequential()};
    }

    if (addr >= RomRegion2Begin) {
      return {waitcnt.wait_two_nonsequential(), waitcnt.wait_two_sequential()};
    }

    return {0, 0};
  }();
  return cycles.internal +
         (cycles.nonsequential +
          (cycles.nonsequential != 0 ? wait_nonsequential : 0)) +
         (cycles.sequential + (cycles.sequential != 0 ? wait_sequential : 0));
}

std::tuple<nonstd::span<u8>, u32> Mmu::select_storage(u32 addr) {
  if (addr >= IWramBegin && addr <= IWramEnd) {
    return {iwram, addr - IWramBegin};
  }

  if (addr >= EWramBegin && addr <= EWramEnd) {
    return {ewram, addr - EWramBegin};
  }

  if (addr >= VramBegin && addr <= VramEnd) {
    return {vram, addr - VramBegin};
  }

  if (addr >= RomRegion1Begin && addr <= RomRegion1End) {
    return {rom, addr - RomRegion1Begin};
  }

  if (addr >= PaletteBegin && addr <= PaletteEnd) {
    return {palette_ram, addr - PaletteBegin};
  }

  for (const auto [begin, end] : rom_regions) {
    if (addr >= begin && addr <= end) {
      return {rom, addr - begin};
    }
  }

  printf("unimplemented select_storage addr %08x\n", addr);
  throw std::runtime_error("unimplemented select storage");
}

}  // namespace gb::advance
