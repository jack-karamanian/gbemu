#pragma once
#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <nonstd/span.hpp>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include "mbc.h"
#include "registers/interrupt_enabled.h"
#include "registers/interrupt_request.h"
#include "registers/lcd_stat.h"
#include "sprite_attribute.h"
#include "task.h"
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

class OamDmaTransfer : public ManualTask {
 public:
  u16 start_addr;
  explicit OamDmaTransfer(u16 start) : ManualTask(4), start_addr{start} {}
};

class Cpu;
class Timers;
class Sound;
class Input;
class Lcd;
class Gpu;
class Memory;
class HdmaTransfer;

struct Hardware {
  Cpu* cpu = nullptr;
  Memory* memory = nullptr;
  HdmaTransfer* hdma = nullptr;
  Timers* timers = nullptr;
  Sound* sound = nullptr;
  Input* input = nullptr;
  Lcd* lcd = nullptr;
  Gpu* gpu = nullptr;
};

class HdmaTransfer {
  Memory* memory;
  int length = 0;
  int progress = 0;
  bool is_active = false;

 public:
  HdmaTransfer(Memory& memory_) : memory{&memory_} {}

  u8 source_high = 0;
  u8 source_low = 0;

  u8 dest_high = 0;
  u8 dest_low = 0;

  inline void transfer_bytes(int num_bytes);

  void start(u8 value) {
    const int num_bytes = ((value & 0x7f) + 1) * 16;
    if (is_active) {
      is_active = test_bit(value, 7);
      if (is_active) {
        progress = 0;
        length = num_bytes;
      }
    } else {
      progress = 0;
      length = num_bytes;
      is_active = test_bit(value, 7);
      if (!is_active) {
        transfer_bytes(length);
      }
    }
  }

  [[nodiscard]] bool active() const { return is_active; }

  [[nodiscard]] u16 source() const {
    return ((source_high << 8) | source_low) & 0xfff0;
  }

  [[nodiscard]] u16 dest() const {
    return 0x8000 + (((dest_high << 8) | dest_low) & 0x1ff0);
  }
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

  std::vector<u8> wram;
  std::vector<u8> extended_ram;

  nonstd::span<u8> memory_span;

  Mbc mbc;

  std::optional<OamDmaTransfer> oam_dma_task;

  SaveRamWriteListener save_ram_write_listener;

  std::optional<u8> read_hardware(u16 addr);
  std::pair<u16, nonstd::span<u8>> select_storage(u16 addr);

 public:
  Memory(Mbc mbc_)
      : memory(SIXTYFOUR_KB, 0),
        vram_bank1(0x2000, 0),
        wram(0x1000, 0),
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
            return storage[normalized_addr + offset--];
          });
      return convert_bytes<T>(bytes);
    }
  }

  void set_hardware(const Hardware& new_hardware) { hardware = new_hardware; }

  nonstd::span<const u8> get_range(std::pair<u16, u16> range);

  void set(u16 addr, u8 val);

  void update(int ticks);

  void reset();

  void load_rom(std::vector<u8>&& data);

  void load_save_ram(std::vector<u8>&& data) { save_ram = std::move(data); }

  void add_save_ram_write_listener(SaveRamWriteListener callback) {
    save_ram_write_listener = std::move(callback);
  }

  void do_dma_transfer(const u8& val);

  u8 get_ram(u16 addr) const { return memory[addr]; }

  void set_ram(u16 addr, u8 val) { memory[addr] = val; }

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

  nonstd::span<const u8> get_sprite_attributes() {
    const u8* sprite_attrib_begin = &memory[0xfe00];
    const u8* sprite_attrib_end = &memory[0xfea0];

    return {sprite_attrib_begin, sprite_attrib_end};
  }

  [[nodiscard]] nonstd::span<const u8> get_tile_atributes(u16 addr) const {
    const u8* begin = &vram_bank1[addr - 0x8000];

    return {begin, 1024};
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
};

inline void HdmaTransfer::transfer_bytes(int num_bytes) {
  const u16 source_addr = source() + progress;
  const u16 dest_addr = dest() + progress;

  for (int i = 0; i < num_bytes; ++i) {
    memory->set(dest_addr + i, memory->at(source_addr + i));
  }

  progress += num_bytes;

  if (progress >= length) {
    is_active = false;
    progress = 0;
  }
}

}  // namespace gb
