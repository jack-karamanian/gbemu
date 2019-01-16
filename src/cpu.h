#pragma once
#include <array>
#include <boost/integer_traits.hpp>
#include <cstdint>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include "memory.h"
#include "types.h"

#define FLAG_ZERO 0x80
#define FLAG_SUBTRACT 0x40
#define FLAG_HALF_CARRY 0x20
#define FLAG_CARRY 0x10

namespace gb {
struct Cpu;
struct Instruction {
  std::string name;
  size_t size;
  size_t cycles;
  std::function<void()> impl;

  Instruction() = delete;
};

struct InstructionTable {
  gb::Cpu* cpu;

  std::array<Instruction, 256> instructions;
  std::array<Instruction, 256> cb_instructions = {{}};

  InstructionTable() = delete;

  InstructionTable(Cpu& cpu);
};
namespace JumpCondition {
enum {
  NZ = 0,
  Z,
  NC,
  C,
};
};
struct Cpu {
  enum Register { C = 0, B, E, D, L, H, F, A, BC = 0, DE = 2, HL = 4 };
  enum Interrupt {
    VBlank = 0x01,
    LcdStat = 0x2,
    Timer = 0x4,
    Serial = 0x08,
    Joypad = 0x10,
  };
  enum MemoryRegister {
    Interrupts = 0xff0f,
  };
  u8 regs[8];

  u16 sp;
  u16 pc;
  u8 m;
  u8 t;

  u8& a;
  u8& b;
  u8& c;
  u16& bc;
  u8& d;
  u8& e;
  u16& de;
  u8& h;
  u8& l;
  u16& hl;
  u8& f;

  bool interrupts_enabled = true;

  Memory& memory;
  InstructionTable instruction_table;

  Cpu(Memory& memory);

  const Instruction& fetch();

  void fetch_and_decode();
  void handle_interrupts();
  bool handle_interrupt(Interrupt interrupt);
  void debug_write();

  inline u8* get_interrupts_register() {
    return memory.at(MemoryRegister::Interrupts);
  }

  inline bool has_interrupt(Interrupt interrupt) {
    return *memory.at(MemoryRegister::Interrupts) & interrupt;
  }

  inline void set_interrupt(Interrupt interrupt) {
    *memory.at(MemoryRegister::Interrupts) |= interrupt;
  }

  inline void clear_interrupt(Interrupt interrupt) {
    *memory.at(0xff0f) = ~interrupt;
  }

  inline void noop() const {}
  inline void invalid() {
    std::ostringstream s;
    s << "invalid instruction: " << std::hex << +memory.memory[pc] << std::endl;
    throw std::runtime_error(s.str());
    // std::cout << s.str();
  }

  inline bool get_carry() { return regs[Register::F] & 0x10; }

  inline bool get_flag(u8 flag) const { return regs[Register::F] & flag; }

  inline void set_flag(u8 flag) { regs[Register::F] |= flag; }

  inline void clear_flag(u8 flag) { regs[Register::F] &= ~flag; }

  template <typename T>
  void set_zero(const T& val) {
    if (val == 0) {
      set_flag(FLAG_ZERO);
    } else {
      clear_flag(FLAG_ZERO);
    }
  }

  // void set_half_carry(const u8 &a, const u8 &b) {
  //  bool half_carry = (((a & 0xf) + (b & 0xf)) & 0x10) == 0x10;
  //  if (half_carry) {
  //    set_flag(FLAG_HALF_CARRY);
  //  } else {
  //    clear_flag(FLAG_HALF_CARRY);
  //  }
  //}

  // void set_half_carry_16(const u16 &a, const u16 &b) {
  //  bool half_carry = (((a & 0xff) + (b & 0xff)) & 0x100) == 0x100;
  //  if (half_carry) {
  //   set_flag(FLAG_HALF_CARRY);
  //  } else {
  //    clear_flag(FLAG_HALF_CARRY);
  //  }
  //}

  template <typename T, typename U = T>
  void set_half_carry(const T& a, const U& b) {
    // Hardcode max of 16 bit for now. Could support more with templates
    constexpr T add_mask = 0xffff >> (16 - (sizeof(T) * 4));
    constexpr T carry_mask = 0x1 << 4 * sizeof(T);

    bool half_carry =
        (((a & add_mask) + (b & add_mask)) & carry_mask) == carry_mask;
    if (half_carry) {
      set_flag(FLAG_HALF_CARRY);
    } else {
      clear_flag(FLAG_HALF_CARRY);
    }
  }

