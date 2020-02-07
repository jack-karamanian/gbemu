#include "cpu.h"
#include <array>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include "instruction_table.h"

namespace gb {
Cpu::Cpu(Memory& memory)
    : m_memory{&memory},
      regs{0x0,  0x00, 0x56, 0xff, 0xd,
           0x00, 0x80, 0x11},  // TODO: allow choice of CGB or DMG registers
                               //: regs{0x13, 0x00, 0xd8, 0x00, 0x4d,
      //       0x01, 0xb0, 0x11},  // TODO: allow choice of CGB or DMG registers
      sp{0xfffe},
      pc{0x100} {}

Ticks Cpu::fetch_and_decode() {
  ticks = 0;
  if (stopped || halted) {
    return adjusted_ticks(4);
  }

  if (queue_interrupts_enabled) {
    interrupts_enabled = true;
    queue_interrupts_enabled = false;
  }

  current_opcode = m_memory->at(pc);

  if (debug) {
    std::cout << instruction_names[current_opcode] << '\n';
  }
  if (current_opcode == 0xcb) {
    current_opcode = m_memory->at(pc + 1);
    execute_cb_opcode(current_opcode);
    if (debug) {
      std::cout << cb_instruction_names[current_opcode] << '\n';
    }
  } else {
    execute_opcode(current_opcode);
  }

  return adjusted_ticks(ticks);
}

Ticks Cpu::handle_interrupts() {
  if (interrupts_enabled) {
    for (int i = 0; i < 5; i++) {
      u8 interrupt = 0x01 << i;
      if (has_interrupt(interrupt) && interrupt_enabled(interrupt)) {
        handle_interrupt(interrupt);
        return adjusted_ticks(20);
      }
    }
  }
  return adjusted_ticks(0);
}

void Cpu::handle_interrupt(u8 interrupt) {
  halted = false;
  stopped = false;
  disable_interrupts();
  clear_interrupt(interrupt);
  push(pc);
  switch (interrupt) {
    case Interrupt::VBlank:
      pc = 0x40;
      break;
    case Interrupt::LcdStat:
      pc = 0x48;
      break;
    case Interrupt::Timer:
      pc = 0x50;
      break;
    case Interrupt::Serial:
      pc = 0x58;
      break;
    case Interrupt::Joypad:
      pc = 0x60;
      break;
  }
}

void Cpu::debug_write() {
  std::cout << "A: " << std::hex << std::showbase << std::setw(4)
            << +regs[Register::A] << '\n'
            << "F: " << std::setw(4) << +regs[Register::F] << '\n'
            << "B: " << std::setw(4) << +regs[Register::B]
            << " C: " << std::setw(4) << +regs[Register::C]
            << " BC: " << std::setw(6) << get_r16(Register::BC) << '\n'
            << "D: " << std::setw(4) << +regs[Register::D]
            << " E: " << std::setw(4) << +regs[Register::E]
            << " DE: " << std::setw(6) << get_r16(Register::DE) << '\n'
            << "H: " << std::setw(4) << +regs[Register::H]
            << " L: " << std::setw(4) << +regs[Register::L]
            << " HL: " << std::setw(6) << get_r16(Register::HL) << '\n'
            << "PC: " << std::setw(6) << pc << '\n'
            << "SP: " << std::setw(6) << sp << '\n'
            << std::endl
            << std::endl;
}

void Cpu::set_half_carry_subtract(const u8 a, const u8 b) {
  bool half_carry = (b & 0x0f) > (a & 0x0f);

  if (half_carry) {
    set_flag(FLAG_HALF_CARRY);
  } else {
    clear_flag(FLAG_HALF_CARRY);
  }
}

u8 Cpu::get_interrupts_register() const {
  return m_memory->at(MemoryRegister::InterruptRequest);
}

bool Cpu::interrupt_enabled(u8 interrupt) const {
  const u8 interrupts = m_memory->get_interrupts_enabled();
  return (interrupts & interrupt) != 0;
}

bool Cpu::has_interrupt(u8 interrupt) const {
  return (m_memory->get_interrupts_request() & interrupt) != 0;
}

void Cpu::request_interrupt(Interrupt interrupt) {
  const u8 interrupts = m_memory->get_ram(MemoryRegister::InterruptRequest);
  m_memory->set_ram(MemoryRegister::InterruptRequest, interrupts | interrupt);
}

void Cpu::clear_interrupt(const u8 interrupt) const {
  const u8 interrupts = m_memory->get_ram(0xff0f);
  m_memory->set_ram(0xff0f, interrupts & ~interrupt);
}

void Cpu::invalid() const {
  std::ostringstream s;
  s << "invalid instruction: " << std::hex << +m_memory->at(pc - 1)
    << std::endl;
  throw std::runtime_error(s.str());
}

void Cpu::carried_add(u8& dest, const u8 a, const u8 b) {
  const u8 carry = get_flag(FLAG_CARRY) ? 1 : 0;
  const u8 res = (a + b) + carry;

  if ((a + b) + carry > 0xff) {
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }
  if ((((a & 0x0f) + (b & 0x0f) + carry) & 0x10) > 0xf) {
    set_flag(FLAG_HALF_CARRY);
  } else {
    clear_flag(FLAG_HALF_CARRY);
  }

  dest = res;

  set_zero(res);
  clear_flag(FLAG_SUBTRACT);
}

void Cpu::add(u8& dest, const u8 a, const u8 b) {
  const u8 res = a + b;

  set_carry(a, b);
  set_half_carry(a, b);

  dest = res;

  set_zero(res);
  clear_flag(FLAG_SUBTRACT);
}

// ADC A,[HL]
void Cpu::add_carry_a_hl() {
  u8& a = regs[Register::A];
  const u16 hl = get_r16(Register::HL);
  const u8 val = m_memory->at(hl);

  carried_add(a, a, val);
}

// ADC A,n8
void Cpu::add_carry_a_d8() {
  u8& a = regs[Register::A];
  u8 val = read_operand();

  carried_add(a, a, val);
}

// ADC A,r8
void Cpu::add_carry_a_r8(Register reg) {
  u8& a = regs[Register::A];
  const u8 val = regs[reg];

  carried_add(a, a, val);
}

// ADD A,r8
void Cpu::add_a_r8(Register reg) {
  u8& a = regs[Register::A];
  u8& val = regs[reg];

  add(a, a, val);
}

// ADD A,[HL]
void Cpu::add_a_hl() {
  u8& a = regs[Register::A];
  const u16 addr = get_r16(Register::HL);
  const u8 val = m_memory->at(addr);

  add(a, a, val);
}

// ADD A,n8
void Cpu::add_a_d8() {
  u8& a = regs[Register::A];
  const u8 val = read_operand();

  add(a, a, val);
}

// ADD HL,r16
void Cpu::add_hl_r16(Register reg) {
  const u16 hl = get_r16(Register::HL);
  const u16 r16 = get_r16(reg);

  const u16 res = hl + r16;

  // set_half_carry(hl, r16);

  if (((hl & 0xfff) + (r16 & 0xfff)) > 0xfff) {
    set_flag(FLAG_HALF_CARRY);
  } else {
    clear_flag(FLAG_HALF_CARRY);
  }
  set_carry(hl, r16);
  clear_flag(FLAG_SUBTRACT);

  // hl = res;
  set_r16(Register::HL, res);
}

// ADD HL,SP
void Cpu::add_hl_sp() {
  const u16 hl = get_r16(Register::HL);
  u16 res = hl + sp;

  set_half_carry(hl, sp);
  set_carry(hl, sp);
  clear_flag(FLAG_SUBTRACT);

  set_r16(Register::HL, res);
}

// ADD SP,s8
void Cpu::add_sp_s8() {
  const s8 val = read_operand();
  int res = sp + val;

  if ((res & 0xffffff00) != 0) {
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }

  set_half_carry(sp, static_cast<u16>(val));

  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);

  sp = res;
}

void Cpu::and_a(const u8 val) {
  u8& a = regs[Register::A];

  a &= val;

  set_zero(a);
  set_flag(FLAG_HALF_CARRY);

  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_CARRY);
}

// AND A,r8
void Cpu::and_a_r8(Register reg) {
  u8& r = regs[reg];
  and_a(r);
}

// AND A,[HL]
void Cpu::and_a_hl() {
  const u16 addr = get_r16(Register::HL);
  const u8 val = m_memory->at(addr);

  and_a(val);
}

// AND A,n8
void Cpu::and_a_d8() {
  u8 val = read_operand();

  and_a(val);
}

void Cpu::bit(const u8 bit_num, const u8 val) {
  bool bit_set = (val & (0x01 << bit_num)) != 0;

  if (bit_set) {
    clear_flag(FLAG_ZERO);
  } else {
    set_flag(FLAG_ZERO);
  }

  set_flag(FLAG_HALF_CARRY);
  clear_flag(FLAG_SUBTRACT);
}

// BIT u8,r8
void Cpu::bit_r8(u8 bit_num, Register reg) {
  u8& val = regs[reg];
  bit(bit_num, val);
}

// BIT u3,[HL]
void Cpu::bit_hl(u8 bit_num) {
  const u16 addr = get_r16(Register::HL);
  const u8 val = m_memory->at(addr);
  bit(bit_num, val);
}

// CALL,nn
void Cpu::call() {
  const u16 addr = read_operand<u16>();
  // u16 next_op = pc + 2;
  // u8 pc_low = (next_op & 0xff00) >> 8;
  // bu8 pc_high = (next_op & 0x00ff);
  // bm_memory->set(--sp, pc_high);
  // m_memory->set(--sp, pc_low);
  push(pc);
  pc = addr;
}

// CALL Z,n16
void Cpu::call_z() {
  if (get_flag(FLAG_ZERO)) {
    call();
  } else {
    ticks = 12;
  }
}

// CALL NZ,n16
void Cpu::call_nz() {
  if (!get_flag(FLAG_ZERO)) {
    call();
  } else {
    ticks = 12;
  }
}

// CALL C,n16
void Cpu::call_c() {
  if (get_flag(FLAG_CARRY)) {
    call();
  } else {
    ticks = 12;
  }
}

// CALL NC,n16
void Cpu::call_nc() {
  if (!get_flag(FLAG_CARRY)) {
    call();
  } else {
    ticks = 12;
  }
}

