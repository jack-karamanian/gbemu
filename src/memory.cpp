#include <algorithm>
#include <iostream>
#include "memory.h"

namespace gb {

std::pair<u16, nonstd::span<const u8>> Memory::select_storage(u16 addr) {
  if (addr < 0x8000) {
    switch (addr & 0xf000) {
      case 0x0000:
      case 0x1000:
      case 0x2000:
      case 0x3000:
        return std::make_pair(addr,
                              nonstd::span<const u8>(rom.data(), SIXTEEN_KB));

      case 0x4000:
      case 0x5000:
      case 0x6000:
      case 0x7000: {
        const int start_addr =
            SIXTEEN_KB * rom_bank_selected.get_rom_bank_selected();
        return std::make_pair(
            addr - 0x4000,
            nonstd::span<const u8>(&rom.at(start_addr), SIXTEEN_KB));
      }
    }
  }
  return std::make_pair(addr, nonstd::span<const u8>(memory));
}
nonstd::span<const u8> Memory::get_range(std::pair<u16, u16> range) {
  const auto [begin, end] = range;

  return nonstd::span<const u8>(&memory.at(begin), &memory.at(end + 1));
}

void Memory::set(const u16& addr, const u8& val) {
  switch (addr & 0xf000) {
    case 0x2000:
    case 0x3000:
      // Set lower 5 rom bank bits
      rom_bank_selected.set_lower(val);
      break;
    case 0x4000:
    case 0x5000:
      // Set upper two rom bank bits
      rom_bank_selected.set_upper(val);
      break;
  }
  switch (addr) {
    case 0xff46:
      do_dma_transfer(val);
      break;
    case 0xff02:
      if (val == 0x81) {
        std::cout << "char: " << memory[0xff01] << std::endl;
      }
      break;

    default:
      if (addr >= 0x8000) {
        memory[addr] = val;
      }
      break;
  }
}

void Memory::reset() {
  memory[0xFF05] = 0x00;
  memory[0xFF06] = 0x00;
  memory[0xFF07] = 0x00;
  memory[0xFF10] = 0x80;
  memory[0xFF11] = 0xBF;
  memory[0xFF12] = 0xF3;
  memory[0xFF14] = 0xBF;
  memory[0xFF16] = 0x3F;
  memory[0xFF17] = 0x00;
  memory[0xFF19] = 0xBF;
  memory[0xFF1A] = 0x7F;
  memory[0xFF1B] = 0xFF;
  memory[0xFF1C] = 0x9F;
  memory[0xFF1E] = 0xBF;
  memory[0xFF20] = 0xFF;
  memory[0xFF21] = 0x00;
  memory[0xFF22] = 0x00;
  memory[0xFF23] = 0xBF;
  memory[0xFF24] = 0x77;
  memory[0xFF25] = 0xF3;
  memory[0xFF26] = 0xF1;
  memory[0xFF40] = 0x91;
  memory[0xFF42] = 0x00;
  memory[0xFF43] = 0x00;
  memory[0xFF45] = 0x00;
  memory[0xFF47] = 0xFC;
  memory[0xFF48] = 0xFF;
  memory[0xFF49] = 0xFF;
  memory[0xFF4A] = 0x00;
  memory[0xFF4B] = 0x00;
  memory[0xFFFE] = 0x00;
}

void Memory::load_rom(const std::vector<u8>& data) {
  std::copy(data.begin(), data.end(), &rom[0]);
}

void Memory::do_dma_transfer(const u8& data) {
  const u16 addr = ((u16)data) << 8;
  for (u16 i = 0; i < 160; i++) {
    memory[0xfe00 + i] = memory[addr + i];
  }
}

u8 Memory::get_input_register() {
  return memory[0xff00];
}

void Memory::set_input_register(u8 val) {
  set(0xff00, val);
}

nonstd::span<const SpriteAttribute> Memory::get_sprite_attributes() {
  const u8* sprite_attrib_begin = &memory[0xfe00];
  const u8* sprite_attrib_end = &memory[0xfea0];

  return nonstd::span<const SpriteAttribute>(
      reinterpret_cast<const SpriteAttribute*>(sprite_attrib_begin),
      reinterpret_cast<const SpriteAttribute*>(sprite_attrib_end));
}
}  // namespace gb
