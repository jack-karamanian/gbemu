#pragma once
#include <cstdint>
#include <functional>
#include "types.h"
#include "memory.h"

#define FLAG_ZERO       0x80
#define FLAG_SUBTRACT   0x40
#define FLAG_HALF_CARRY 0x20
#define FLAG_CARRY      0x10

namespace gb {
struct Cpu {
  enum Register {
    B = 0,
    C,
    D,
    E,
    H,
    L,
    F,
    A,
    BC = 0,
    DE = 1,
    HL = 2
  };
  u8 regs[8];

  u8 sp;
  u8 pc;
  u8 m;
  u8 t;

  Memory *memory;

  Cpu(Memory &memory) : memory(&memory) {}

  u8 fetch() {
    return memory->memory[pc++];
  }

  void decode(Memory& memory);

  inline void noop() const {}

  inline bool get_carry() {
    return regs[Register::F] & 0x10;
  }

  inline bool get_flag(u8 flag) const {
    return regs[Register::F] & flag;
  }

  inline void set_flag(u8 flag) {
    regs[Register::F] |= flag;
  }

  inline void clear_flag(u8 flag) {
    regs[Register::F] &= ~flag;
  }

  void add_carry(u8 &dst, u8 val, bool carry) {
    val += carry ? 1 : 0;
  }

  //void set_16(u8 &reg_high, u8 &reg_low, u16 val) {
  //  reg_high = (val & 0xFF00) >> 8;
  //  reg_low = (val & 0xFF);
  //}
  
  inline void set_16(Register dst, u16 val) {
    *((u16 *) &regs[dst]) = val;
  }

  inline u16 get_16(Register reg) {
    u16 *res = (u16 *)&regs[reg];
    return *res;
  }
};
}
