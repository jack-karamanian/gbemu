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
  namespace JumpCondition {
    enum {
      NZ = 0,
      Z,
      NC,
      C,
    };
  };
struct Cpu {
  enum Register {
    C = 0,
    B,
    E,
    D,
    L,
    H,
    F,
    A,
    BC = 0,
    DE = 1,
    HL = 2
  };
  u8 regs[8];

  u16 sp;
  u16 pc;
  u8 m;
  u8 t;

  bool interrupts = true;

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

  template<typename T, typename U = T>
  void set_half_carry(const T &a, const U &b) {
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

  void set_half_carry_subtract(const u8 &a, const u8 &b) {
    bool half_carry = (b & 0x0f) > (a & 0x0f);

    if (half_carry) {
      set_flag(FLAG_HALF_CARRY);
    } else {
      clear_flag(FLAG_HALF_CARRY);
    }
  }

  template<typename T, typename U = T>
  void set_carry(const T &a, const U &b) {
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

  // ADD HL,r16
  void add_hl_r16(const Register &reg) {
    u16 &hl = get_r16(Register::HL);
    u16 &r16 = get_r16(reg);
    
    u16 res = hl + r16;

    set_half_carry(hl, r16);
    set_carry(hl, r16);
    clear_flag(FLAG_SUBTRACT);

    hl = res;
  }

  // ADD HL,SP
  void add_hl_sp() {
    u16 &hl = get_r16(Register::HL);
    u16 res = hl + sp;

    set_half_carry(hl, (u16)sp);
    set_carry(hl, (u16)sp);
    clear_flag(FLAG_SUBTRACT);

    hl = res;
  }

  // ADD SP,s8
  void add_sp_s8() {
    s8 val = read_value();
    int res = sp + val;

    if (res & 0xffff0000) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }

    set_half_carry(sp, val);

    sp = res;
  }

  void and_a(const u8& val) {
    u8 &a = regs[Register::A];

    a &= val;

    set_zero(a);
    set_flag(FLAG_HALF_CARRY);

    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_CARRY);
  }

  // AND A,r8
  void and_a_r8(const Register &reg) {
    u8 &r = regs[reg];
    and_a(r);
  }

  // AND A,[HL]
  void and_a_hl() {
    u16 &addr = get_r16(Register::HL);
    u8 *val = memory.at(addr);

    and_a(*val);
  }

  // AND A,n8
  void and_a_d8() {
    u8 val = read_value();

    and_a(val);
  }

  void bit(const u8 &bit_num, const u8 &val) {
    bool bit_set = val & (0x01 << bit_num);

    if (bit_set) {
      clear_flag(FLAG_ZERO);
    } else {
      set_flag(FLAG_ZERO); 
    }

    set_flag(FLAG_HALF_CARRY);
    clear_flag(FLAG_SUBTRACT);
  }

  // BIT u8,r8
  void bit_r8(u8 bit_num, const Register &reg) {
    u8 &val = regs[reg];
    bit(bit_num, val);
  }

  // BIT u3,[HL]
  void bit_hl(u8 bit_num) {
    u16 &addr = get_r16(Register::HL);
    u8 *val = memory.at(addr);
    bit(bit_num, *val);
  }

  // CALL,nn
  void call() {
    u16 addr = read_value<u16>();
    u16 next_op = pc + 3;
    u8 pc_low = (next_op & 0xff00) >> 8;
    u8 pc_high  = (next_op & 0x00ff);
    memory.set(--sp, pc_high);
    memory.set(--sp, pc_low);
    pc = addr;
  }

  // CALL Z,n16
  void call_z() {
    if (get_flag(FLAG_ZERO)) {
      call();
    }
  }

  // CALL NZ,n16
  void call_nz() {
    if (!get_flag(FLAG_ZERO)) {
      call();
    }
  }

  // CALL C,n16
  void call_c() {
    if (get_flag(FLAG_CARRY)) {
      call();
    }
  }

  // CALL NC,n16
  void call_nc() {
    if (!get_flag(FLAG_CARRY)) {
      call();
    }
  }

  // CCF
  void ccf() {
    regs[Register::F] &= ~(regs[Register::F] & FLAG_CARRY);
  }

  void compare_a(const u8 &val) {
    u8 &a = regs[Register::A];

    u8 res = a - val;

    set_zero(res);
    set_half_carry_subtract(a, val);

    if (val > a) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }

