#pragma once
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>
#include "mbc.h"
#include "nonstd/span.hpp"
#include "registers/interrupt_enabled.h"
#include "registers/interrupt_request.h"
#include "registers/lcd_stat.h"
#include "sprite_attribute.h"
#include "utils.h"

#define SIXTEEN_KB 16384
#define SIXTYFOUR_KB 0x10000
#define TWO_MB 0x200000

namespace gb {

struct RomHeader {
  Mbc::Type mbc_type;
  int rom_size;
  int save_ram_size;
};

class Memory {
  using MemoryListener = std::function<void(u8 val, u8 prev_val)>;
  using AddrMemoryListener = std::function<void(u16 addr, u8 val, u8 prev_val)>;
  using SaveRamWriteListener = std::function<void(std::size_t index, u8 val)>;

  std::vector<u8> memory;
  std::vector<u8> rom;
  std::vector<u8> save_ram;

  std::vector<u8> vram_bank1;

  std::vector<u8> extended_ram;

  nonstd::span<u8> memory_span;

  Mbc mbc;


  std::unordered_map<u16, MemoryListener> write_callbacks;
  SaveRamWriteListener save_ram_write_listener;

  std::pair<u16, nonstd::span<u8>> select_storage(u16 addr);

 public:
  Memory(Mbc mbc_)
      : memory(SIXTYFOUR_KB, 0),
        vram_bank1(0x2000, 0),
        extended_ram(0x1000 * 7, 0),
        memory_span{memory},
        mbc{mbc_} {}

  template <typename T = u8>
  T at(u16 addr) {
    const auto x = select_storage(addr);
    const auto& normalized_addr = x.first;
    const auto& storage = x.second;
    if constexpr (sizeof(T) == 1) {
      return storage.at(normalized_addr);
    } else {
      std::array<u8, sizeof(T)> bytes;
      std::generate(
          bytes.begin(), bytes.end(),
          [storage, normalized_addr, offset = sizeof(T) - 1]() mutable {
            return storage.at(normalized_addr + offset--);
          });
      return convert_bytes<T>(bytes);
    }
  }

  nonstd::span<const u8> get_range(std::pair<u16, u16> range);

  void set(u16 addr, u8 val);

  void reset();

  void load_rom(std::vector<u8>&& data);

  void load_save_ram(std::vector<u8>&& data) { save_ram = std::move(data); }

  void add_save_ram_write_listener(SaveRamWriteListener callback) {
    save_ram_write_listener = std::move(callback);
  }

  void add_write_listener(u16 addr, MemoryListener callback) {
    write_callbacks.emplace(addr, std::move(callback));
  }

  template <typename... Addrs>
  void add_write_listener_for_addrs(AddrMemoryListener callback,
                                    Addrs... args) {
    for (u16 addr : {args...}) {
      add_write_listener(addr, [callback, addr](u8 val, u8 prev_val) {
        callback(addr, val, prev_val);
      });
    }
  }

  void add_write_listener_for_range(u16 begin,
                                    u16 end,
                                    const AddrMemoryListener& callback);

  void do_dma_transfer(const u8& val);

  u8 get_ram(u16 addr) const { return memory[addr]; }

  void set_ram(u16 addr, u8 val) { memory[addr] = val; }

  u8 get_input_register() const { return memory[0xff00]; }
  void set_input_register(u8 val) { memory[0xff00] = val; }

  u8 get_lcd_stat() const { return memory[Registers::LcdStat::Address]; }
  void set_lcd_stat(u8 val) { memory[Registers::LcdStat::Address] = val; }

  u8 get_interrupts_enabled() const {
    return memory[Registers::InterruptEnabled::Address];
  }

  void set_interrupts_enabled(u8 val) {
    memory[Registers::InterruptEnabled::Address] = val;
  }

  u8 get_interrupts_request() const {
    return memory[Registers::InterruptRequest::Address];
  }

  void set_interrupts_request(u8 val) {
    memory[Registers::InterruptRequest::Address] = val;
  }

  void set_ly(u8 val) { memory[0xff44] = val; }

  nonstd::span<const SpriteAttribute> get_sprite_attributes();
};
}  // namespace gb
