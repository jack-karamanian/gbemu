#pragma once
#include <algorithm>
#include <array>
#include <functional>
#include <utility>
#include "nonstd/span.hpp"
#include "sprite_attribute.h"
#include "utils.h"

namespace gb {
class Memory {
  std::array<u8, 0x10000> memory;

 public:
  template <typename T = u8>
  T at(u16 addr) {
    if constexpr (sizeof(T) == 1) {
      return memory.at(addr);
    } else {
      std::array<u8, sizeof(T)> bytes;
      std::generate(bytes.begin(), bytes.end(),
                    [this, addr, offset = sizeof(T) - 1]() mutable {
                      return memory.at(addr + offset--);
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
