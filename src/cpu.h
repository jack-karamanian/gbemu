#pragma once
#include <cstdint>
#include <functional>
#include <boost/integer_traits.hpp>
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

  //void set_half_carry(const u8 &a, const u8 &b) {
  //  bool half_carry = (((a & 0xf) + (b & 0xf)) & 0x10) == 0x10;
  //  if (half_carry) {
  //    set_flag(FLAG_HALF_CARRY);
  //  } else {
  //    clear_flag(FLAG_HALF_CARRY);
  //  }
  //}

  //void set_half_carry_16(const u16 &a, const u16 &b) {
  //  bool half_carry = (((a & 0xff) + (b & 0xff)) & 0x100) == 0x100;
  //  if (half_carry) {
  //   set_flag(FLAG_HALF_CARRY);
  //  } else {
  //    clear_flag(FLAG_HALF_CARRY);
  //  }
  //}

  template<typename T>
  void set_half_carry(const T &a, const T &b) {
    // Hardcode max of 16 bit for now. Could support more with templates
    constexpr T add_mask = 0xffff >> (16 - (sizeof(T) * 4));
    constexpr T carry_mask = 0x1 << 4 * sizeof(T);

    bool half_carry = (((a & add_mask) + (b & add_mask)) & carry_mask) == carry_mask;
    if (half_carry) {
      set_flag(FLAG_HALF_CARRY);
    } else {
      clear_flag(FLAG_HALF_CARRY);
    }
  }

  template<typename T>
  void set_carry(const T &a, const T &b) {
    typename next_largest_type<T>::type res = a + b;
    //if (res > 0xff) {
    if (res > boost::integer_traits<T>::const_max) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }
  }

  void carried_add(u8 &dest, const u8 &a, const u8 &b) {
    u8 carry = get_flag(FLAG_CARRY) ? 1 : 0;
    u8 res = a + b + carry;

    set_carry(a, res);
    set_half_carry(a, res);

    dest = res;

    set_zero(res);
    clear_flag(FLAG_SUBTRACT);
  }

  void add(u8 &dest, const u8 &a, const u8 &b) {
    u8 res = a + b;

    set_carry(a, res);
    set_half_carry(a, res);

    dest = res;

    set_zero(res);
    clear_flag(FLAG_SUBTRACT);
  }

  // ADC A,[HL]
  void add_carry_a_hl() {
    u8 &a = regs[Register::A];
    u16 hl = get_16(Register::HL);
    u8 val = *memory.at(hl);

    carried_add(a, a, val);
  }

  // ADC A,n8
  void add_carry_a_d8() {
    u8 &a = regs[Register::A];
    u8 val = read_value();
    
    carried_add(a, a, val);
  }

  // ADC A,r8
  void add_carry_a_r8(const Register &reg) {
    u8 &a = regs[Register::A];
    u8 &val = regs[reg];

    carried_add(a, a, val);
  }

  // ADD A,r8
  void add_a_r8(const Register &reg) {
    u8 &a = regs[Register::A];
    u8 &val = regs[reg];

    add(a, a, val);
  }

  // ADD A,[HL]
  void add_a_hl() {
    u8 &a = regs[Register::A];
    u16 addr = get_16(Register::HL);
    u8 val = *memory.at(addr);

    add(a, a, val);
  }

  // ADD A,n8
  void add_a_d8() {
    u8 &a = regs[Register::A];
    u8 val = read_value();

    add(a, a, val);
  }

  void inc_r8(const Register& reg) {
    u8 &r = regs[reg];
    u8 res = r + 1;

    set_zero(res);
    clear_flag(FLAG_SUBTRACT);
    set_half_carry(r, res);

    r = res;
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

  template<typename T = u8>
  T read_value() {
    T val = *memory.at<T>(pc + 1);
    pc += 1 + sizeof(T);
    return val;
  }

  u16 read_short() {
    u16 val = *memory.at<u16>(pc + 1);
    pc += 3;
    return val;
  }

  void load_16(const Register& dst) {
    u16 val = read_short();
    set_16(dst, val);
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