// CCF
void Cpu::ccf() {
  if (get_flag(FLAG_CARRY)) {
    clear_flag(FLAG_CARRY);
  } else {
    set_flag(FLAG_CARRY);
  }
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

void Cpu::compare_a(const u8 val) {
  const u8 a = regs[Register::A];

  const u8 res = a - val;

  set_zero(res);
  set_half_carry_subtract(a, val);

  if (val > a) {
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }

  set_flag(FLAG_SUBTRACT);
}

// CP A,r8
void Cpu::cp_a_r8(Register reg) {
  compare_a(regs[reg]);
}

// CP A,[HL]
void Cpu::cp_a_hl() {
  compare_a(value_at_r16(Register::HL));
}

// CP A,n8
void Cpu::cp_a_d8() {
  const u8 val = read_operand();
  compare_a(val);
}

// CPL
void Cpu::cpl() {
  regs[Register::A] = ~regs[Register::A];
  set_flag(FLAG_SUBTRACT);
  set_flag(FLAG_HALF_CARRY);
}

// DAA
// from
// https://github.com/taisel/GameBoy-Online/blob/master/js/GameBoyCore.js#L588
void Cpu::daa() {
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

void Cpu::dec(u8& val) {
  const u8 res = val - 1;

  set_zero(res);
  set_half_carry_subtract(val, 1);
  set_flag(FLAG_SUBTRACT);

  val = res;
}

// DEC r8
void Cpu::dec_r8(Register reg) {
  dec(regs[reg]);
}

// DEC [HL]
void Cpu::dec_hl() {
  // u8& val = value_at_r16(Register::HL);
  // dec(val);
  mutate(Register::HL, &Cpu::dec);
}

// DEC r16
void Cpu::dec_r16(Register reg) {
  const u16 val = get_r16(reg);
  set_r16(reg, val - 1);
}

// DEC SP
void Cpu::dec_sp() {
  sp--;
}

// DI
void Cpu::disable_interrupts() {
  interrupts_enabled = false;
}

// EI
void Cpu::enable_interrupts() {
  queue_interrupts_enabled = true;
}

void Cpu::halt() {
  halted = true;
}

void Cpu::inc(u8& val) {
  u8 res = val + 1;

  set_half_carry(val, static_cast<u8>(1));
  set_zero(res);
  clear_flag(FLAG_SUBTRACT);

  val = res;
}

// INC r8
void Cpu::inc_r8(Register reg) {
  inc(regs[reg]);
}

// INC [HL]
void Cpu::inc_hl() {
  // u8& val = value_at_r16(Register::HL);
  // inc(val);
  mutate(Register::HL, &Cpu::inc);
}

// INC r16
void Cpu::inc_r16(Register reg) {
  const u16 val = get_r16(reg);
  set_r16(reg, val + 1);
}

// INC SP
void Cpu::inc_sp() {
  sp++;
}

void Cpu::jump(const u16 addr) {
  pc = addr;
}

// JP n16
void Cpu::jp_d16() {
  const u16 addr = read_operand<u16>();
  jump(addr);
}

bool Cpu::can_jump(const u8 opcode, int offset) {
  int index = ((opcode & 0x38) >> 3) - offset;

  return ((index == JumpCondition::NZ && !get_flag(FLAG_ZERO)) ||
          (index == JumpCondition::Z && get_flag(FLAG_ZERO)) ||
          (index == JumpCondition::NC && !get_flag(FLAG_CARRY)) ||
          (index == JumpCondition::C && get_flag(FLAG_CARRY)));
}

void Cpu::jump_conditional(const u16 addr, int index_offset) {
  if (can_jump(current_opcode, index_offset)) {
    jump(addr);
  } else {
    ticks -= 4;
  }
}

// JP cc,n16
void Cpu::jp_cc_n16() {
  const u16 addr = read_operand<u16>();
  jump_conditional(addr);
}

// JP HL
void Cpu::jp_hl() {
  const u16 hl = get_r16(Register::HL);
  pc = hl;
}

// JR e8
void Cpu::jr_e8() {
  const s8 offset = read_operand();
  jump(pc + offset);
}

// JR cc,e8
void Cpu::jr_cc_e8() {
  const s8 offset = read_operand();
  jump_conditional(pc + offset, 4);
}

// LD r8,r8
void Cpu::ld_r8_r8(Register dst, Register src) {
  regs[dst] = regs[src];
}

// LD r8,n8
void Cpu::ld_r8_d8(Register dst) {
  const u8 val = read_operand();
  regs[dst] = val;
}

// LD r16,n16
void Cpu::ld_r16_d16(Register dst) {
  const u16 val = read_operand<u16>();

  // r16 = val;
  set_r16(dst, val);
}

// LD [HL],r8
void Cpu::ld_hl_r8(Register reg) {
  const u16 hl = get_r16(Register::HL);
  // u8* val = m_memory->at(hl);
  //*val = regs[reg];
  m_memory->set(hl, regs[reg]);
}

// LD [HL],n8
void Cpu::ld_hl_d8() {
  const u16 hl = get_r16(Register::HL);
  // u8* val = m_memory->at(hl);
  //*val = read_operand();
  m_memory->set(hl, read_operand());
}

// LD r8,[HL]
void Cpu::ld_r8_hl(Register reg) {
  const u16 hl = get_r16(Register::HL);
  const u8 val = m_memory->at(hl);
  regs[reg] = val;
}

// LD [r16], A
void Cpu::ld_r16_a(Register reg) {
  // u8& val = value_at_r16(reg);
  // val = regs[Register::A];
  m_memory->set(get_r16(reg), regs[Register::A]);
}

// LD [n16],A
void Cpu::ld_d16_a() {
  const u16 addr = read_operand<u16>();
  // u8* val = m_memory->at(addr);
  //*val = regs[Register::A];
  m_memory->set(addr, regs[Register::A]);
}

void Cpu::load_offset(const u8 offset, const u8 val) {
  m_memory->set(0xff00 + offset, val);
}

// LD [$FF00 + n8],A
void Cpu::ld_offset_a() {
  const u8 offset = read_operand();
  load_offset(offset, regs[Register::A]);
}

// LD [$FF00 + C],A
void Cpu::ld_offset_c_a() {
  load_offset(regs[Register::C], regs[Register::A]);
}

// LD A,[r16]
void Cpu::ld_a_r16(Register reg) {
  const u8 val = value_at_r16(reg);
  regs[Register::A] = val;
}

// LD A,[n16]
void Cpu::ld_a_d16() {
  const u16 addr = read_operand<u16>();
  const u8 val = m_memory->at(addr);
  regs[Register::A] = val;
}

void Cpu::read_offset_from_memory(const u8 offset, u8& dest) {
  const u8 val = m_memory->at(0xff00 + offset);
  dest = val;
}

// LD A,[$FF00 + n8]
void Cpu::ld_read_offset_d8() {
  const u8 offset = read_operand();
  read_offset_from_memory(offset, regs[Register::A]);
}

// LD A,[$FF00 + C]
void Cpu::ld_read_offset_c() {
  read_offset_from_memory(regs[Register::C], regs[Register::A]);
}

// TODO: fix this
void Cpu::load_hl_a() {
  const u16 hl = get_r16(Register::HL);
  m_memory->set(hl, regs[Register::A]);
}

void Cpu::load_a_hl() {
  const u8 val = value_at_r16(Register::HL);
  regs[Register::A] = val;
}

// LD [HL+],A
void Cpu::ld_hl_inc_a() {
  load_hl_a();
  inc_r16(Register::HL);
}

// LD [HL-],A
void Cpu::ld_hl_dec_a() {
  load_hl_a();
  dec_r16(Register::HL);
}

// LD A,[HL+]
void Cpu::ld_a_hl_inc() {
  load_a_hl();
  inc_r16(Register::HL);
}

// LD A,[HL-]
void Cpu::ld_a_hl_dec() {
  load_a_hl();
  dec_r16(Register::HL);
}

// LD SP,n16
void Cpu::ld_sp_d16() {
  const u16 val = read_operand<u16>();
  sp = val;
}

// LD [n16],SP
void Cpu::ld_d16_sp() {
  const u16 addr = read_operand<u16>();

  m_memory->set(addr, sp & 0xff);
  m_memory->set(addr + 1, sp >> 8);
}

// LD HL,SP+e8
void Cpu::ld_hl_sp_s8() {
  const s8 val = read_operand();
  int res = sp + val;
  if (res & 0xffff0000) {
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }

  set_half_carry(sp, static_cast<u16>(val));

  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);

  set_r16(Register::HL, res);
}

// LD SP,HL
void Cpu::ld_sp_hl() {
  const u16 hl = get_r16(Register::HL);
  sp = hl;
}

void Cpu::or_a(const u8 val) {
  regs[Register::A] |= val;
  set_zero(regs[Register::A]);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
  clear_flag(FLAG_CARRY);
}

// OR A,r8
void Cpu::or_a_r8(Register reg) {
  or_a(regs[reg]);
}

// OR A,[HL]
void Cpu::or_a_hl() {
  or_a(value_at_r16(Register::HL));
}

// OR A,n8
void Cpu::or_a_d8() {
  or_a(read_operand());
}

void Cpu::pop(u16& reg) {
  const u8 low = m_memory->at(sp + 1);
  const u8 high = m_memory->at(sp);
  sp += 2;

  reg = ((static_cast<u16>(low)) << 8) | high;
}

// POP AF
void Cpu::pop_af() {
  u16 value = 0;
  pop(value);
  set_r16(Register::F, value);
  regs[Register::F] &= 0xf0;
}

// POP r16
void Cpu::pop_r16(Register reg) {
  u16 value = 0;
  pop(value);
  set_r16(reg, value);
}

void Cpu::push(const u16 val) {
  u8 low = (val & 0xff00) >> 8;
  u8 high = val & 0xff;

  m_memory->set(--sp, low);
  m_memory->set(--sp, high);
}

// PUSH AF
void Cpu::push_af() {
  push(get_r16(Register::F));
}

// PUSH r16
void Cpu::push_r16(Register reg) {
  push(get_r16(reg));
}

void Cpu::set_bit(u8& dest, const u8 bit, bool set) {
  const u8 bit_mask = 0x1 << bit;
  if (set) {
    dest |= bit_mask;
  } else {
    dest &= ~bit_mask;
  }
}

// RES u3,r8
void Cpu::res_u3_r8(const u8 bit, Register reg) {
  set_bit(regs[reg], bit, false);
}

// RES u3,[HL]
void Cpu::res_u3_hl(const u8 bit) {
  // set_bit(value_at_r16(Register::HL), bit, false);
  mutate(Register::HL, &Cpu::set_bit, bit, false);
}

// RET
void Cpu::ret() {
  pop(pc);
}

// RET,cc
void Cpu::ret_conditional() {
  if (can_jump(current_opcode, 0)) {
    ret();
  } else {
    ticks = 8;
  }
}

// RETI
void Cpu::reti() {
  ret();
  enable_interrupts();
}

void Cpu::rotate(u8& val, bool left) {
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

void Cpu::rotate_zero(u8& val, bool left) {
  rotate(val, left);
  set_zero(val);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

// RL r8
void Cpu::rl_r8(Register reg) {
  rotate_zero(regs[reg]);
}

// RL, [HL]
void Cpu::rl_hl() {
  mutate(Register::HL, &Cpu::rotate_zero, true);
}

// RLA
void Cpu::rl_a() {
  rotate(regs[Register::A]);

  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

void Cpu::rotate_carry(u8& val, bool left) {
  bool carry = (val & (left ? 0x80 : 0x01)) != 0;

  if (left) {
    val <<= 1;
  } else {
    val >>= 1;
  }

  if (carry) {
    if (left) {
      val |= 0x01;
    } else {
      val |= 0x80;
    }
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }
}

void Cpu::rotate_carry_zero(u8& val, bool left) {
  rotate_carry(val, left);
  set_zero(val);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

// RLC r8
void Cpu::rlc_r8(Register reg) {
  rotate_carry_zero(regs[reg]);
}

// RLC [HL]
void Cpu::rlc_hl() {
  // u8 val = value_at_r16(Register::HL);
  // rotate_carry_zero(val);
  mutate(Register::HL, &Cpu::rotate_carry_zero, true);
}

// RLCA
void Cpu::rlca() {
  rotate_carry(regs[Register::A]);
  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

// RR r8
void Cpu::rr_r8(Register reg) {
  rotate_zero(regs[reg], false);
}

// RR [HL]
void Cpu::rr_hl() {
  u8 val = value_at_r16(Register::HL);
  rotate_zero(val, false);
  m_memory->set(get_r16(Register::HL), val);
}

// RRA
void Cpu::rra() {
  rotate(regs[Register::A], false);
  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

// RRC r8
void Cpu::rrc_r8(Register reg) {
  rotate_carry_zero(regs[reg], false);
}

// RRC [HL]
void Cpu::rrc_hl() {
  u8 val = value_at_r16(Register::HL);
  rotate_carry_zero(val, false);
  m_memory->set(get_r16(Register::HL), val);
}

// RRCA
void Cpu::rrca() {
  rotate_carry(regs[Register::A], false);
  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

// RST vec
void Cpu::rst() {
  push(pc);

  const u16 addr = current_opcode & 0x38;
  pc = addr;
}

void Cpu::carried_subtract(u8& dst, const u8 src) {
  const u8 carry = get_flag(FLAG_CARRY) ? 1 : 0;
  const u8 res = (dst - src) - carry;

  if ((dst & 0x0f) < static_cast<u16>(src & 0x0f) + carry) {
    set_flag(FLAG_HALF_CARRY);
  } else {
    clear_flag(FLAG_HALF_CARRY);
  }

  set_zero(res);

  if (static_cast<u16>(src) + carry > dst) {
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }
  set_flag(FLAG_SUBTRACT);

  dst = res;
}

// SBC A,r8
void Cpu::sbc_a_r8(Register reg) {
  carried_subtract(regs[Register::A], regs[reg]);
}

// SBC A,[HL]
void Cpu::sbc_a_hl() {
  carried_subtract(regs[Register::A], value_at_r16(Register::HL));
}

// SBC A,n8
void Cpu::sbc_a_d8() {
  carried_subtract(regs[Register::A], read_operand());
}

// SCF
void Cpu::scf() {
  set_flag(FLAG_CARRY);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_HALF_CARRY);
}

// SET u3,r8
void Cpu::set_u3_r8(const u8 bit, Register reg) {
  set_bit(regs[reg], bit, true);
}

// SET u3,[HL]
void Cpu::set_u3_hl(const u8 bit) {
  u8 val = value_at_r16(Register::HL);
  set_bit(val, bit, true);
  m_memory->set(get_r16(Register::HL), val);
}

void Cpu::shift_arithmetic(u8& val, bool left) {
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
void Cpu::sla_r8(Register reg) {
  shift_arithmetic(regs[reg]);
}

// SLA [HL]
void Cpu::sla_hl() {
  u8 val = value_at_r16(Register::HL);
  shift_arithmetic(val);
  m_memory->set(get_r16(Register::HL), val);
}

// SRA r8
void Cpu::sra_r8(Register reg) {
  shift_arithmetic(regs[reg], false);
}

// SRA [HL]
void Cpu::sra_hl() {
  u8 val = value_at_r16(Register::HL);
  shift_arithmetic(val, false);
  m_memory->set(get_r16(Register::HL), val);
}

void Cpu::shift(u8& val) {
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
void Cpu::srl_r8(Register reg) {
  shift(regs[reg]);
}

// SRL [HL]
void Cpu::srl_hl() {
  u8 val = value_at_r16(Register::HL);
  shift(val);
  m_memory->set(get_r16(Register::HL), val);
}

// STOP
void Cpu::stop() {
  const u8 speed_switch = m_memory->get_ram(0xff4d);
  m_memory->set_ram(0xff41, m_memory->get_ram(0xff41) & ~0x80);
  if ((speed_switch & 0x1) != 0) {
    double_speed = !double_speed;
    m_memory->set_ram(0xff4d, ~speed_switch & 0x80);
    ticks = (128 * 1024) - 76;
  } else {
    stopped = true;
  }
}

void Cpu::subtract(u8& dst, const u8 src) {
  u8 res = dst - src;

  set_half_carry_subtract(dst, src);
  if (src > dst) {
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }
  set_zero(res);
  set_flag(FLAG_SUBTRACT);

  dst = res;
}

// SUB A,r8
void Cpu::sub_a_r8(Register reg) {
  subtract(regs[Register::A], regs[reg]);
}

// SUB A,[HL]
void Cpu::sub_a_hl() {
  subtract(regs[Register::A], value_at_r16(Register::HL));
}

// SUB A,n8
void Cpu::sub_a_d8() {
  subtract(regs[A], read_operand());
}

void Cpu::swap(u8& val) {
  u8 high = (val & 0xf0) >> 4;
  u8 low = (val & 0xf);
  val = (low << 4) | high;

  set_zero(val);
  clear_flag(FLAG_CARRY);
  clear_flag(FLAG_HALF_CARRY);
  clear_flag(FLAG_SUBTRACT);
}

// SWAP r8
void Cpu::swap_r8(Register reg) {
  swap(regs[reg]);
}

// SWAP [HL]
void Cpu::swap_hl() {
  // swap(value_at_r16(Register::HL));
  mutate(Register::HL, &Cpu::swap);
}

// clang thinks xor is block related?
void Cpu::exclusive_or(u8& dst, const u8 src) {
  dst ^= src;
  set_zero(dst);
  clear_flag(FLAG_SUBTRACT);
  clear_flag(FLAG_CARRY);
  clear_flag(FLAG_HALF_CARRY);
}

// XOR A,r8
void Cpu::xor_a_r8(Register reg) {
  exclusive_or(regs[Register::A], regs[reg]);
}

// XOR A,[HL]
void Cpu::xor_a_hl() {
  exclusive_or(regs[Register::A], value_at_r16(Register::HL));
}

// XOR A,n8
void Cpu::xor_a_d8() {
  exclusive_or(regs[Register::A], read_operand());
}

void Cpu::load_reg_to_addr(Register dst, Register src) {
  const u16 addr = regs[dst];
  m_memory->set(addr, src);
}

u16 Cpu::get_r16(Register reg) {
  return (regs[reg + 1] << 8) | regs[reg];
}

void Cpu::set_r16(Register reg, u16 value) {
  regs[reg] = value & 0xff;
  regs[reg + 1] = (value >> 8) & 0xff;
}

u8 Cpu::value_at_r16(Register reg) {
  const u16 addr = get_r16(reg);
  return m_memory->at(addr);
}

#define GB_INSTRUCTION(op, size, cycles, function)                       \
  case op: {                                                             \
    constexpr int operands_size = size - 1;                              \
    if constexpr (operands_size == 1) {                                  \
      current_operand = m_memory->at(pc + 1);                            \
      if (debug) {                                                       \
        std::cout << std::hex << +std::get<u8>(current_operand) << '\n'; \
      }                                                                  \
    } else if constexpr (operands_size == 2) {                           \
      current_operand = m_memory->at<u16>(pc + 1);                       \
      if (debug) {                                                       \
        std::cout << std::hex << std::get<u16>(current_operand) << '\n'; \
      }                                                                  \
    } else if constexpr (operands_size > 2) {                            \
      static_assert(operands_size <= 2,                                  \
                    "number of operands must be 0, 1, or 2");            \
    }                                                                    \
    pc += size;                                                          \
    ticks += cycles;                                                     \
    function;                                                            \
    break;                                                               \
  }

void Cpu::execute_opcode(u8 opcode) {
  switch (opcode) {
    // NOP
    GB_INSTRUCTION(0x0, 1, 4, noop())

    // LD BC,d16
    GB_INSTRUCTION(0x1, 3, 12, ld_r16_d16(Cpu::BC))

    // LD (BC),A
    GB_INSTRUCTION(0x2, 1, 8, ld_r16_a(Cpu::BC))

    // INC BC
    GB_INSTRUCTION(0x3, 1, 8, inc_r16(Cpu::BC))

    // INC B
    GB_INSTRUCTION(0x4, 1, 4, inc_r8(Cpu::B))

    // DEC B
    GB_INSTRUCTION(0x5, 1, 4, dec_r8(Cpu::B))

    // LD B,d8
    GB_INSTRUCTION(0x6, 2, 8, ld_r8_d8(Cpu::B))

    // RLCA
    GB_INSTRUCTION(0x7, 1, 4, rlca())

    // LD (a16),SP
    GB_INSTRUCTION(0x8, 3, 20, ld_d16_sp())

    // ADD HL,BC
    GB_INSTRUCTION(0x9, 1, 8, add_hl_r16(Cpu::BC))

    // LD A,(BC)
    GB_INSTRUCTION(0xa, 1, 8, ld_a_r16(Cpu::BC))

    // DEC BC
    GB_INSTRUCTION(0xb, 1, 8, dec_r16(Cpu::BC))

    // INC C
    GB_INSTRUCTION(0xc, 1, 4, inc_r8(Cpu::C))

    // DEC C
    GB_INSTRUCTION(0xd, 1, 4, dec_r8(Cpu::C))

    // LD C,d8
    GB_INSTRUCTION(0xe, 2, 8, ld_r8_d8(Cpu::C))

    // RRCA
    GB_INSTRUCTION(0xf, 1, 4, rrca())

    // STOP 0
    GB_INSTRUCTION(0x10, 2, 4, stop())

    // LD DE,d16
    GB_INSTRUCTION(0x11, 3, 12, ld_r16_d16(Cpu::DE))

    // LD (DE),A
    GB_INSTRUCTION(0x12, 1, 8, ld_r16_a(Cpu::DE))

    // INC DE
    GB_INSTRUCTION(0x13, 1, 8, inc_r16(Cpu::DE))

    // INC D
    GB_INSTRUCTION(0x14, 1, 4, inc_r8(Cpu::D))

    // DEC D
    GB_INSTRUCTION(0x15, 1, 4, dec_r8(Cpu::D))

    // LD D,d8
    GB_INSTRUCTION(0x16, 2, 8, ld_r8_d8(Cpu::D))

    // RLA
    GB_INSTRUCTION(0x17, 1, 4, rl_a())

    // JR r8
    GB_INSTRUCTION(0x18, 2, 12, jr_e8())

    // ADD HL,DE
    GB_INSTRUCTION(0x19, 1, 8, add_hl_r16(DE))

    // LD A,(DE)
    GB_INSTRUCTION(0x1a, 1, 8, ld_a_r16(DE))

    // DEC DE
    GB_INSTRUCTION(0x1b, 1, 8, dec_r16(DE))

    // INC E
    GB_INSTRUCTION(0x1c, 1, 4, inc_r8(E))

    // DEC E
    GB_INSTRUCTION(0x1d, 1, 4, dec_r8(E))

    // LD E,d8
    GB_INSTRUCTION(0x1e, 2, 8, ld_r8_d8(E))

    // RRA
    GB_INSTRUCTION(0x1f, 1, 4, rra())

    // JR NZ,r8
    GB_INSTRUCTION(0x20, 2, 12, jr_cc_e8())

    // LD HL,d16
    GB_INSTRUCTION(0x21, 3, 12, ld_r16_d16(HL))

    // LD (HL+),A
    GB_INSTRUCTION(0x22, 1, 8, ld_hl_inc_a())

    // INC HL
    GB_INSTRUCTION(0x23, 1, 8, inc_r16(HL))

    // INC H
    GB_INSTRUCTION(0x24, 1, 4, inc_r8(H))

    // DEC H
    GB_INSTRUCTION(0x25, 1, 4, dec_r8(H))

    // LD H,d8
    GB_INSTRUCTION(0x26, 2, 8, ld_r8_d8(H))

    // DAA
    GB_INSTRUCTION(0x27, 1, 4, daa())

    // JR Z,r8
    GB_INSTRUCTION(0x28, 2, 12, jr_cc_e8())

    // ADD HL,HL
    GB_INSTRUCTION(0x29, 1, 8, add_hl_r16(HL))

    // LD A,(HL+)
    GB_INSTRUCTION(0x2a, 1, 8, ld_a_hl_inc())

    // DEC HL
    GB_INSTRUCTION(0x2b, 1, 8, dec_r16(HL))

    // INC L
    GB_INSTRUCTION(0x2c, 1, 4, inc_r8(L))

    // DEC L
    GB_INSTRUCTION(0x2d, 1, 4, dec_r8(L))

    // LD L,d8
    GB_INSTRUCTION(0x2e, 2, 8, ld_r8_d8(L))

    // CPL
    GB_INSTRUCTION(0x2f, 1, 4, cpl())

    // JR NC,r8
    GB_INSTRUCTION(0x30, 2, 12, jr_cc_e8())

    // LD SP,d16
    GB_INSTRUCTION(0x31, 3, 12, ld_sp_d16())

    // LD (HL-),A
    GB_INSTRUCTION(0x32, 1, 8, ld_hl_dec_a())

    // INC SP
    GB_INSTRUCTION(0x33, 1, 8, inc_sp())

    // INC (HL)
    GB_INSTRUCTION(0x34, 1, 12, inc_hl())

    // DEC (HL)
    GB_INSTRUCTION(0x35, 1, 12, dec_hl())

    // LD (HL),d8
    GB_INSTRUCTION(0x36, 2, 12, ld_hl_d8())

    // SCF
    GB_INSTRUCTION(0x37, 1, 4, scf())

    // JR C,r8
    GB_INSTRUCTION(0x38, 2, 12, jr_cc_e8())

    // ADD HL,SP
    GB_INSTRUCTION(0x39, 1, 8, add_hl_sp())

    // LD A,(HL-)
    GB_INSTRUCTION(0x3a, 1, 8, ld_a_hl_dec())

    // DEC SP
    GB_INSTRUCTION(0x3b, 1, 8, dec_sp())

    // INC A
    GB_INSTRUCTION(0x3c, 1, 4, inc_r8(A))

    // DEC A
    GB_INSTRUCTION(0x3d, 1, 4, dec_r8(A))

    // LD A,d8
    GB_INSTRUCTION(0x3e, 2, 8, ld_r8_d8(A))

    // CCF
    GB_INSTRUCTION(0x3f, 1, 4, ccf())

    // LD B,B
    GB_INSTRUCTION(0x40, 1, 4, ld_r8_r8(B, B))

    // LD B,C
    GB_INSTRUCTION(0x41, 1, 4, ld_r8_r8(B, C))

    // LD B,D
    GB_INSTRUCTION(0x42, 1, 4, ld_r8_r8(B, D))

    // LD B,E
    GB_INSTRUCTION(0x43, 1, 4, ld_r8_r8(B, E))

    // LD B,H
    GB_INSTRUCTION(0x44, 1, 4, ld_r8_r8(B, H))

    // LD B,L
    GB_INSTRUCTION(0x45, 1, 4, ld_r8_r8(B, L))

    // LD B,(HL)
    GB_INSTRUCTION(0x46, 1, 8, ld_r8_hl(B))

    // LD B,A
    GB_INSTRUCTION(0x47, 1, 4, ld_r8_r8(B, A))

    // LD C,B
    GB_INSTRUCTION(0x48, 1, 4, ld_r8_r8(C, B))

    // LD C,C
    GB_INSTRUCTION(0x49, 1, 4, ld_r8_r8(C, C))

    // LD C,D
    GB_INSTRUCTION(0x4a, 1, 4, ld_r8_r8(C, D))

    // LD C,E
    GB_INSTRUCTION(0x4b, 1, 4, ld_r8_r8(C, E))

    // LD C,H
    GB_INSTRUCTION(0x4c, 1, 4, ld_r8_r8(C, H))

    // LD C,L
    GB_INSTRUCTION(0x4d, 1, 4, ld_r8_r8(C, L))

    // LD C,(HL)
    GB_INSTRUCTION(0x4e, 1, 8, ld_r8_hl(C))

    // LD C,A
    GB_INSTRUCTION(0x4f, 1, 4, ld_r8_r8(C, A))

    // LD D,B
    GB_INSTRUCTION(0x50, 1, 4, ld_r8_r8(D, B))

    // LD D,C
    GB_INSTRUCTION(0x51, 1, 4, ld_r8_r8(D, C))

    // LD D,D
    GB_INSTRUCTION(0x52, 1, 4, ld_r8_r8(D, D))

    // LD D,E
    GB_INSTRUCTION(0x53, 1, 4, ld_r8_r8(D, E))

    // LD D,H
    GB_INSTRUCTION(0x54, 1, 4, ld_r8_r8(D, H))

    // LD D,L
    GB_INSTRUCTION(0x55, 1, 4, ld_r8_r8(D, L))

    // LD D,(HL)
    GB_INSTRUCTION(0x56, 1, 8, ld_r8_hl(D))

    // LD D,A
    GB_INSTRUCTION(0x57, 1, 4, ld_r8_r8(D, A))

    // LD E,B
    GB_INSTRUCTION(0x58, 1, 4, ld_r8_r8(E, B))

    // LD E,C
    GB_INSTRUCTION(0x59, 1, 4, ld_r8_r8(E, C))

    // LD E,D
    GB_INSTRUCTION(0x5a, 1, 4, ld_r8_r8(E, D))

    // LD E,E
    GB_INSTRUCTION(0x5b, 1, 4, ld_r8_r8(E, E))

    // LD E,H
    GB_INSTRUCTION(0x5c, 1, 4, ld_r8_r8(E, H))

    // LD E,L
    GB_INSTRUCTION(0x5d, 1, 4, ld_r8_r8(E, L))

    // LD E,(HL)
    GB_INSTRUCTION(0x5e, 1, 8, ld_r8_hl(E))

    // LD E,A
    GB_INSTRUCTION(0x5f, 1, 4, ld_r8_r8(E, A))

    // LD H,B
    GB_INSTRUCTION(0x60, 1, 4, ld_r8_r8(H, B))

    // LD H,C
    GB_INSTRUCTION(0x61, 1, 4, ld_r8_r8(H, C))

    // LD H,D
    GB_INSTRUCTION(0x62, 1, 4, ld_r8_r8(H, D))

    // LD H,E
    GB_INSTRUCTION(0x63, 1, 4, ld_r8_r8(H, E))

    // LD H,H
    GB_INSTRUCTION(0x64, 1, 4, ld_r8_r8(H, H))

    // LD H,L
    GB_INSTRUCTION(0x65, 1, 4, ld_r8_r8(H, L))

    // LD H,(HL)
    GB_INSTRUCTION(0x66, 1, 8, ld_r8_hl(H))

    // LD H,A
    GB_INSTRUCTION(0x67, 1, 4, ld_r8_r8(H, A))

    // LD L,B
    GB_INSTRUCTION(0x68, 1, 4, ld_r8_r8(L, B))

    // LD L,C
    GB_INSTRUCTION(0x69, 1, 4, ld_r8_r8(L, C))

    // LD L,D
    GB_INSTRUCTION(0x6a, 1, 4, ld_r8_r8(L, D))

    // LD L,E
    GB_INSTRUCTION(0x6b, 1, 4, ld_r8_r8(L, E))

    // LD L,H
    GB_INSTRUCTION(0x6c, 1, 4, ld_r8_r8(L, H))

    // LD L,L
    GB_INSTRUCTION(0x6d, 1, 4, ld_r8_r8(L, L))

    // LD L,(HL)
    GB_INSTRUCTION(0x6e, 1, 8, ld_r8_hl(L))

    // LD L,A
    GB_INSTRUCTION(0x6f, 1, 4, ld_r8_r8(L, A))

    // LD (HL),B
    GB_INSTRUCTION(0x70, 1, 8, ld_hl_r8(B))

    // LD (HL),C
    GB_INSTRUCTION(0x71, 1, 8, ld_hl_r8(C))

    // LD (HL),D
    GB_INSTRUCTION(0x72, 1, 8, ld_hl_r8(D))

    // LD (HL),E
    GB_INSTRUCTION(0x73, 1, 8, ld_hl_r8(E))

    // LD (HL),H
    GB_INSTRUCTION(0x74, 1, 8, ld_hl_r8(H))

    // LD (HL),L
    GB_INSTRUCTION(0x75, 1, 8, ld_hl_r8(L))

    // HALT
    GB_INSTRUCTION(0x76, 1, 4, halt())

    // LD (HL),A
    GB_INSTRUCTION(0x77, 1, 8, ld_hl_r8(A))

    // LD A,B
    GB_INSTRUCTION(0x78, 1, 4, ld_r8_r8(A, B))

    // LD A,C
    GB_INSTRUCTION(0x79, 1, 4, ld_r8_r8(A, C))

    // LD A,D
    GB_INSTRUCTION(0x7a, 1, 4, ld_r8_r8(A, D))

    // LD A,E
    GB_INSTRUCTION(0x7b, 1, 4, ld_r8_r8(A, E))

    // LD A,H
    GB_INSTRUCTION(0x7c, 1, 4, ld_r8_r8(A, H))

    // LD A,L
    GB_INSTRUCTION(0x7d, 1, 4, ld_r8_r8(A, L))

    // LD A,(HL)
    GB_INSTRUCTION(0x7e, 1, 8, load_a_hl())

    // LD A,A
    GB_INSTRUCTION(0x7f, 1, 4, ld_r8_r8(A, A))

    // ADD A,B
    GB_INSTRUCTION(0x80, 1, 4, add_a_r8(B))

    // ADD A,C
    GB_INSTRUCTION(0x81, 1, 4, add_a_r8(C))

    // ADD A,D
    GB_INSTRUCTION(0x82, 1, 4, add_a_r8(D))

    // ADD A,E
    GB_INSTRUCTION(0x83, 1, 4, add_a_r8(E))

    // ADD A,H
    GB_INSTRUCTION(0x84, 1, 4, add_a_r8(H))

    // ADD A,L
    GB_INSTRUCTION(0x85, 1, 4, add_a_r8(L))

    // ADD A,(HL)
    GB_INSTRUCTION(0x86, 1, 8, add_a_hl())

    // ADD A,A
    GB_INSTRUCTION(0x87, 1, 4, add_a_r8(A))

    // ADC A,B
    GB_INSTRUCTION(0x88, 1, 4, add_carry_a_r8(B))

    // ADC A,C
    GB_INSTRUCTION(0x89, 1, 4, add_carry_a_r8(C))

    // ADC A,D
    GB_INSTRUCTION(0x8a, 1, 4, add_carry_a_r8(D))

    // ADC A,E
    GB_INSTRUCTION(0x8b, 1, 4, add_carry_a_r8(E))

    // ADC A,H
    GB_INSTRUCTION(0x8c, 1, 4, add_carry_a_r8(H))

    // ADC A,L
    GB_INSTRUCTION(0x8d, 1, 4, add_carry_a_r8(L))

    // ADC A,(HL)
    GB_INSTRUCTION(0x8e, 1, 8, add_carry_a_hl())

    // ADC A,A
    GB_INSTRUCTION(0x8f, 1, 4, add_carry_a_r8(A))

    // SUB B
    GB_INSTRUCTION(0x90, 1, 4, sub_a_r8(B))

    // SUB C
    GB_INSTRUCTION(0x91, 1, 4, sub_a_r8(C))

    // SUB D
    GB_INSTRUCTION(0x92, 1, 4, sub_a_r8(D))

    // SUB E
    GB_INSTRUCTION(0x93, 1, 4, sub_a_r8(E))

    // SUB H
    GB_INSTRUCTION(0x94, 1, 4, sub_a_r8(H))

    // SUB L
    GB_INSTRUCTION(0x95, 1, 4, sub_a_r8(L))

    // SUB (HL)
    GB_INSTRUCTION(0x96, 1, 8, sub_a_hl())

    // SUB A
    GB_INSTRUCTION(0x97, 1, 4, sub_a_r8(A))

    // SBC A,B
    GB_INSTRUCTION(0x98, 1, 4, sbc_a_r8(B))

    // SBC A,C
    GB_INSTRUCTION(0x99, 1, 4, sbc_a_r8(C))

    // SBC A,D
    GB_INSTRUCTION(0x9a, 1, 4, sbc_a_r8(D))

    // SBC A,E
    GB_INSTRUCTION(0x9b, 1, 4, sbc_a_r8(E))

    // SBC A,H
    GB_INSTRUCTION(0x9c, 1, 4, sbc_a_r8(H))

    // SBC A,L
    GB_INSTRUCTION(0x9d, 1, 4, sbc_a_r8(L))

    // SBC A,(HL)
    GB_INSTRUCTION(0x9e, 1, 8, sbc_a_hl())

    // SBC A,A
    GB_INSTRUCTION(0x9f, 1, 4, sbc_a_r8(A))

    // AND B
    GB_INSTRUCTION(0xa0, 1, 4, and_a_r8(B))

    // AND C
    GB_INSTRUCTION(0xa1, 1, 4, and_a_r8(C))

    // AND D
    GB_INSTRUCTION(0xa2, 1, 4, and_a_r8(D))

    // AND E
    GB_INSTRUCTION(0xa3, 1, 4, and_a_r8(E))

    // AND H
    GB_INSTRUCTION(0xa4, 1, 4, and_a_r8(H))

    // AND L
    GB_INSTRUCTION(0xa5, 1, 4, and_a_r8(L))

    // AND (HL)
    GB_INSTRUCTION(0xa6, 1, 8, and_a_hl())

    // AND A
    GB_INSTRUCTION(0xa7, 1, 4, and_a_r8(A))

    // XOR B
    GB_INSTRUCTION(0xa8, 1, 4, xor_a_r8(B))

    // XOR C
    GB_INSTRUCTION(0xa9, 1, 4, xor_a_r8(C))

    // XOR D
    GB_INSTRUCTION(0xaa, 1, 4, xor_a_r8(D))

    // XOR E
    GB_INSTRUCTION(0xab, 1, 4, xor_a_r8(E))

    // XOR H
    GB_INSTRUCTION(0xac, 1, 4, xor_a_r8(H))

    // XOR L
    GB_INSTRUCTION(0xad, 1, 4, xor_a_r8(L))

    // XOR (HL)
    GB_INSTRUCTION(0xae, 1, 8, xor_a_hl())

    // XOR A
    GB_INSTRUCTION(0xaf, 1, 4, xor_a_r8(A))

    // OR B
    GB_INSTRUCTION(0xb0, 1, 4, or_a_r8(B))

    // OR C
    GB_INSTRUCTION(0xb1, 1, 4, or_a_r8(C))

    // OR D
    GB_INSTRUCTION(0xb2, 1, 4, or_a_r8(D))

    // OR E
    GB_INSTRUCTION(0xb3, 1, 4, or_a_r8(E))

    // OR H
    GB_INSTRUCTION(0xb4, 1, 4, or_a_r8(H))

    // OR L
    GB_INSTRUCTION(0xb5, 1, 4, or_a_r8(L))

    // OR (HL)
    GB_INSTRUCTION(0xb6, 1, 8, or_a_hl())

    // OR A
    GB_INSTRUCTION(0xb7, 1, 4, or_a_r8(A))

    // CP B
    GB_INSTRUCTION(0xb8, 1, 4, cp_a_r8(B))

    // CP C
    GB_INSTRUCTION(0xb9, 1, 4, cp_a_r8(C))

    // CP D
    GB_INSTRUCTION(0xba, 1, 4, cp_a_r8(D))

    // CP E
    GB_INSTRUCTION(0xbb, 1, 4, cp_a_r8(E))

    // CP H
    GB_INSTRUCTION(0xbc, 1, 4, cp_a_r8(H))

    // CP L
    GB_INSTRUCTION(0xbd, 1, 4, cp_a_r8(L))

    // CP (HL)
    GB_INSTRUCTION(0xbe, 1, 8, cp_a_hl())

    // CP A
    GB_INSTRUCTION(0xbf, 1, 4, cp_a_r8(A))

    // RET NZ
    GB_INSTRUCTION(0xc0, 1, 20, ret_conditional())

    // POP BC
    GB_INSTRUCTION(0xc1, 1, 12, pop_r16(BC))

    // JP NZ,a16
    GB_INSTRUCTION(0xc2, 3, 16, jp_cc_n16())

    // JP a16
    GB_INSTRUCTION(0xc3, 3, 16, jp_d16())

    // CALL NZ,a16
    GB_INSTRUCTION(0xc4, 3, 24, call_nz())

    // PUSH BC
    GB_INSTRUCTION(0xc5, 1, 16, push_r16(BC))

    // ADD A,d8
    GB_INSTRUCTION(0xc6, 2, 8, add_a_d8())

    // RST 00H
    GB_INSTRUCTION(0xc7, 1, 16, rst())

    // RET Z
    GB_INSTRUCTION(0xc8, 1, 20, ret_conditional())

    // RET
    GB_INSTRUCTION(0xc9, 1, 16, ret())

    // JP Z,a16
    GB_INSTRUCTION(0xca, 3, 16, jp_cc_n16())

    // PREFIX CB
    GB_INSTRUCTION(0xcb, 1, 4, noop())

    // CALL Z,a16
    GB_INSTRUCTION(0xcc, 3, 24, call_z())

    // CALL a16
    GB_INSTRUCTION(0xcd, 3, 24, call())

    // ADC A,d8
    GB_INSTRUCTION(0xce, 2, 8, add_carry_a_d8())

    // RST 08H
    GB_INSTRUCTION(0xcf, 1, 16, rst())

    // RET NC
    GB_INSTRUCTION(0xd0, 1, 20, ret_conditional())

    // POP DE
    GB_INSTRUCTION(0xd1, 1, 12, pop_r16(DE))

    // JP NC,a16
    GB_INSTRUCTION(0xd2, 3, 16, jp_cc_n16())

    // INVALID
    GB_INSTRUCTION(0xd3, 1, 16, invalid())

    // CALL NC,a16
    GB_INSTRUCTION(0xd4, 3, 24, call_nc())

    // PUSH DE
    GB_INSTRUCTION(0xd5, 1, 16, push_r16(DE))

    // SUB d8
    GB_INSTRUCTION(0xd6, 2, 8, sub_a_d8())

    // RST 10H
    GB_INSTRUCTION(0xd7, 1, 16, rst())

    // RET C
    GB_INSTRUCTION(0xd8, 1, 20, ret_conditional())

    // RETI
    GB_INSTRUCTION(0xd9, 1, 16, reti())

    // JP C,a16
    GB_INSTRUCTION(0xda, 3, 16, jp_cc_n16())

    // INVALID
    GB_INSTRUCTION(0xdb, 1, 16, invalid())

    // CALL C,a16
    GB_INSTRUCTION(0xdc, 3, 24, call_c())

    // INVALID
    GB_INSTRUCTION(0xdd, 1, 24, invalid())

    // SBC A,d8
    GB_INSTRUCTION(0xde, 2, 8, sbc_a_d8())

    // RST 18H
    GB_INSTRUCTION(0xdf, 1, 16, rst())

    // LDH (a8),A
    GB_INSTRUCTION(0xe0, 2, 12, ld_offset_a())

    // POP HL
    GB_INSTRUCTION(0xe1, 1, 12, pop_r16(HL))

    // LD (C),A
    GB_INSTRUCTION(0xe2, 1, 8, ld_offset_c_a())

    // INVALID
    GB_INSTRUCTION(0xe3, 1, 8, invalid())

    // INVALID
    GB_INSTRUCTION(0xe4, 1, 8, invalid())

    // PUSH HL
    GB_INSTRUCTION(0xe5, 1, 16, push_r16(HL))

    // AND d8
    GB_INSTRUCTION(0xe6, 2, 8, and_a_d8())

    // RST 20H
    GB_INSTRUCTION(0xe7, 1, 16, rst())

    // ADD SP,r8
    GB_INSTRUCTION(0xe8, 2, 16, add_sp_s8())

    // JP (HL)
    GB_INSTRUCTION(0xe9, 1, 4, jp_hl())

    // LD (a16),A
    GB_INSTRUCTION(0xea, 3, 16, ld_d16_a())

    // INVALID
    GB_INSTRUCTION(0xeb, 1, 16, invalid())

    // INVALID
    GB_INSTRUCTION(0xec, 1, 16, invalid())

    // INVALID
    GB_INSTRUCTION(0xed, 1, 16, invalid())

    // XOR d8
    GB_INSTRUCTION(0xee, 2, 8, xor_a_d8())

    // RST 28H
    GB_INSTRUCTION(0xef, 1, 16, rst())

    // LDH A,(a8)
    GB_INSTRUCTION(0xf0, 2, 12, ld_read_offset_d8())

    // POP AF
    GB_INSTRUCTION(0xf1, 1, 12, pop_af())

    // LD A,(C)
    GB_INSTRUCTION(0xf2, 1, 8, ld_read_offset_c())

    // DI
    GB_INSTRUCTION(0xf3, 1, 4, disable_interrupts())

    // INVALID
    GB_INSTRUCTION(0xf4, 1, 4, invalid())

    // PUSH AF
    GB_INSTRUCTION(0xf5, 1, 16, push_af())

    // OR d8
    GB_INSTRUCTION(0xf6, 2, 8, or_a_d8())

    // RST 30H
    GB_INSTRUCTION(0xf7, 1, 16, rst())

    // LD HL,SP+r8
    GB_INSTRUCTION(0xf8, 2, 12, ld_hl_sp_s8())

    // LD SP,HL
    GB_INSTRUCTION(0xf9, 1, 8, ld_sp_hl())

    // LD A,(a16)
    GB_INSTRUCTION(0xfa, 3, 16, ld_a_d16())

    // EI
    GB_INSTRUCTION(0xfb, 1, 4, enable_interrupts())

    // INVALID
    GB_INSTRUCTION(0xfc, 1, 4, invalid())

    // INVALID
    GB_INSTRUCTION(0xfd, 1, 4, invalid())

    // CP d8
    GB_INSTRUCTION(0xfe, 2, 8, cp_a_d8())

    // RST 38H
    GB_INSTRUCTION(0xff, 1, 16, rst())
  }
}

void Cpu::execute_cb_opcode(u8 opcode) {
  switch (opcode) {
    // RLC B
    GB_INSTRUCTION(0x0, 2, 8, rlc_r8(B))

    // RLC C
    GB_INSTRUCTION(0x1, 2, 8, rlc_r8(C))

    // RLC D
    GB_INSTRUCTION(0x2, 2, 8, rlc_r8(D))

    // RLC E
    GB_INSTRUCTION(0x3, 2, 8, rlc_r8(E))

    // RLC H
    GB_INSTRUCTION(0x4, 2, 8, rlc_r8(H))

    // RLC L
    GB_INSTRUCTION(0x5, 2, 8, rlc_r8(L))

    // RLC (HL)
    GB_INSTRUCTION(0x6, 2, 16, rlc_hl())

    // RLC A
    GB_INSTRUCTION(0x7, 2, 8, rlc_r8(A))

    // RRC B
    GB_INSTRUCTION(0x8, 2, 8, rrc_r8(B))

    // RRC C
    GB_INSTRUCTION(0x9, 2, 8, rrc_r8(C))

    // RRC D
    GB_INSTRUCTION(0xa, 2, 8, rrc_r8(D))

    // RRC E
    GB_INSTRUCTION(0xb, 2, 8, rrc_r8(E))

    // RRC H
    GB_INSTRUCTION(0xc, 2, 8, rrc_r8(H))

    // RRC L
    GB_INSTRUCTION(0xd, 2, 8, rrc_r8(L))

    // RRC (HL)
    GB_INSTRUCTION(0xe, 2, 16, rrc_hl())

    // RRC A
    GB_INSTRUCTION(0xf, 2, 8, rrc_r8(A))

    // RL B
    GB_INSTRUCTION(0x10, 2, 8, rl_r8(B))

    // RL C
    GB_INSTRUCTION(0x11, 2, 8, rl_r8(C))

    // RL D
    GB_INSTRUCTION(0x12, 2, 8, rl_r8(D))

    // RL E
    GB_INSTRUCTION(0x13, 2, 8, rl_r8(E))

    // RL H
    GB_INSTRUCTION(0x14, 2, 8, rl_r8(H))

    // RL L
    GB_INSTRUCTION(0x15, 2, 8, rl_r8(L))

    // RL (HL)
    GB_INSTRUCTION(0x16, 2, 16, rl_hl())

    // RL A
    GB_INSTRUCTION(0x17, 2, 8, rl_r8(A))

    // RR B
    GB_INSTRUCTION(0x18, 2, 8, rr_r8(B))

    // RR C
    GB_INSTRUCTION(0x19, 2, 8, rr_r8(C))

    // RR D
    GB_INSTRUCTION(0x1a, 2, 8, rr_r8(D))

    // RR E
    GB_INSTRUCTION(0x1b, 2, 8, rr_r8(E))

    // RR H
    GB_INSTRUCTION(0x1c, 2, 8, rr_r8(H))

    // RR L
    GB_INSTRUCTION(0x1d, 2, 8, rr_r8(L))

    // RR (HL)
    GB_INSTRUCTION(0x1e, 2, 16, rr_hl())

    // RR A
    GB_INSTRUCTION(0x1f, 2, 8, rr_r8(A))

    // SLA B
    GB_INSTRUCTION(0x20, 2, 8, sla_r8(B))

    // SLA C
    GB_INSTRUCTION(0x21, 2, 8, sla_r8(C))

    // SLA D
    GB_INSTRUCTION(0x22, 2, 8, sla_r8(D))

    // SLA E
    GB_INSTRUCTION(0x23, 2, 8, sla_r8(E))

    // SLA H
    GB_INSTRUCTION(0x24, 2, 8, sla_r8(H))

    // SLA L
    GB_INSTRUCTION(0x25, 2, 8, sla_r8(L))

    // SLA (HL)
    GB_INSTRUCTION(0x26, 2, 16, sla_hl())

    // SLA A
    GB_INSTRUCTION(0x27, 2, 8, sla_r8(A))

    // SRA B
    GB_INSTRUCTION(0x28, 2, 8, sra_r8(B))

    // SRA C
    GB_INSTRUCTION(0x29, 2, 8, sra_r8(C))

    // SRA D
    GB_INSTRUCTION(0x2a, 2, 8, sra_r8(D))

    // SRA E
    GB_INSTRUCTION(0x2b, 2, 8, sra_r8(E))

    // SRA H
    GB_INSTRUCTION(0x2c, 2, 8, sra_r8(H))

    // SRA L
    GB_INSTRUCTION(0x2d, 2, 8, sra_r8(L))

    // SRA (HL)
    GB_INSTRUCTION(0x2e, 2, 16, sra_hl())

    // SRA A
    GB_INSTRUCTION(0x2f, 2, 8, sra_r8(A))

    // SWAP B
    GB_INSTRUCTION(0x30, 2, 8, swap_r8(B))

    // SWAP C
    GB_INSTRUCTION(0x31, 2, 8, swap_r8(C))

    // SWAP D
    GB_INSTRUCTION(0x32, 2, 8, swap_r8(D))

    // SWAP E
    GB_INSTRUCTION(0x33, 2, 8, swap_r8(E))

    // SWAP H
    GB_INSTRUCTION(0x34, 2, 8, swap_r8(H))

    // SWAP L
    GB_INSTRUCTION(0x35, 2, 8, swap_r8(L))

    // SWAP (HL)
    GB_INSTRUCTION(0x36, 2, 16, swap_hl())

    // SWAP A
    GB_INSTRUCTION(0x37, 2, 8, swap_r8(A))

    // SRL B
    GB_INSTRUCTION(0x38, 2, 8, srl_r8(B))

    // SRL C
    GB_INSTRUCTION(0x39, 2, 8, srl_r8(C))

    // SRL D
    GB_INSTRUCTION(0x3a, 2, 8, srl_r8(D))

    // SRL E
    GB_INSTRUCTION(0x3b, 2, 8, srl_r8(E))

    // SRL H
    GB_INSTRUCTION(0x3c, 2, 8, srl_r8(H))

    // SRL L
    GB_INSTRUCTION(0x3d, 2, 8, srl_r8(L))

    // SRL (HL)
    GB_INSTRUCTION(0x3e, 2, 16, srl_hl())

    // SRL A
    GB_INSTRUCTION(0x3f, 2, 8, srl_r8(A))

    // BIT 0,B
    GB_INSTRUCTION(0x40, 2, 8, bit_r8(0, B))

    // BIT 0,C
    GB_INSTRUCTION(0x41, 2, 8, bit_r8(0, C))

    // BIT 0,D
    GB_INSTRUCTION(0x42, 2, 8, bit_r8(0, D))

    // BIT 0,E
    GB_INSTRUCTION(0x43, 2, 8, bit_r8(0, E))

    // BIT 0,H
    GB_INSTRUCTION(0x44, 2, 8, bit_r8(0, H))

    // BIT 0,L
    GB_INSTRUCTION(0x45, 2, 8, bit_r8(0, L))

    // BIT 0,(HL)
    GB_INSTRUCTION(0x46, 2, 12, bit_hl(0))

    // BIT 0,A
    GB_INSTRUCTION(0x47, 2, 8, bit_r8(0, A))

    // BIT 1,B
    GB_INSTRUCTION(0x48, 2, 8, bit_r8(1, B))

    // BIT 1,C
    GB_INSTRUCTION(0x49, 2, 8, bit_r8(1, C))

    // BIT 1,D
    GB_INSTRUCTION(0x4a, 2, 8, bit_r8(1, D))

    // BIT 1,E
    GB_INSTRUCTION(0x4b, 2, 8, bit_r8(1, E))

    // BIT 1,H
    GB_INSTRUCTION(0x4c, 2, 8, bit_r8(1, H))

    // BIT 1,L
    GB_INSTRUCTION(0x4d, 2, 8, bit_r8(1, L))

    // BIT 1,(HL)
    GB_INSTRUCTION(0x4e, 2, 12, bit_hl(1))

    // BIT 1,A
    GB_INSTRUCTION(0x4f, 2, 8, bit_r8(1, A))

    // BIT 2,B
    GB_INSTRUCTION(0x50, 2, 8, bit_r8(2, B))

    // BIT 2,C
    GB_INSTRUCTION(0x51, 2, 8, bit_r8(2, C))

    // BIT 2,D
    GB_INSTRUCTION(0x52, 2, 8, bit_r8(2, D))

    // BIT 2,E
    GB_INSTRUCTION(0x53, 2, 8, bit_r8(2, E))

    // BIT 2,H
    GB_INSTRUCTION(0x54, 2, 8, bit_r8(2, H))

    // BIT 2,L
    GB_INSTRUCTION(0x55, 2, 8, bit_r8(2, L))

    // BIT 2,(HL)
    GB_INSTRUCTION(0x56, 2, 12, bit_hl(2))

    // BIT 2,A
    GB_INSTRUCTION(0x57, 2, 8, bit_r8(2, A))

    // BIT 3,B
    GB_INSTRUCTION(0x58, 2, 8, bit_r8(3, B))

    // BIT 3,C
    GB_INSTRUCTION(0x59, 2, 8, bit_r8(3, C))

    // BIT 3,D
    GB_INSTRUCTION(0x5a, 2, 8, bit_r8(3, D))

    // BIT 3,E
    GB_INSTRUCTION(0x5b, 2, 8, bit_r8(3, E))

    // BIT 3,H
    GB_INSTRUCTION(0x5c, 2, 8, bit_r8(3, H))

    // BIT 3,L
    GB_INSTRUCTION(0x5d, 2, 8, bit_r8(3, L))

    // BIT 3,(HL)
    GB_INSTRUCTION(0x5e, 2, 12, bit_hl(3))

    // BIT 3,A
    GB_INSTRUCTION(0x5f, 2, 8, bit_r8(3, A))

    // BIT 4,B
    GB_INSTRUCTION(0x60, 2, 8, bit_r8(4, B))

    // BIT 4,C
    GB_INSTRUCTION(0x61, 2, 8, bit_r8(4, C))

    // BIT 4,D
    GB_INSTRUCTION(0x62, 2, 8, bit_r8(4, D))

    // BIT 4,E
    GB_INSTRUCTION(0x63, 2, 8, bit_r8(4, E))

    // BIT 4,H
    GB_INSTRUCTION(0x64, 2, 8, bit_r8(4, H))

    // BIT 4,L
    GB_INSTRUCTION(0x65, 2, 8, bit_r8(4, L))

    // BIT 4,(HL)
    GB_INSTRUCTION(0x66, 2, 12, bit_hl(4))

    // BIT 4,A
    GB_INSTRUCTION(0x67, 2, 8, bit_r8(4, A))

    // BIT 5,B
    GB_INSTRUCTION(0x68, 2, 8, bit_r8(5, B))

    // BIT 5,C
    GB_INSTRUCTION(0x69, 2, 8, bit_r8(5, C))

    // BIT 5,D
    GB_INSTRUCTION(0x6a, 2, 8, bit_r8(5, D))

    // BIT 5,E
    GB_INSTRUCTION(0x6b, 2, 8, bit_r8(5, E))

    // BIT 5,H
    GB_INSTRUCTION(0x6c, 2, 8, bit_r8(5, H))

    // BIT 5,L
    GB_INSTRUCTION(0x6d, 2, 8, bit_r8(5, L))

    // BIT 5,(HL)
    GB_INSTRUCTION(0x6e, 2, 12, bit_hl(5))

    // BIT 5,A
    GB_INSTRUCTION(0x6f, 2, 8, bit_r8(5, A))

    // BIT 6,B
    GB_INSTRUCTION(0x70, 2, 8, bit_r8(6, B))

    // BIT 6,C
    GB_INSTRUCTION(0x71, 2, 8, bit_r8(6, C))

    // BIT 6,D
    GB_INSTRUCTION(0x72, 2, 8, bit_r8(6, D))

    // BIT 6,E
    GB_INSTRUCTION(0x73, 2, 8, bit_r8(6, E))

    // BIT 6,H
    GB_INSTRUCTION(0x74, 2, 8, bit_r8(6, H))

    // BIT 6,L
    GB_INSTRUCTION(0x75, 2, 8, bit_r8(6, L))

    // BIT 6,(HL)
    GB_INSTRUCTION(0x76, 2, 12, bit_hl(6))

    // BIT 6,A
    GB_INSTRUCTION(0x77, 2, 8, bit_r8(6, A))

    // BIT 7,B
    GB_INSTRUCTION(0x78, 2, 8, bit_r8(7, B))

    // BIT 7,C
    GB_INSTRUCTION(0x79, 2, 8, bit_r8(7, C))

    // BIT 7,D
    GB_INSTRUCTION(0x7a, 2, 8, bit_r8(7, D))

    // BIT 7,E
    GB_INSTRUCTION(0x7b, 2, 8, bit_r8(7, E))

    // BIT 7,H
    GB_INSTRUCTION(0x7c, 2, 8, bit_r8(7, H))

    // BIT 7,L
    GB_INSTRUCTION(0x7d, 2, 8, bit_r8(7, L))

    // BIT 7,(HL)
    GB_INSTRUCTION(0x7e, 2, 12, bit_hl(7))

    // BIT 7,A
    GB_INSTRUCTION(0x7f, 2, 8, bit_r8(7, A))

    // RES 0,B
    GB_INSTRUCTION(0x80, 2, 8, res_u3_r8(0, B))

    // RES 0,C
    GB_INSTRUCTION(0x81, 2, 8, res_u3_r8(0, C))

    // RES 0,D
    GB_INSTRUCTION(0x82, 2, 8, res_u3_r8(0, D))

    // RES 0,E
    GB_INSTRUCTION(0x83, 2, 8, res_u3_r8(0, E))

    // RES 0,H
    GB_INSTRUCTION(0x84, 2, 8, res_u3_r8(0, H))

    // RES 0,L
    GB_INSTRUCTION(0x85, 2, 8, res_u3_r8(0, L))

    // RES 0,(HL)
    GB_INSTRUCTION(0x86, 2, 16, res_u3_hl(0))

    // RES 0,A
    GB_INSTRUCTION(0x87, 2, 8, res_u3_r8(0, A))

    // RES 1,B
    GB_INSTRUCTION(0x88, 2, 8, res_u3_r8(1, B))

    // RES 1,C
    GB_INSTRUCTION(0x89, 2, 8, res_u3_r8(1, C))

    // RES 1,D
    GB_INSTRUCTION(0x8a, 2, 8, res_u3_r8(1, D))

    // RES 1,E
    GB_INSTRUCTION(0x8b, 2, 8, res_u3_r8(1, E))

    // RES 1,H
    GB_INSTRUCTION(0x8c, 2, 8, res_u3_r8(1, H))

    // RES 1,L
    GB_INSTRUCTION(0x8d, 2, 8, res_u3_r8(1, L))

    // RES 1,(HL)
    GB_INSTRUCTION(0x8e, 2, 16, res_u3_hl(1))

    // RES 1,A
    GB_INSTRUCTION(0x8f, 2, 8, res_u3_r8(1, A))

    // RES 2,B
    GB_INSTRUCTION(0x90, 2, 8, res_u3_r8(2, B))

    // RES 2,C
    GB_INSTRUCTION(0x91, 2, 8, res_u3_r8(2, C))

    // RES 2,D
    GB_INSTRUCTION(0x92, 2, 8, res_u3_r8(2, D))

    // RES 2,E
    GB_INSTRUCTION(0x93, 2, 8, res_u3_r8(2, E))

    // RES 2,H
    GB_INSTRUCTION(0x94, 2, 8, res_u3_r8(2, H))

    // RES 2,L
    GB_INSTRUCTION(0x95, 2, 8, res_u3_r8(2, L))

    // RES 2,(HL)
    GB_INSTRUCTION(0x96, 2, 16, res_u3_hl(2))

    // RES 2,A
    GB_INSTRUCTION(0x97, 2, 8, res_u3_r8(2, A))

    // RES 3,B
    GB_INSTRUCTION(0x98, 2, 8, res_u3_r8(3, B))

    // RES 3,C
    GB_INSTRUCTION(0x99, 2, 8, res_u3_r8(3, C))

    // RES 3,D
    GB_INSTRUCTION(0x9a, 2, 8, res_u3_r8(3, D))

    // RES 3,E
    GB_INSTRUCTION(0x9b, 2, 8, res_u3_r8(3, E))

    // RES 3,H
    GB_INSTRUCTION(0x9c, 2, 8, res_u3_r8(3, H))

    // RES 3,L
    GB_INSTRUCTION(0x9d, 2, 8, res_u3_r8(3, L))

    // RES 3,(HL)
    GB_INSTRUCTION(0x9e, 2, 16, res_u3_hl(3))

    // RES 3,A
    GB_INSTRUCTION(0x9f, 2, 8, res_u3_r8(3, A))

    // RES 4,B
    GB_INSTRUCTION(0xa0, 2, 8, res_u3_r8(4, B))

    // RES 4,C
    GB_INSTRUCTION(0xa1, 2, 8, res_u3_r8(4, C))

    // RES 4,D
    GB_INSTRUCTION(0xa2, 2, 8, res_u3_r8(4, D))

    // RES 4,E
    GB_INSTRUCTION(0xa3, 2, 8, res_u3_r8(4, E))

    // RES 4,H
    GB_INSTRUCTION(0xa4, 2, 8, res_u3_r8(4, H))

    // RES 4,L
    GB_INSTRUCTION(0xa5, 2, 8, res_u3_r8(4, L))

    // RES 4,(HL)
    GB_INSTRUCTION(0xa6, 2, 16, res_u3_hl(4))

    // RES 4,A
    GB_INSTRUCTION(0xa7, 2, 8, res_u3_r8(4, A))

    // RES 5,B
    GB_INSTRUCTION(0xa8, 2, 8, res_u3_r8(5, B))

    // RES 5,C
    GB_INSTRUCTION(0xa9, 2, 8, res_u3_r8(5, C))

    // RES 5,D
    GB_INSTRUCTION(0xaa, 2, 8, res_u3_r8(5, D))

    // RES 5,E
    GB_INSTRUCTION(0xab, 2, 8, res_u3_r8(5, E))

    // RES 5,H
    GB_INSTRUCTION(0xac, 2, 8, res_u3_r8(5, H))

    // RES 5,L
    GB_INSTRUCTION(0xad, 2, 8, res_u3_r8(5, L))

    // RES 5,(HL)
    GB_INSTRUCTION(0xae, 2, 16, res_u3_hl(5))

    // RES 5,A
    GB_INSTRUCTION(0xaf, 2, 8, res_u3_r8(5, A))

    // RES 6,B
    GB_INSTRUCTION(0xb0, 2, 8, res_u3_r8(6, B))

    // RES 6,C
    GB_INSTRUCTION(0xb1, 2, 8, res_u3_r8(6, C))

    // RES 6,D
    GB_INSTRUCTION(0xb2, 2, 8, res_u3_r8(6, D))

    // RES 6,E
    GB_INSTRUCTION(0xb3, 2, 8, res_u3_r8(6, E))

    // RES 6,H
    GB_INSTRUCTION(0xb4, 2, 8, res_u3_r8(6, H))

    // RES 6,L
    GB_INSTRUCTION(0xb5, 2, 8, res_u3_r8(6, L))

    // RES 6,(HL)
    GB_INSTRUCTION(0xb6, 2, 16, res_u3_hl(6))

    // RES 6,A
    GB_INSTRUCTION(0xb7, 2, 8, res_u3_r8(6, A))

    // RES 7,B
    GB_INSTRUCTION(0xb8, 2, 8, res_u3_r8(7, B))

    // RES 7,C
    GB_INSTRUCTION(0xb9, 2, 8, res_u3_r8(7, C))

    // RES 7,D
    GB_INSTRUCTION(0xba, 2, 8, res_u3_r8(7, D))

    // RES 7,E
    GB_INSTRUCTION(0xbb, 2, 8, res_u3_r8(7, E))

    // RES 7,H
    GB_INSTRUCTION(0xbc, 2, 8, res_u3_r8(7, H))

    // RES 7,L
    GB_INSTRUCTION(0xbd, 2, 8, res_u3_r8(7, L))

    // RES 7,(HL)
    GB_INSTRUCTION(0xbe, 2, 16, res_u3_hl(7))

    // RES 7,A
    GB_INSTRUCTION(0xbf, 2, 8, res_u3_r8(7, A))

    // SET 0,B
    GB_INSTRUCTION(0xc0, 2, 8, set_u3_r8(0, B))

    // SET 0,C
    GB_INSTRUCTION(0xc1, 2, 8, set_u3_r8(0, C))

    // SET 0,D
    GB_INSTRUCTION(0xc2, 2, 8, set_u3_r8(0, D))

    // SET 0,E
    GB_INSTRUCTION(0xc3, 2, 8, set_u3_r8(0, E))

    // SET 0,H
    GB_INSTRUCTION(0xc4, 2, 8, set_u3_r8(0, H))

    // SET 0,L
    GB_INSTRUCTION(0xc5, 2, 8, set_u3_r8(0, L))

    // SET 0,(HL)
    GB_INSTRUCTION(0xc6, 2, 16, set_u3_hl(0))

    // SET 0,A
    GB_INSTRUCTION(0xc7, 2, 8, set_u3_r8(0, A))

    // SET 1,B
    GB_INSTRUCTION(0xc8, 2, 8, set_u3_r8(1, B))

    // SET 1,C
    GB_INSTRUCTION(0xc9, 2, 8, set_u3_r8(1, C))

    // SET 1,D
    GB_INSTRUCTION(0xca, 2, 8, set_u3_r8(1, D))

    // SET 1,E
    GB_INSTRUCTION(0xcb, 2, 8, set_u3_r8(1, E))

    // SET 1,H
    GB_INSTRUCTION(0xcc, 2, 8, set_u3_r8(1, H))

    // SET 1,L
    GB_INSTRUCTION(0xcd, 2, 8, set_u3_r8(1, L))

    // SET 1,(HL)
    GB_INSTRUCTION(0xce, 2, 16, set_u3_hl(1))

    // SET 1,A
    GB_INSTRUCTION(0xcf, 2, 8, set_u3_r8(1, A))

    // SET 2,B
    GB_INSTRUCTION(0xd0, 2, 8, set_u3_r8(2, B))

    // SET 2,C
    GB_INSTRUCTION(0xd1, 2, 8, set_u3_r8(2, C))

    // SET 2,D
    GB_INSTRUCTION(0xd2, 2, 8, set_u3_r8(2, D))

    // SET 2,E
    GB_INSTRUCTION(0xd3, 2, 8, set_u3_r8(2, E))

    // SET 2,H
    GB_INSTRUCTION(0xd4, 2, 8, set_u3_r8(2, H))

    // SET 2,L
    GB_INSTRUCTION(0xd5, 2, 8, set_u3_r8(2, L))

    // SET 2,(HL)
    GB_INSTRUCTION(0xd6, 2, 16, set_u3_hl(2))

    // SET 2,A
    GB_INSTRUCTION(0xd7, 2, 8, set_u3_r8(2, A))

    // SET 3,B
    GB_INSTRUCTION(0xd8, 2, 8, set_u3_r8(3, B))

    // SET 3,C
    GB_INSTRUCTION(0xd9, 2, 8, set_u3_r8(3, C))

    // SET 3,D
    GB_INSTRUCTION(0xda, 2, 8, set_u3_r8(3, D))

    // SET 3,E
    GB_INSTRUCTION(0xdb, 2, 8, set_u3_r8(3, E))

    // SET 3,H
    GB_INSTRUCTION(0xdc, 2, 8, set_u3_r8(3, H))

    // SET 3,L
    GB_INSTRUCTION(0xdd, 2, 8, set_u3_r8(3, L))

    // SET 3,(HL)
    GB_INSTRUCTION(0xde, 2, 16, set_u3_hl(3))

    // SET 3,A
    GB_INSTRUCTION(0xdf, 2, 8, set_u3_r8(3, A))

    // SET 4,B
    GB_INSTRUCTION(0xe0, 2, 8, set_u3_r8(4, B))

    // SET 4,C
    GB_INSTRUCTION(0xe1, 2, 8, set_u3_r8(4, C))

    // SET 4,D
    GB_INSTRUCTION(0xe2, 2, 8, set_u3_r8(4, D))

    // SET 4,E
    GB_INSTRUCTION(0xe3, 2, 8, set_u3_r8(4, E))

    // SET 4,H
    GB_INSTRUCTION(0xe4, 2, 8, set_u3_r8(4, H))

    // SET 4,L
    GB_INSTRUCTION(0xe5, 2, 8, set_u3_r8(4, L))

    // SET 4,(HL)
    GB_INSTRUCTION(0xe6, 2, 16, set_u3_hl(4))

    // SET 4,A
    GB_INSTRUCTION(0xe7, 2, 8, set_u3_r8(4, A))

    // SET 5,B
    GB_INSTRUCTION(0xe8, 2, 8, set_u3_r8(5, B))

    // SET 5,C
    GB_INSTRUCTION(0xe9, 2, 8, set_u3_r8(5, C))

    // SET 5,D
    GB_INSTRUCTION(0xea, 2, 8, set_u3_r8(5, D))

    // SET 5,E
    GB_INSTRUCTION(0xeb, 2, 8, set_u3_r8(5, E))

    // SET 5,H
    GB_INSTRUCTION(0xec, 2, 8, set_u3_r8(5, H))

    // SET 5,L
    GB_INSTRUCTION(0xed, 2, 8, set_u3_r8(5, L))

    // SET 5,(HL)
    GB_INSTRUCTION(0xee, 2, 16, set_u3_hl(5))

    // SET 5,A
    GB_INSTRUCTION(0xef, 2, 8, set_u3_r8(5, A))

    // SET 6,B
    GB_INSTRUCTION(0xf0, 2, 8, set_u3_r8(6, B))

    // SET 6,C
    GB_INSTRUCTION(0xf1, 2, 8, set_u3_r8(6, C))

    // SET 6,D
    GB_INSTRUCTION(0xf2, 2, 8, set_u3_r8(6, D))

    // SET 6,E
    GB_INSTRUCTION(0xf3, 2, 8, set_u3_r8(6, E))

    // SET 6,H
    GB_INSTRUCTION(0xf4, 2, 8, set_u3_r8(6, H))

    // SET 6,L
    GB_INSTRUCTION(0xf5, 2, 8, set_u3_r8(6, L))

    // SET 6,(HL)
    GB_INSTRUCTION(0xf6, 2, 16, set_u3_hl(6))

    // SET 6,A
    GB_INSTRUCTION(0xf7, 2, 8, set_u3_r8(6, A))

    // SET 7,B
    GB_INSTRUCTION(0xf8, 2, 8, set_u3_r8(7, B))

    // SET 7,C
    GB_INSTRUCTION(0xf9, 2, 8, set_u3_r8(7, C))

    // SET 7,D
    GB_INSTRUCTION(0xfa, 2, 8, set_u3_r8(7, D))

    // SET 7,E
    GB_INSTRUCTION(0xfb, 2, 8, set_u3_r8(7, E))

    // SET 7,H
    GB_INSTRUCTION(0xfc, 2, 8, set_u3_r8(7, H))

    // SET 7,L
    GB_INSTRUCTION(0xfd, 2, 8, set_u3_r8(7, L))

    // SET 7,(HL)
    GB_INSTRUCTION(0xfe, 2, 16, set_u3_hl(7))

    // SET 7,A
    GB_INSTRUCTION(0xff, 2, 8, set_u3_r8(7, A))
  }
}
}  // namespace gb
