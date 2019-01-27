#pragma once
#include <array>
#include "types.h"

namespace gb {
struct Memory {
  std::array<u8, 0x10000> memory;

  template <typename T = u8>
  inline const T* at(u16 addr) {
    // assert(addr + (sizeof(T) - 1) < 0xffff);
    return reinterpret_cast<const T*>(&memory.at(addr));
  }

  void set(const u16& addr, const u8& val);

  void do_dma_transfer(const u8& val);
  const u8* get_input_register();
  void set_input_register(u8 val);
};
}  // namespace gb
