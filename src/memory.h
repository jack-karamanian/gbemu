#pragma once
#include <algorithm>
#include <cassert>
#include <functional>
#include <optional>
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

struct BgAttribute {
  u8 value;

  u8 bg_palette() const { return value & 0x07; }

  u8 vram_bank() const { return (value & 0x08) >> 3; }

  bool horizontal_flip() const { return test_bit(value, 5); }

  bool vertical_flip() const { return test_bit(value, 6); }

  bool bg_priority() const { return test_bit(value, 7); }
};

class Cpu;
class Timers;
class Sound;
class Input;
class Lcd;
class Gpu;
class Memory;

struct Hardware {
  Cpu* cpu = nullptr;
  Memory* memory = nullptr;
  Timers* timers = nullptr;
  Sound* sound = nullptr;
  Input* input = nullptr;
  Lcd* lcd = nullptr;
  Gpu* gpu = nullptr;
};

class Memory {
  using WriteListener = void (*)(u16, u8, const Hardware&);
  using SaveRamWriteListener = std::function<void(std::size_t index, u8 val)>;

  WriteListener write_listener = nullptr;
  Hardware hardware;

  std::vector<u8> memory;
  std::vector<u8> rom;
  std::vector<u8> save_ram;

  std::vector<u8> vram_bank1;

  std::vector<u8> extended_ram;

  nonstd::span<u8> memory_span;

  Mbc mbc;


  SaveRamWriteListener save_ram_write_listener;

  std::optional<u8> read_hardware(u16 addr);
  std::pair<u16, nonstd::span<u8>> select_storage(u16 addr);

 public:
  Memory(Mbc mbc_)
      : memory(SIXTYFOUR_KB, 0),
        vram_bank1(0x2000, 0),
        extended_ram(0x1000 * 7, 0),
        memory_span{memory},
        mbc{mbc_} {}

  void set_write_listener(WriteListener callback) { write_listener = callback; }

  template <typename T = u8>
  T at(u16 addr) {
    const auto hardware_value = read_hardware(addr);

    if (hardware_value) {
      return *hardware_value;
    }
    const auto x = select_storage(addr);
    const auto& normalized_addr = x.first;
    const auto& storage = x.second;
    if constexpr (sizeof(T) == 1) {
      assert(normalized_addr < storage.size());
      return storage[normalized_addr];
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

  void set_hardware(const Hardware& new_hardware) { hardware = new_hardware; }

  nonstd::span<const u8> get_range(std::pair<u16, u16> range);

  void set(u16 addr, u8 val);

  void reset();

  void load_rom(std::vector<u8>&& data);

  void load_save_ram(std::vector<u8>&& data) { save_ram = std::move(data); }

  void add_save_ram_write_listener(SaveRamWriteListener callback) {
    save_ram_write_listener = std::move(callback);
  }

  void do_dma_transfer(const u8& val);

  u8 get_ram(u16 addr) const { return memory[addr]; }

  void set_ram(u16 addr, u8 val) { memory[addr] = val; }

  u8 get_input_register() const { return memory[0xff00]; }
  void set_input_register(u8 val) { memory[0xff00] = val; }

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

  nonstd::span<const SpriteAttribute> get_sprite_attributes() {
    const u8* sprite_attrib_begin = &memory[0xfe00];
    const u8* sprite_attrib_end = &memory[0xfea0];

    return {reinterpret_cast<const SpriteAttribute*>(sprite_attrib_begin),
            reinterpret_cast<const SpriteAttribute*>(sprite_attrib_end)};
  }

  nonstd::span<const BgAttribute> get_background_attributes() const {
    const u8* begin = &vram_bank1[0x1800];  // 0x9800

    return {reinterpret_cast<const BgAttribute*>(begin), 1024};
  }

  nonstd::span<const BgAttribute> get_window_attributes() const {
    const u8* begin = &vram_bank1[0x1c00];  // 0x9c00

    return {reinterpret_cast<const BgAttribute*>(begin), 1024};
  }

  [[nodiscard]] nonstd::span<const BgAttribute> get_tile_atributes(
      u16 addr) const {
    const u8* begin = &vram_bank1[addr - 0x8000];

    return {reinterpret_cast<const BgAttribute*>(begin), 1024};
  }

  [[nodiscard]] nonstd::span<const u8> get_vram(int bank = 0) {
    switch (bank) {
      case 0:
        return {&memory[0x8000], 0x2000};
      case 1:
        return vram_bank1;
      default:
        throw std::runtime_error("invalid vram bank");
    }
  }

  [[nodiscard]] nonstd::span<const u8> get_vram_bank1() const {
    return vram_bank1;
  }
};
}  // namespace gb
