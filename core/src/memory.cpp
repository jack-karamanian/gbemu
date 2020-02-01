#include "memory.h"
#include <algorithm>
#include <iostream>
#include "gpu.h"
#include "input.h"
#include "lcd.h"
#include "registers/cgb.h"
#include "registers/dmg.h"
#include "sound.h"
#include "timers.h"

namespace gb {

std::pair<u16, nonstd::span<u8>> Memory::select_storage(u16 addr) {
  if (addr >= 0 && addr <= 0x3fff) {
    const int start_addr = SIXTEEN_KB * mbc.lower_rom_bank_selected();
    return {addr - start_addr, {&rom.at(start_addr), SIXTEEN_KB}};
  }

  if (addr >= 0x4000 && addr <= 0x7fff) {
    const int start_addr = SIXTEEN_KB * mbc.rom_bank_selected();
    return {addr - 0x4000, {&rom.at(start_addr), SIXTEEN_KB}};
  }

  if (mbc.in_ram_range(addr)) {
    return {Mbc::relative_ram_address(addr),
            {&save_ram[mbc.absolute_ram_offset()], 8192}};
  }

  if (addr >= 0xc000 && addr <= 0xcfff) {
    const u16 resolved_addr = addr - 0xc000;
    return {resolved_addr, {wram.data(), 4096}};
  }

  if (addr >= 0xd000 && addr <= 0xdfff) {
    const u8 ram_bank = memory[Registers::Cgb::Svbk::Address] & 0x7;
    const int start_addr = 4096 * ((ram_bank == 0 ? 1 : ram_bank) - 1);
    return {addr - 0xd000, {&extended_ram[start_addr], 4096}};
  }
  if (addr >= 0x8000 && addr <= 0x9fff) {
    if ((memory[Registers::Cgb::Vbk::Address] & 1) != 0) {
      return {addr - 0x8000, {vram_bank1}};
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
    // Input
    case Registers::Input::Address:
      return hardware.input->input_state;
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
    case Registers::Cgb::BgPaletteIndex::Address:
      return hardware.gpu->background_palette_index();
    case Registers::Cgb::SpritePaletteIndex::Address:
      return hardware.gpu->sprite_palette_index();
    case Registers::Cgb::BgPaletteColor::Address:
      return hardware.gpu->read_background_color();
    case Registers::Cgb::SpritePaletteColor::Address:
      return hardware.gpu->read_sprite_color();
    case Registers::Ly::Address:
      return hardware.lcd->get_ly();
    case Registers::Lyc::Address:
      return hardware.lcd->get_lyc();
    case Registers::LcdStat::Address:
      return hardware.lcd->get_lcd_stat().get_value();
    case Registers::Scx::Address:
      return hardware.gpu->scx;
    case Registers::Scy::Address:
      return hardware.gpu->scy;
    case Registers::WindowX::Address:
      return hardware.gpu->window_x;
    case Registers::WindowY::Address:
      return hardware.gpu->window_y;
    // Sound
    case Registers::Sound::Control::NR52::Address:
      return hardware.sound->handle_memory_read(addr);
    // HDMA
    case Registers::Cgb::Hdma::SourceHigh::Address:
    case Registers::Cgb::Hdma::SourceLow::Address:
    case Registers::Cgb::Hdma::DestHigh::Address:
    case Registers::Cgb::Hdma::DestLow::Address:
      return 0xff;
    case Registers::Cgb::Hdma::Start::Address:
      std::cout << "UNIMPLEMENTED: READ HDMA START\n";
      return {};
    default:
      return {};
  }
}

void Memory::set(u16 addr, u8 val) {
  switch (addr) {
    // Input
    case Registers::Input::Address:
      hardware.input->input_state = val;
      return;
    // HDMA
    case Registers::Cgb::Hdma::SourceHigh::Address:
      hardware.hdma->source_high = val;
      return;
    case Registers::Cgb::Hdma::SourceLow::Address:
      hardware.hdma->source_low = val;
      return;
    case Registers::Cgb::Hdma::DestHigh::Address:
      hardware.hdma->dest_high = val;
      return;
    case Registers::Cgb::Hdma::DestLow::Address:
      hardware.hdma->dest_low = val;
      return;
    case Registers::Cgb::Hdma::Start::Address:
      hardware.hdma->start(val);
      return;
    // Graphics
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
    case Registers::LcdStat::Address: {
      const Registers::LcdStat lcd_stat = hardware.lcd->get_lcd_stat();
      hardware.lcd->set_lcd_stat(lcd_stat.write_value(val));
      return;
    }
    case Registers::Lcdc::Address:
      hardware.lcd->set_enabled(test_bit(val, 7));
      memory[addr] = val;
      return;
    case Registers::Scx::Address:
      hardware.gpu->scx = val;
      return;
    case Registers::Scy::Address:
      hardware.gpu->scy = val;
      return;

    case Registers::WindowX::Address:
      hardware.gpu->window_x = val;
      return;
    case Registers::WindowY::Address:
      hardware.gpu->window_y = val;
      return;
    // Sound
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
          oam_dma_task = OamDmaTransfer{static_cast<u16>(val << 8)};
          break;
        case 0xff02:
          if (val == 0x81) {
            std::cout << memory[0xff01];
          }
          break;

        default:
          if (addr >= 0x8000 && !mbc.in_ram_range(addr)) {
            const auto [resolved_addr, storage] = select_storage(addr);
            storage[resolved_addr] = val;
          }
          break;
      }
    }
  }
}

void Memory::update(int ticks) {
  visit_optional(oam_dma_task, [&](auto& task) {
    task.advance(ticks);
    bool canceled = task.for_each_cycle([&](int cycles) {
      if (cycles > 162) {
        return true;
      }
      if (cycles > 1) {
        int progress = cycles - 2;
        memory[0xfe00 + progress] = at(task.start_addr + progress);
      }
      return false;
    });
    if (canceled) {
      oam_dma_task = {};
    }
  });
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
  memory[0xFFFF] = 0x00;

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
