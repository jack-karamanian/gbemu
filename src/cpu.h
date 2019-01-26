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
}
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
    InterruptRequest = 0xff0f,
    InterruptEnabled = 0xffff,
    LcdControl = 0xff40,
  };
  u8 regs[8];

  u16 sp;
  u16 pc;
  u8 m;
  unsigned int ticks;

  u8 current_opcode = 0x00;
  u8* current_operand;

  bool interrupts_enabled = false;
  bool stopped = false;
  bool halted = false;

  Memory* memory;
  InstructionTable instruction_table;

  Cpu(Memory& memory);

  const Instruction& fetch();

  int fetch_and_decode();
  int handle_interrupts();
  bool handle_interrupt(u8 interrupt);
  void debug_write();

  u8* get_interrupts_register() const;

  bool interrupt_enabled(u8 interrupt) const;

  bool has_interrupt(u8 interrupt) const;

  void request_interrupt(Interrupt interrupt);

  void clear_interrupt(const u8 interrupt) const;

  inline void noop() const {}
  void invalid() const;

  inline bool get_carry() const { return regs[Register::F] & 0x10; }

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
  template <typename T = u8>
  T read_operand() {
    return *reinterpret_cast<T*>(current_operand);
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

  template <typename T>
  void set_half_carry(const T& a, const T& b) {
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

  void set_half_carry_subtract(const u8& a, const u8& b);

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

  void carried_add(u8& dest, const u8& a, const u8& b);

  void add(u8& dest, const u8& a, const u8& b);

  // ADC A,[HL]
  void add_carry_a_hl();

  // ADC A,n8
  void add_carry_a_d8();

  // ADC A,r8
  void add_carry_a_r8(const Register& reg);

  // ADD A,r8
  void add_a_r8(const Register& reg);

  // ADD A,[HL]
  void add_a_hl();

  // ADD A,n8
  void add_a_d8();

  // ADD HL,r16
  void add_hl_r16(const Register& reg);

  // ADD HL,SP
  void add_hl_sp();

  // ADD SP,s8
  void add_sp_s8();

  void and_a(const u8& val);

  // AND A,r8
  void and_a_r8(const Register& reg);

  // AND A,[HL]
  void and_a_hl();

  // AND A,n8
  void and_a_d8();

  void bit(const u8& bit_num, const u8& val);

  // BIT u8,r8
  void bit_r8(u8 bit_num, const Register& reg);

  // BIT u3,[HL]
  void bit_hl(u8 bit_num);

  // CALL,nn
  void call();

  // CALL Z,n16
  void call_z();

  // CALL NZ,n16
  void call_nz();

  // CALL C,n16
  void call_c();

  // CALL NC,n16
  void call_nc();

  // CCF
  void ccf();

  void compare_a(const u8& val);

  void cp_a_r8(const Register& reg);

  // CP A,[HL]
  void cp_a_hl();

  // CP A,n8
  void cp_a_d8();

  // CPL
  void cpl();

  // DAA
  // from
  // https://github.com/taisel/GameBoy-Online/blob/master/js/GameBoyCore.js#L588
  void daa();

  void dec(u8& val);

  // DEC r8
  void dec_r8(const Register& reg);

  // DEC [HL]
  void dec_hl();

  // DEC r16
  void dec_r16(const Register& reg);

  // DEC SP
  void dec_sp();

  // DI
  void disable_interrupts();

  // EI
  void enable_interrupts();

  void halt();

  void inc(u8& val);

  // INC r8
  void inc_r8(const Register& reg);

  // INC [HL]
  void inc_hl();

  // INC r16
  void inc_r16(const Register& reg);

  // INC SP
  void inc_sp();

  void jump(const u16& addr);

  // JP n16
  void jp_d16();

  bool can_jump(const u8& opcode, int offset);

  void jump_conditional(const u16& addr, int index_offset = 0);

  // JP cc,n16
  void jp_cc_n16();

  // JP HL
  void jp_hl();

  // JR e8
  void jr_e8();

  // JR cc,e8
  void jr_cc_e8();

  // LD r8,r8
  void ld_r8_r8(const Register& dst, const Register& src);

  // LD r8,n8
  void ld_r8_d8(const Register& dst);

  // LD r16,n16
  void ld_r16_d16(const Register& dst);

  // LD [HL],r8
  void ld_hl_r8(const Register& reg);

  // LD [HL],n8
  void ld_hl_d8();

  // LD r8,[HL]
  void ld_r8_hl(const Register& reg);

  // LD [r16], A
  void ld_r16_a(const Register& reg);

  // LD [n16],A
  void ld_d16_a();

  void load_offset(const u8& offset, const u8& val);

  // LD [$FF00 + n8],A
  void ld_offset_a();

  // LD [$FF00 + C],A
  void ld_offset_c_a();

  // LD A,[r16]
  void ld_a_r16(const Register& reg);

  // LD A,[n16]
  void ld_a_d16();

  void read_offset_from_memory(const u8& offset, u8& dest);

  // LD A,[$FF00 + n8]
  void ld_read_offset_d8();

  // LD A,[$FF00 + C]
  void ld_read_offset_c();

  // TODO: fix this
  void load_hl_a();

  void load_a_hl();

  // LD [HL+],A
  void ld_hl_inc_a();

  // LD [HL-],A
  void ld_hl_dec_a();

  // LD A,[HL+]
  void ld_a_hl_inc();

  // LD A,[HL-]
  void ld_a_hl_dec();

  // LD SP,n16
  void ld_sp_d16();

  // LD [n16],SP
  void ld_d16_sp();

  // LD HL,SP+e8
  void ld_hl_sp_s8();

  // LD SP,HL
  void ld_sp_hl();

  void or_a(const u8& val);

  // OR A,r8
  void or_a_r8(const Register& reg);

  // OR A,[HL]
  void or_a_hl();

  // OR A,n8
  void or_a_d8();

  void pop(u16& reg);

  // POP AF
  void pop_af();

  // POP r16
  void pop_r16(const Register& reg);

  void push(const u16& val);

  // PUSH AF
  void push_af();

  // PUSH r16
  void push_r16(const Register& reg);

  void set_bit(u8& dest, const u8& bit, bool set);

  // RES u3,r8
  void res_u3_r8(const u8 bit, const Register& reg);

  // RES u3,[HL]
  void res_u3_hl(const u8 bit);

  // RET
  void ret();

  // RET,cc
  void ret_conditional();

  // RETI
  void reti();

  void rotate(u8& val, bool left = true);

  void rotate_zero(u8& val, bool left = true);

  // RL r8
  void rl_r8(const Register& reg);

  // RL, [HL]
  void rl_hl();
  // RLA
  void rl_a();

  void rotate_carry(u8& val, bool left = true);

  void rotate_carry_zero(u8& val, bool left = true);

  // RLC r8
  void rlc_r8(const Register& reg);

  // RLC [HL]
  void rlc_hl();

  // RLCA
  void rlca();

  // RR r8
  void rr_r8(const Register& reg);

  // RR [HL]
  void rr_hl();

  // RRA
  void rra();

  // RRC r8
  void rrc_r8(const Register& reg);

  // RRC [HL]
  void rrc_hl();

  // RRCA
  void rrca();

  // RST vec
  void rst();

  void carried_subtract(u8& dst, const u8& src);

  // SBC A,r8
  void sbc_a_r8(const Register& reg);

  // SBC A,[HL]
  void sbc_a_hl();

  // SBC A,n8
  void sbc_a_d8();

  // SCF
  void scf();

  // SET u3,r8
  void set_u3_r8(const u8& bit, const Register& reg);

  // SET u3,[HL]
  void set_u3_hl(const u8& bit);

  void shift_arithmetic(u8& val, bool left = true);

  // SLA r8
  void sla_r8(const Register& reg);

  // SLA [HL]
  void sla_hl();

  // SRA r8
  void sra_r8(const Register& reg);

  // SRA [HL]
  void sra_hl();

  void shift(u8& val);

  // SRL r8
  void srl_r8(const Register& reg);

  // SRL [HL]
  void srl_hl();

  // STOP
  void stop();

  void subtract(u8& dst, const u8& src);

  // SUB A,r8
  void sub_a_r8(const Register& reg);

  // SUB A,[HL]
  void sub_a_hl();

  // SUB A,n8
  void sub_a_d8();

  void swap(u8& val);

  // SWAP r8
  void swap_r8(const Register reg);

  // SWAP [HL]
  void swap_hl();

  // clang thinks xor is block related?
  void exclusive_or(u8& dst, const u8& src);

  // XOR A,r8
  void xor_a_r8(const Register& reg);

  // XOR A,[HL]
  void xor_a_hl();

  // XOR A,n8
  void xor_a_d8();

  // void set_16(u8 &reg_high, u8 &reg_low, u16 val) {
  //  reg_high = (val & 0xFF00) >> 8;
  //  reg_low = (val & 0xFF);
  //}

  void load_reg_to_addr(const Register& dst, const Register& src);

  u16& get_r16(const Register& reg);

  u8 value_at_r16(const Register& reg);
  template <typename F, typename... Args>
  void mutate(const Register reg, F f, Args... args) {
    u8 val = value_at_r16(reg);
    (this->*f)(val, args...);
    memory->set(get_r16(reg), val);
  }
};
}  // namespace gb