    clear_flag(FLAG_SUBTRACT);
  }

  // CP A,[HL]
  void cp_a_hl() {
    u8 &addr = regs[Register::HL];
    u8 *val = memory.at(addr);

    compare_a(*val);
  }

  // CP A,n8
  void cp_a_d8() {
    u8 val = read_value();
    compare_a(val);
  }

  // CPL
  void cpl() {
    regs[Register::A] = ~regs[Register::A];
    set_flag(FLAG_SUBTRACT);
    set_flag(FLAG_HALF_CARRY);
  }

  // DAA
  // from https://github.com/taisel/GameBoy-Online/blob/master/js/GameBoyCore.js#L588
  void daa() {
    const bool carry = get_flag(FLAG_CARRY);
    const bool subtract = get_flag(FLAG_SUBTRACT);
    const bool half_carry = get_flag(FLAG_HALF_CARRY);

    u8 &a = regs[Register::A];

    if (!subtract) {
      if (carry || a > 0x99) {
        a = (a + 0x60) + 0xff;
        set_flag(FLAG_CARRY);
      }
      if (half_carry || (a & 0xf) > 0x9) {
        a = (a + 0x6) & 0xff;
        clear_flag(FLAG_HALF_CARRY);
      }
    } else if (carry && half_carry) {
      a = (a + 0x9a) & 0xff;
      clear_flag(FLAG_HALF_CARRY);
    } else if (carry) {
      a = (a + 0xa0) & 0xff;
    } else if (half_carry) {
      a = (a + 0xfa) & 0xff;
      clear_flag(FLAG_HALF_CARRY);
    }
    set_zero(a);
  }

  void dec(u8 &val) {
    u8 res = val - 1;

    set_zero(res);
    set_half_carry_subtract(val, 1);

    val = res;
  }

  // DEC r8
  void dec_r8(const Register &reg) {
    dec(regs[reg]);
  }

  // DEC [HL]
  void dec_hl() {
    u8 &val = value_at_r16(Register::HL);
    dec(val);
  }

  // DEC r16
  void dec_r16(const Register &reg) {
    u16 &val = get_r16(reg);
    val--;
  }

  // DEC SP
  void dec_sp() {
    sp--;
  }

  // DI
  void disable_interrupts() {
    interrupts = false;
  }

  // EI
  void enable_interrupts() {
    interrupts = true;
  }

  void halt() {
  }

  void inc(u8 &val) {
    u8 res = val + 1;

    set_half_carry(val, 1);
    set_zero(res);
    clear_flag(FLAG_SUBTRACT);

    val = res;
  }

  // INC r8
  void inc_r8(const Register &reg) {
    inc(regs[reg]);
  }

  // INC [HL]
  void inc_hl() {
    u8 &val = value_at_r16(Register::HL);
    inc(val);
  }

  // INC r16
  void inc_r16(const Register &reg) {
    u16 &r16 = get_r16(reg);
    r16++;
  }

  // INC SP
  void inc_sp() {
    sp++;
  }

  inline void jump(const u16 &addr) {
    pc = addr;
  }

  // JP n16
  void jp_d16() {
    u16 addr = read_value<u16>();
    jump(addr);
  }

  bool can_jump(const u8 &opcode, int offset) {
    int index = ((opcode & 0x38) >> 3) - offset;

    return (
      (index == JumpCondition::NZ && !get_flag(FLAG_ZERO))
      || (index == JumpCondition::Z && get_flag(FLAG_ZERO))
      || (index == JumpCondition::NC && !get_flag(FLAG_CARRY))
      || (index == JumpCondition::C && get_flag(FLAG_CARRY))
    );
  }

  void jump_conditional(const u16 &addr, int index_offset = 0) {
    const u8 opcode = *memory.at(pc);

    if (can_jump(opcode, index_offset)) {
      jump(addr);
    }
  } 

  // JP cc,n16
  void jp_cc_n16() {
    u16 addr = read_value<u16>();
    jump_conditional(addr);
  }

  // JP HL
  void jp_hl() {
    const u16 &hl = get_r16(Register::HL);
    pc = hl;
  }

  // JR e8
  void jr_e8() {
    const u8 &offset = read_value();
    jump(pc + offset);
  }

  // JR cc,e8
  void jr_cc_e8() {
    const u8 &offset = read_value();
    jump_conditional(pc + offset, 4);
  }

  template<typename T>
  void load(T &dst, T &val) {
    dst = val;
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
 //   pc += 1 + sizeof(T);
    return val;
  }

  void load_reg_to_addr(const Register& dst, const Register& src) {
    u16 addr = regs[dst];
    memory.set(addr, src);
  }

  inline u16 get_16(const Register& reg) {
    u16 *res = (u16 *)&regs[reg];
    return *res;
  }

  inline u16& get_r16(const Register &reg) {
    return (u16&) *&regs[reg];
  }

  inline u8& value_at_r16(const Register &reg) {
    u16 &addr = get_r16(Register::HL);
    u8 *val = memory.at(addr);
    return *val;
  }
};
}
