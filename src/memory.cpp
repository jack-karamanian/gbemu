#include <algorithm>
#include <iostream>
#include "memory.h"

namespace gb {

std::pair<u16, nonstd::span<const u8>> Memory::select_storage(u16 addr) {
  if (mbc.in_ram_range(addr)) {
    return {mbc.relative_ram_address(addr),
            {&save_ram[mbc.absolute_ram_offset()], 8192}};
  }
  if (addr < 0x8000) {
    switch (addr & 0xf000) {
      case 0x0000:
      case 0x1000:
      case 0x2000:
      case 0x3000: {
        const int start_addr = SIXTEEN_KB * mbc.lower_rom_bank_selected();
        if (start_addr != 0) {
          std::cout << std::hex << start_addr << '\n';
        }

        return {addr - start_addr, {&rom.at(start_addr), SIXTEEN_KB}};
      }

      case 0x4000:
      case 0x5000:
      case 0x6000:
      case 0x7000: {
        const int start_addr = SIXTEEN_KB * mbc.rom_bank_selected();
        return {addr - 0x4000, {&rom.at(start_addr), SIXTEEN_KB}};
      }
    }
  }
  return {addr, memory_span};
}

nonstd::span<const u8> Memory::get_range(std::pair<u16, u16> range) {
  const auto [begin, end] = range;

  return {&memory.at(begin), &memory.at(end + 1)};
}

void Memory::set(u16 addr, u8 val) {
  auto write_callback_for_addr = write_callbacks.find(addr);
  if (write_callback_for_addr != write_callbacks.end()) {
    write_callback_for_addr->second(val, memory[addr]);
  }
  if (addr == 0xff44) {
    memory[0xff44] = 0;
    return;
  }

  if (bool mbc_handled = mbc.handle_memory_write(addr, val); !mbc_handled) {
    if (mbc.save_ram_enabled() && mbc.in_ram_range(addr)) {
      std::size_t ram_index = mbc.absolute_ram_address(addr);
      save_ram[ram_index] = val;
      save_ram_write_listener(ram_index, val);
    } else {
      switch (addr) {
        case 0xff46:
          do_dma_transfer(val);
          break;
        case 0xff02:
          if (val == 0x81) {
            std::cout << memory[0xff01];
          }
          break;

        default:
          if (addr >= 0x8000) {
            memory[addr] = val;
          }
          break;
      }
    }
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
  memory[0xFF00] = 0xFF;
}

void Memory::add_write_listener_for_range(u16 begin,
                                          u16 end,
                                          const AddrMemoryListener& callback) {
  for (u16 addr = begin; addr <= end; addr++) {
    add_write_listener(addr, [callback, addr](u8 val, u8 prev_val) {
      callback(addr, val, prev_val);
    });
  }
}

RomHeader Memory::load_rom(nonstd::span<const u8> data) {
  // Decode the MBC type from the cart header
  const Mbc::Type mbc_type = [rom_type = data[0x147]] {
    printf("rom type: %d\n", rom_type);
    switch (rom_type) {
      case 0x00:
        return Mbc::Type::None;
        break;
      case 0x01:
      case 0x02:
      case 0x03:
        return Mbc::Type::MBC1;
        break;
      case 0x05:
      case 0x06:
        return Mbc::Type::MBC2;
        break;
      case 0x0f:
      case 0x10:
      case 0x11:
      case 0x12:
      case 0x13:
        return Mbc::Type::MBC3;
        break;
      case 0x19:
        return Mbc::Type::MBC5;
        break;
      default:
        return Mbc::Type::None;
    }
  }();

  const int save_ram_size = [ram_size = data[0x149], mbc_type] {
    if (mbc_type == Mbc::Type::MBC2) {
      return 256;
    }
    switch (ram_size) {
      case 0:
        return 0;
      case 1:
        return 2 * 1024;
      case 2:
        return 8 * 1024;
      case 3:
        return 32 * 1024;
      case 4:
        return 128 * 1024;
      case 5:
        return 64 * 1024;
      default:
        throw std::runtime_error("invalid save ram size");
    }
  }();

  save_ram.resize(save_ram_size);
  std::fill(save_ram.begin(), save_ram.end(), 0xff);

  const int rom_size = (1024 * 32) << data[0x148];
  mbc = Mbc{mbc_type, static_cast<u16>(rom_size / SIXTEEN_KB)};
  printf("%d\n", data[0x148]);
  rom.resize(rom_size);

  std::copy(data.begin(), data.end(), &rom[0]);

  return {mbc_type, rom_size, save_ram_size};
}

void Memory::do_dma_transfer(const u8& data) {
  const u16 addr = ((u16)data) << 8;
  for (u16 i = 0; i < 160; i++) {
    memory[0xfe00 + i] = memory[addr + i];
  }
}

nonstd::span<const SpriteAttribute> Memory::get_sprite_attributes() {
  const u8* sprite_attrib_begin = &memory[0xfe00];
  const u8* sprite_attrib_end = &memory[0xfea0];

  return nonstd::span<const SpriteAttribute>(
      reinterpret_cast<const SpriteAttribute*>(sprite_attrib_begin),
      reinterpret_cast<const SpriteAttribute*>(sprite_attrib_end));
}
}  // namespace gb
