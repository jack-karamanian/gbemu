#pragma once
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>
#include "nonstd/span.hpp"
#include "sprite_attribute.h"
#include "utils.h"

#define SIXTEEN_KB 16384
#define SIXTYFOUR_KB 0x10000
#define TWO_MB 0x200000

namespace gb {
class RomBank {
  u8 lower = 0x01;
  u8 upper = 0x00;

 public:
  void set_lower(u8 val) { lower = val & 0x1f; }
  void set_upper(u8 val) { upper = val & 0x03; }

  u8 get_rom_bank_selected() { return (upper << 5) | lower; }
};

class Memory {
  std::vector<u8> memory;
  std::vector<u8> rom;

  bool external_ram_enabled = false;

  RomBank rom_bank_selected;

  std::pair<u16, nonstd::span<const u8>> select_storage(u16 addr);

 public:
  Memory() : memory(SIXTYFOUR_KB), rom(TWO_MB) {}
  template <typename T = u8>
  T at(u16 addr) {
    const auto [normalized_addr, storage] = select_storage(addr);
    if constexpr (sizeof(T) == 1) {
      return storage.at(normalized_addr);
    } else {
      std::array<u8, sizeof(T)> bytes;
      std::generate(
          bytes.begin(), bytes.end(),
          [this, storage, normalized_addr, offset = sizeof(T) - 1]() mutable {
            return storage.at(normalized_addr + offset--);
          });
      return convert_bytes<T>(bytes);
    }
  }

  nonstd::span<const u8> get_range(std::pair<u16, u16> range);

  void set(const u16& addr, const u8& val);

  void reset();

  void load_rom(const std::vector<u8>& data);

  void do_dma_transfer(const u8& val);

  u8 get_input_register();
  void set_input_register(u8 val);

  nonstd::span<const SpriteAttribute> get_sprite_attributes();
};
}  // namespace gb