  void set_half_carry_subtract(const u8& a, const u8& b) {
    bool half_carry = (b & 0x0f) > (a & 0x0f);

    if (half_carry) {
      set_flag(FLAG_HALF_CARRY);
    } else {
      clear_flag(FLAG_HALF_CARRY);
    }
  }

  template <typename T, typename U = T>
  void set_carry(const T& a, const U& b) {
    typename next_largest_type<T>::type res = a + b;
    // if (res > 0xff) {
    if (res > boost::integer_traits<T>::const_max) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }
  }

  void carried_add(u8& dest, const u8& a, const u8& b) {
    u8 carry = get_flag(FLAG_CARRY) ? 1 : 0;
    u8 res = a + b + carry;

    set_carry(a, res);
    set_half_carry(a, res);

    dest = res;

    set_zero(res);
    clear_flag(FLAG_SUBTRACT);
  }

  void add(u8& dest, const u8& a, const u8& b) {
    u8 res = a + b;

    set_carry(a, res);
    set_half_carry(a, res);

    dest = res;

    set_zero(res);
    clear_flag(FLAG_SUBTRACT);
  }

  // ADC A,[HL]
  void add_carry_a_hl() {
    u8& a = regs[Register::A];
    u16 hl = get_16(Register::HL);
    u8 val = *memory.at(hl);

    carried_add(a, a, val);
  }

  // ADC A,n8
  void add_carry_a_d8() {
    u8& a = regs[Register::A];
    u8 val = read_value();

    carried_add(a, a, val);
  }

  // ADC A,r8
  void add_carry_a_r8(const Register& reg) {
    u8& a = regs[Register::A];
    u8& val = regs[reg];

    carried_add(a, a, val);
  }

  // ADD A,r8
  void add_a_r8(const Register& reg) {
    u8& a = regs[Register::A];
    u8& val = regs[reg];

    add(a, a, val);
  }

  // ADD A,[HL]
  void add_a_hl() {
    u8& a = regs[Register::A];
    u16 addr = get_16(Register::HL);
    u8 val = *memory.at(addr);

    add(a, a, val);
  }

  // ADD A,n8
  void add_a_d8() {
    u8& a = regs[Register::A];
    u8 val = read_value();

    add(a, a, val);
  }

  // ADD HL,r16
  void add_hl_r16(const Register& reg) {
    u16& hl = get_r16(Register::HL);
    u16& r16 = get_r16(reg);

    u16 res = hl + r16;

    set_half_carry(hl, r16);
    set_carry(hl, r16);
    clear_flag(FLAG_SUBTRACT);

    hl = res;
  }

  // ADD HL,SP
  void add_hl_sp() {
    u16& hl = get_r16(Register::HL);
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

    clear_flag(FLAG_ZERO);
    clear_flag(FLAG_SUBTRACT);

    sp = res;
  }

