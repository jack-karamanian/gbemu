#pragma once
#include "system.h"
#include "types.h"

namespace gb {
struct Memory {
  const System* system;
  Memory(const System* system) : system{system} {}
  u8 memory[0xFFFF];

  template <typename T = u8>
  inline T* at(const u16& addr) {
    return reinterpret_cast<T*>(&memory[addr]);
  }

  void set(const u16& addr, const u8& val);

  void do_dma_transfer(const u8& val);
};
}  // namespace gb
