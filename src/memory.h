#pragma once
#include "types.h"

namespace gb {
struct Memory {
  u8 memory[0xFFFF];

  template<typename T = u8>
  inline T* at(const u16 &addr) {
    return reinterpret_cast<T *>(&memory[addr]);
  } 

  void set(const u16 &addr, const u8 &val) {
    memory[addr] = val;
  }

};
}
