#pragma once
#include "mbc.h"
#include "nonstd/span.hpp"

namespace gb {
struct RomHeader {
  Mbc mbc;
  unsigned int rom_size;
  int save_ram_size;
  bool is_cgb;
};

RomHeader parse_rom(nonstd::span<const u8> data);

}  // namespace gb
