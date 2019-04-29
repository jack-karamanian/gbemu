#include <algorithm>
#include <iostream>
#include "gpu.h"
#include "lcd.h"
#include "memory.h"
#include "registers/cgb.h"
#include "registers/dmg.h"
#include "sound.h"
#include "timers.h"

namespace gb {

std::pair<u16, nonstd::span<u8>> Memory::select_storage(u16 addr) {
  if (addr < 0x8000) {
    switch (addr & 0xf000) {
      case 0x0000:
      case 0x1000:
      case 0x2000:
      case 0x3000: {
        const int start_addr = SIXTEEN_KB * mbc.lower_rom_bank_selected();
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

  if (mbc.in_ram_range(addr)) {
    return {mbc.relative_ram_address(addr),
            {&save_ram[mbc.absolute_ram_offset()], 8192}};
  }

  switch (addr & 0xf000) {
    case 0x8000:
    case 0x9000:
      if ((memory[Registers::Cgb::Vbk::Address] & 1) != 0) {
        return {addr - 0x8000, {vram_bank1}};
      }
      break;
    case 0xd000: {
      const u8 ram_bank = memory[Registers::Cgb::Svbk::Address] & 0x7;
      const int start_addr = 4096 * (ram_bank == 0 ? 1 : ram_bank) - 1;
      return {addr - 0xd000, {&extended_ram[start_addr], 4096}};
    }
  }

  return {addr, memory_span};
}

nonstd::span<const u8> Memory::get_range(std::pair<u16, u16> range) {
  const auto [begin, end] = range;

  return {&memory.at(begin), &memory.at(end + 1)};
}

std::optional<u8> Memory::read_hardware(u16 addr) {
  switch (addr) {
    case Registers::Cgb::BgPaletteIndex::Address:
      return hardware.gpu->background_palette_index();
    case Registers::Cgb::SpritePaletteIndex::Address:
      return hardware.gpu->sprite_palette_index();
    case Registers::Cgb::BgPaletteColor::Address:
      return hardware.gpu->read_background_color();
    case Registers::Cgb::SpritePaletteColor::Address:
      return hardware.gpu->read_sprite_color();
    // Timers
    case Registers::Tac::Address:
      return hardware.timers->get_tac().get_value();
    case Registers::Tima::Address:
      return hardware.timers->get_tima();
    case Registers::Tma::Address:
      return hardware.timers->get_tma();
    case Registers::Div::Address:
      return hardware.timers->get_div();
    // Graphics
    case Registers::Ly::Address:
      return hardware.lcd->get_ly();
    case Registers::Lyc::Address:
      return hardware.lcd->get_lyc();
    case Registers::LcdStat::Address:
      return hardware.lcd->get_lcd_stat().get_value();
    case 0xff43:
      return hardware.gpu->get_scx();
    case 0xff42:
      return hardware.gpu->get_scy();
    // Sound
    case Registers::Sound::Control::NR52::Address:
      return hardware.sound->handle_memory_read(addr);
    default:
      return {};
  }
}

void Memory::set(u16 addr, u8 val) {
  switch (addr) {
    case Registers::Cgb::BgPaletteIndex::Address:
      hardware.gpu->set_background_color_index(val);
      return;
    case Registers::Cgb::SpritePaletteIndex::Address:
      hardware.gpu->set_sprite_color_index(val);
      return;
    case Registers::Cgb::BgPaletteColor::Address:
      hardware.gpu->compute_background_color(val);
      return;
    case Registers::Cgb::SpritePaletteColor::Address:
      hardware.gpu->compute_sprite_color(val);
      return;
    case Registers::Lyc::Address:
      hardware.lcd->set_lyc(val);
      return;
    case Registers::LcdStat::Address:
      hardware.lcd->set_lcd_stat(Registers::LcdStat{val});
      return;
    case Registers::Lcdc::Address:
      hardware.lcd->set_enabled(test_bit(val, 7));
      break;
    case 0xff43:
      hardware.gpu->set_scx(val);
      return;
    case 0xff42:
      hardware.gpu->set_scy(val);
      return;
    case 0xff10:
    case 0xff11:
    case 0xff12:
    case 0xff13:
    case 0xff14:
    case 0xff15:
    case 0xff16:
    case 0xff17:
    case 0xff18:
    case 0xff19:
    case 0xff1a:
    case 0xff1b:
    case 0xff1c:
    case 0xff1d:
    case 0xff1e:
    case 0xff1f:
    case 0xff20:
    case 0xff21:
    case 0xff22:
    case 0xff23:
    case 0xff24:
    case 0xff25:
    case 0xff26:
      hardware.sound->handle_memory_write(addr, val);
      memory[addr] = val;
      return;

    case Registers::Div::Address:
    case Registers::Tac::Address:
    case Registers::Tma::Address:
    case Registers::Tima::Address:
      hardware.timers->handle_memory_write(addr, val);
      return;
  }

  write_listener(addr, val, hardware);

  if (bool mbc_handled = mbc.handle_memory_write(addr, val); !mbc_handled) {
    if (mbc.save_ram_enabled() && mbc.in_ram_range(addr)) {
      std::size_t ram_index = mbc.absolute_ram_address(addr);
      save_ram[ram_index] = val;
      if (save_ram_write_listener) {
        save_ram_write_listener(ram_index, val);
      }
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
          if (addr >= 0x8000 && !mbc.in_ram_range(addr)) {
            // memory[addr] = val;
            const auto [resolved_addr, storage] = select_storage(addr);
            storage[resolved_addr] = val;
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

  memory[0xff4d] = 0x7e;
}

void Memory::load_rom(std::vector<u8>&& data) {
  rom = std::move(data);
}

void Memory::do_dma_transfer(const u8& data) {
  const u16 addr = static_cast<u16>(data) << 8;
  for (u16 i = 0; i < 160; i++) {
    memory[0xfe00 + i] = memory[addr + i];
  }
}
}  // namespace gb