  void and_a(const u8& val) {
    u8& a = regs[Register::A];

    a &= val;

    set_zero(a);
    set_flag(FLAG_HALF_CARRY);

    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_CARRY);
  }

  // AND A,r8
  void and_a_r8(const Register& reg) {
    u8& r = regs[reg];
    and_a(r);
  }

  // AND A,[HL]
  void and_a_hl() {
    u16& addr = get_r16(Register::HL);
    u8* val = memory.at(addr);

    and_a(*val);
  }

  // AND A,n8
  void and_a_d8() {
    u8 val = read_value();

    and_a(val);
  }

  void bit(const u8& bit_num, const u8& val) {
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
  void bit_r8(u8 bit_num, const Register& reg) {
    u8& val = regs[reg];
    bit(bit_num, val);
  }

  // BIT u3,[HL]
  void bit_hl(u8 bit_num) {
    u16& addr = get_r16(Register::HL);
    u8* val = memory.at(addr);
    bit(bit_num, *val);
  }

  // CALL,nn
  void call() {
    u16 addr = read_value<u16>();
    u16 next_op = pc + 2;
    // u8 pc_low = (next_op & 0xff00) >> 8;
    // bu8 pc_high = (next_op & 0x00ff);
    // bmemory.set(--sp, pc_high);
    // memory.set(--sp, pc_low);
    push(next_op);
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
  void ccf() { regs[Register::F] &= ~(regs[Register::F] & FLAG_CARRY); }

  void compare_a(const u8& val) {
    u8& a = regs[Register::A];

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

  void cp_a_r8(const Register& reg) { compare_a(regs[reg]); }

  // CP A,[HL]
  void cp_a_hl() {
    u8& addr = regs[Register::HL];
    u8* val = memory.at(addr);

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
  // from
  // https://github.com/taisel/GameBoy-Online/blob/master/js/GameBoyCore.js#L588
  void daa() {
    const bool carry = get_flag(FLAG_CARRY);
    const bool subtract = get_flag(FLAG_SUBTRACT);
    const bool half_carry = get_flag(FLAG_HALF_CARRY);

    u8& a = regs[Register::A];

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

  void dec(u8& val) {
    u8 res = val - 1;

    set_zero(res);
    set_half_carry_subtract(val, 1);

    val = res;
  }

  // DEC r8
  void dec_r8(const Register& reg) { dec(regs[reg]); }

  // DEC [HL]
  void dec_hl() {
    u8& val = value_at_r16(Register::HL);
    dec(val);
  }

  // DEC r16
  void dec_r16(const Register& reg) {
    u16& val = get_r16(reg);
    val--;
  }

  // DEC SP
  void dec_sp() { sp--; }

  // DI
  void disable_interrupts() { interrupts_enabled = false; }

  // EI
  void enable_interrupts() { interrupts_enabled = true; }

  void halt() {}

  void inc(u8& val) {
    u8 res = val + 1;

    set_half_carry(val, 1);
    set_zero(res);
    clear_flag(FLAG_SUBTRACT);

    val = res;
  }

  // INC r8
  void inc_r8(const Register& reg) { inc(regs[reg]); }

  // INC [HL]
  void inc_hl() {
    u8& val = value_at_r16(Register::HL);
    inc(val);
  }

  // INC r16
  void inc_r16(const Register& reg) {
    u16& r16 = get_r16(reg);
    r16++;
  }

  // INC SP
  void inc_sp() { sp++; }

  inline void jump(const u16& addr) { pc = addr - 3; }

  // JP n16
  void jp_d16() {
    u16 addr = read_value<u16>();
    jump(addr);
  }

  bool can_jump(const u8& opcode, int offset) {
    int index = ((opcode & 0x38) >> 3) - offset;

    return ((index == JumpCondition::NZ && !get_flag(FLAG_ZERO)) ||
            (index == JumpCondition::Z && get_flag(FLAG_ZERO)) ||
            (index == JumpCondition::NC && !get_flag(FLAG_CARRY)) ||
            (index == JumpCondition::C && get_flag(FLAG_CARRY)));
  }

  void jump_conditional(const u16& addr, int index_offset = 0) {
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
    const u16& hl = get_r16(Register::HL);
    pc = hl;
  }

  // JR e8
  void jr_e8() {
    const u8& offset = read_value();
    jump(pc + offset);
  }

  // JR cc,e8
  void jr_cc_e8() {
    const u8& offset = read_value();
    jump_conditional(pc + offset, 4);
  }

  // LD r8,r8
  void ld_r8_r8(const Register& dst, const Register& src) {
    regs[dst] = regs[src];
  }

  // LD r8,n8
  void ld_r8_d8(const Register& dst) {
    const u8& val = read_value();
    regs[dst] = val;
  }

  // LD r16,n16
  void ld_r16_d16(const Register& dst) {
    u16& r16 = get_r16(dst);
    const u16& val = read_value<u16>();

    r16 = val;
  }

  // LD [HL],r8
  void ld_hl_r8(const Register& reg) {
    const u16& hl = get_r16(Register::HL);
    u8* val = memory.at(hl);
    *val = regs[reg];
  }

  // LD [HL],n8
  void ld_hl_d8() {
    const u16& hl = get_r16(Register::HL);
    u8* val = memory.at(hl);
    *val = read_value();
  }

  // LD r8,[HL]
  void ld_r8_hl(const Register& reg) {
    const u16& hl = get_r16(Register::HL);
    u8* val = memory.at(hl);
    regs[reg] = *val;
  }

  // LD [r16], A
  void ld_r16_a(const Register& reg) {
    u16& r16 = get_r16(reg);
    r16 = regs[Register::A];
  }

  // LD [n16],A
  void ld_d16_a() {
    const u16& addr = read_value<u16>();
    u8* val = memory.at(addr);
    *val = regs[Register::A];
  }

  void load_offset(const u8& offset, const u8& val) {
    u8* dest = memory.at(0xFF00 + offset);
    *dest = val;
  }

  // LD [$FF00 + n8],A
  void ld_offset_a() {
    const u8& offset = read_value();
    load_offset(offset, regs[Register::A]);
  }

  // LD [$FF00 + C],A
  void ld_offset_c_a() { load_offset(regs[Register::C], regs[Register::A]); }

  // LD A,[r16]
  void ld_a_r16(const Register& reg) {
    const u8& val = value_at_r16(reg);
    regs[Register::A] = val;
  }

  // LD A,[n16]
  void ld_a_d16() {
    const u16& addr = read_value<u16>();
    const u8* val = memory.at(addr);
    regs[Register::A] = *val;
  }

  void read_offset_from_memory(const u8& offset, u8& dest) {
    const u8* val = memory.at(0xff00 + offset);
    dest = *val;
  }

  // LD A,[$FF00 + n8]
  void ld_read_offset_d8() {
    const u8& offset = read_value();
    read_offset_from_memory(offset, regs[Register::A]);
  }

  // LD A,[$FF00 + C]
  void ld_read_offset_c() {
    read_offset_from_memory(regs[Register::C], regs[Register::A]);
  }

  // TODO: fix this
  void load_hl_a() {
    u16& hl = get_r16(Register::HL);
    u8* dst = memory.at(hl);
    *dst = regs[Register::A];
  }

  void load_a_hl() {
    const u8& val = value_at_r16(Register::HL);
    regs[Register::A] = val;
  }

  // LD [HL+],A
  void ld_hl_inc_a() {
    load_hl_a();
    inc_r16(Register::HL);
  }

  // LD [HL-],A
  void ld_hl_dec_a() {
    load_hl_a();
    dec_r16(Register::HL);
  }

  // LD A,[HL+]
  void ld_a_hl_inc() {
    load_a_hl();
    inc_r16(Register::HL);
  }

  // LD A,[HL-]
  void ld_a_hl_dec() {
    load_a_hl();
    dec_r16(Register::HL);
  }

  // LD SP,n16
  void ld_sp_d16() {
    const u16& val = read_value<u16>();
    sp = val;
  }

  // LD [n16],SP
  void ld_d16_sp() {
    const u16& addr = read_value<u16>();
    u16* val = memory.at<u16>(addr);
    *val = sp;
  }

  // LD HL,SP+e8
  void ld_hl_sp_s8() {
    const s8& val = read_value<s8>();
    int res = sp + val;
    if (res & 0xffff0000) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }

    set_half_carry(sp, val);

    clear_flag(FLAG_ZERO);
    clear_flag(FLAG_SUBTRACT);

    sp = res;
  }

  // LD SP,HL
  void ld_sp_hl() {
    const u16& hl = get_r16(Register::HL);
    sp = hl;
  }

  void or_a(const u8& val) {
    regs[Register::A] |= val;
    set_zero(regs[Register::A]);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
    clear_flag(FLAG_CARRY);
  }

  // OR A,r8
  void or_a_r8(const Register& reg) { or_a(regs[reg]); }

  // OR A,[HL]
  void or_a_hl() { or_a(value_at_r16(Register::HL)); }

  // OR A,n8
  void or_a_d8() { or_a(read_value()); }

  void pop(u16& reg) {
    u8 low = *memory.at(sp);
    u8 high = *memory.at(sp + 1);
    sp += 2;

    reg = (((u16)low) << 8) | high;
    std::cout << "popped " << std::hex << +reg << std::endl;
  }

  // POP AF
  void pop_af() {
    u16& af = get_r16(Register::F);
    pop(af);
  }

  // POP r16
  void pop_r16(const Register& reg) { pop(get_r16(reg)); }

  void push(const u16& val) {
    u8 low = (val & 0xff00) >> 8;
    u8 high = val & 0xff;

    memory.set(--sp, high);
    memory.set(--sp, low);
  }

  // PUSH AF
  void push_af() { push(get_r16(Register::F)); }

  // PUSH r16
  void push_r16(const Register& reg) { push(get_r16(reg)); }

  void set_bit(u8& dest, const u8& bit, bool set) {
    const u8 bit_mask = 0x1 << bit;
    if (set) {
      dest |= bit_mask;
    } else {
      dest &= ~bit_mask;
    }
  }

  // RES u3,r8
  void res_u3_r8(const u8 bit, const Register& reg) {
    set_bit(regs[reg], bit, false);
  }

  // RES u3,[HL]
  void res_u3_hl(const u8 bit) {
    set_bit(value_at_r16(Register::HL), bit, false);
  }

  // RET
  void ret() {
    pop(pc);
    pc--;
  }

  // RET,cc
  void ret_conditional() {
    u8 opcode = *memory.at(pc);
    if (can_jump(opcode, 0)) {
      ret();
    }
  }

  // RETI
  void reti() {
    ret();
    enable_interrupts();
  }

  void rotate(u8& val, bool left = true) {
    const bool did_carry = get_flag(FLAG_CARRY);

    const bool set_carry = (val & (left ? 0x80 : 0x01)) != 0;

    if (left) {
      val <<= 1;
    } else {
      val >>= 1;
    }

    if (did_carry) {
      if (left) {
        val |= 0x01;
      } else {
        val |= 0x80;
      }
    }

    if (set_carry) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }
  }

  void rotate_zero(u8& val, bool left = true) {
    rotate(val, left);
    set_zero(val);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
  }

  // RL r8
  void rl_r8(const Register& reg) { rotate_zero(regs[reg]); }

  // RL, [HL]
  void rl_hl() {
    u8& val = value_at_r16(Register::HL);
    rotate_zero(val);
  }

  // RLA
  void rl_a() {
    rotate(regs[Register::A]);

    clear_flag(FLAG_ZERO);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
  }

  void rotate_carry(u8& val, bool left = true) {
    int carry_val = (left ? 0x80 : 0x01);

    if (left) {
      val <<= 1;
    } else {
      val >>= 1;
    }

    if (val & carry_val) {
      set_flag(FLAG_CARRY);
      if (left) {
        val |= 0x01;
      } else {
        val |= 0x80;
      }
    } else {
      clear_flag(FLAG_CARRY);
    }
  }

  void rotate_carry_zero(u8& val, bool left = true) {
    rotate_carry(val, left);
    set_zero(val);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
  }

  // RLC r8
  void rlc_r8(const Register& reg) { rotate_carry_zero(regs[reg]); }

  // RLC [HL]
  void rlc_hl() {
    u8& val = value_at_r16(Register::HL);
    rotate_carry_zero(val);
  }

  // RLCA
  void rlca() {
    rotate_carry(regs[Register::A]);
    clear_flag(FLAG_ZERO);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
  }

  // RR r8
  void rr_r8(const Register& reg) { rotate_zero(regs[reg], false); }

  // RR [HL]
  void rr_hl() {
    u8& val = value_at_r16(Register::HL);
    rotate_zero(val, false);
  }

  // RRA
  void rra() {
    rotate(regs[Register::A], false);
    clear_flag(FLAG_ZERO);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
  }

  // RRC r8
  void rrc_r8(const Register& reg) { rotate_carry_zero(regs[reg], false); }

  // RRC [HL]
  void rrc_hl() { rotate_carry_zero(value_at_r16(Register::HL), false); }

  // RRCA
  void rrca() {
    rotate_carry(regs[Register::A], false);
    clear_flag(FLAG_ZERO);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
  }

  // RST vec
  void rst() {
    u8 opcode = *memory.at(pc);
    push(pc);

    u16 addr = opcode & 0x38;
    pc = addr;
  }

  void carried_subtract(u8& dst, const u8& src) {
    u8 carry = get_flag(FLAG_CARRY) ? 1 : 0;
    u8 res = dst - src - carry;

    set_half_carry_subtract(dst, src);
    set_zero(res);

    if (src > dst) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }
    set_flag(FLAG_SUBTRACT);

    dst = res;
  }

  // SBC A,r8
  void sbc_a_r8(const Register& reg) {
    carried_subtract(regs[Register::A], regs[reg]);
  }

  // SBC A,[HL]
  void sbc_a_hl() {
    carried_subtract(regs[Register::A], value_at_r16(Register::HL));
  }

  // SBC A,n8
  void sbc_a_d8() { carried_subtract(regs[Register::A], read_value()); }

  // SCF
  void scf() {
    set_flag(FLAG_CARRY);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_HALF_CARRY);
  }

  // SET u3,r8
  void set_u3_r8(const u8& bit, const Register& reg) {
    set_bit(regs[reg], bit, true);
  }

  // SET u3,[HL]
  void set_u3_hl(const u8& bit) {
    set_bit(value_at_r16(Register::HL), bit, true);
  }

  void shift_arithmetic(u8& val, bool left = true) {
    bool is_carry = val & (left ? 0x80 : 0x01);

    if (left) {
      val <<= 1;
    } else {
      u8 bit7 = val & 0x80;
      val >>= 1;
      val |= bit7;
    }

    if (is_carry) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }

    set_zero(val);
    clear_flag(FLAG_HALF_CARRY);
    clear_flag(FLAG_SUBTRACT);
  }

  // SLA r8
  void sla_r8(const Register& reg) { shift_arithmetic(regs[reg]); }

  // SLA [HL]
  void sla_hl() { shift_arithmetic(value_at_r16(Register::HL)); }

  // SRA r8
  void sra_r8(const Register& reg) { shift_arithmetic(regs[reg], false); }

  // SRA [HL]
  void sra_hl() { shift_arithmetic(value_at_r16(Register::HL), false); }

  void shift(u8& val) {
    bool is_carry = val & 0x01;

    val >>= 1;

    if (is_carry) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }

    set_zero(val);
    clear_flag(FLAG_HALF_CARRY);
    clear_flag(FLAG_SUBTRACT);
  }

  // SRL r8
  void srl_r8(const Register& reg) { shift(regs[reg]); }

  // SRL [HL]
  void srl_hl() { shift(value_at_r16(Register::HL)); }

  // STOP
  void stop() {}

  void subtract(u8& dst, const u8& src) {
    u8 res = dst - src;

    set_half_carry_subtract(dst, src);
    if (src > dst) {
      set_flag(FLAG_CARRY);
    } else {
      clear_flag(FLAG_CARRY);
    }
    set_zero(res);
    set_flag(FLAG_SUBTRACT);

    dst = src;
  }

  // SUB A,r8
  void sub_a_r8(const Register& reg) { subtract(regs[Register::A], regs[reg]); }

  // SUB A,[HL]
  void sub_a_hl() { subtract(regs[Register::A], value_at_r16(Register::HL)); }

  // SUB A,n8
  void sub_a_d8() { subtract(regs[A], read_value()); }

  void swap(u8& val) {
    u8 high = (val & 0xf0) >> 4;
    u8 low = (val & 0xf);
    val = (low << 4) | high;
  }

  // SWAP r8
  void swap_r8(const Register& reg) { swap(regs[reg]); }

  // SWAP [HL]
  void swap_hl() { swap(value_at_r16(Register::HL)); }

  // clang thinks xor is block related?
  void exclusive_or(u8& dst, const u8& src) {
    dst ^= src;
    set_zero(dst);
    clear_flag(FLAG_SUBTRACT);
    clear_flag(FLAG_CARRY);
    clear_flag(FLAG_HALF_CARRY);
  }

  // XOR A,r8
  void xor_a_r8(const Register& reg) {
    exclusive_or(regs[Register::A], regs[reg]);
  }

  // XOR A,[HL]
  void xor_a_hl() {
    exclusive_or(regs[Register::A], value_at_r16(Register::HL));
  }

  // XOR A,n8
  void xor_a_d8() { exclusive_or(regs[Register::A], read_value()); }

  // void set_16(u8 &reg_high, u8 &reg_low, u16 val) {
  //  reg_high = (val & 0xFF00) >> 8;
  //  reg_low = (val & 0xFF);
  //}

  template <typename T = u8>
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
    u16* res = (u16*)&regs[reg];
    return *res;
  }

  inline u16& get_r16(const Register& reg) { return (u16&)*&regs[reg]; }

  inline u8& value_at_r16(const Register& reg) {
    u16& addr = get_r16(reg);
    u8* val = memory.at(addr);
    return *val;
  }
};
}  // namespace gb
