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

  Memory &memory;

  Cpu(Memory &memory) : memory(memory) {}

  u8 fetch() {
    return memory.memory[pc++];
  }

  void decode(const Memory &memory);

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

  template<typename T>
  void set_zero(const T &val) {
    if (val == 0) {
      set_flag(FLAG_ZERO);
    } else {
      clear_flag(FLAG_ZERO);
    }
  }

  void set_half_carry(const u16 &a, const u16 &b) {
    bool half_carry = (((a & 0xf) + (b & 0xf)) & 0x10) == 0x10;
    if (half_carry) {
      set_flag(FLAG_HALF_CARRY);
    } else {
      clear_flag(FLAG_HALF_CARRY);
    }
  }

  void add_carry(u8 &dst, u8 val, bool carry) {
    val += carry ? 1 : 0;
  }

  void inc_8(const Register& reg) {
    regs[reg]++;

    set_zero(regs[reg]);
    clear_flag(FLAG_SUBTRACT);
    set_half_carry(regs[reg] - 1, regs[reg]);
  }

  void inc_at_hl() {
    const u8 &addr = regs[Register::HL];
    u8 *val = memory.at(addr);
    (*val)++;

    set_zero(*val);
    clear_flag(FLAG_SUBTRACT);
  }

  inline void dec_8(const Register& reg) {
    regs[reg]--;
  }

  inline void inc_16(const Register& reg) {
    (*((u16 *) &regs[reg]))++;
    // 1 8
  }

  inline void dec_16(const Register& reg) {
    (*((u16 *) &regs[reg]))--;
    // 1 8
  }
  //void set_16(u8 &reg_high, u8 &reg_low, u16 val) {
  //  reg_high = (val & 0xFF00) >> 8;
  //  reg_low = (val & 0xFF);
  //}
  
  inline void set_16(const Register& dst, const u16& val) {
    *((u16 *) &regs[dst]) = val;
  }

  void load_16(const Register& dst) {
    u16 val = *memory.at<u16>(pc + 1);
    set_16(dst, val);
    pc += 3;
  }

  void load_reg_to_addr(const Register& dst, const Register& src) {
    u16 addr = regs[dst];
    memory.set(addr, src);
  }

  inline u16 get_16(const Register& reg) {
    u16 *res = (u16 *)&regs[reg];
    return *res;
  }
};
}
