#include <array>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include "cpu.h"

namespace gb {
Cpu::Cpu(Memory& memory)
    : regs{0x13, 0x00, 0xd8, 0x00, 0x4d, 0x01, 0xb0, 0x01},
      sp{0xfffe},
      pc{0},
      m{0},
      ticks{0},
      memory{&memory},
      instruction_table(*this) {}

const Instruction& Cpu::fetch() {
  const u8 opcode = memory->at(pc);
  if (opcode == 0xCB) {
    pc++;
    ticks += 4;
    return instruction_table.cb_instructions.at(memory->at(pc));
  }
  return instruction_table.instructions.at(opcode);
}

int Cpu::fetch_and_decode() {
  ticks = 0;
  if (stopped || halted) {
    return 4;
  }

  if (queue_interrupts_enabled) {
    interrupts_enabled = true;
    queue_interrupts_enabled = false;
  }

  const Instruction& inst = fetch();
  if (debug) {
    std::cout << inst.name << std::endl;
    std::cout << "opcode: " << std::hex << +memory->at(pc) << std::endl;
  }
  const int operands_size = (inst.size - 1);

  if (operands_size == 1) {
    if (debug) {
      std::cout << std::hex << +memory->at(pc + 1) << std::endl;
    }
    current_operand = memory->at(pc + 1);
  } else if (operands_size == 2) {
    if (debug) {
      std::cout << std::hex << memory->at<u16>(pc + 1) << std::endl;
    }
    current_operand = memory->at<u16>(pc + 1);
  }

  current_opcode = memory->at(pc);

  pc += inst.size;
  ticks += inst.cycles;

  inst.impl();

  return ticks;
}

int Cpu::handle_interrupts() {
  if (interrupts_enabled) {
    for (int i = 0; i < 5; i++) {
      u8 interrupt = 0x01 << i;
      if (has_interrupt(interrupt) && interrupt_enabled(interrupt)) {
        handle_interrupt(interrupt);
        return 20;
      }
    }
  }
  return 0;
}

bool Cpu::handle_interrupt(u8 interrupt) {
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
    default:
      return false;
  }
  return true;
}

void Cpu::debug_write() {
  std::cout << "A: " << std::hex << std::showbase << std::setw(4)
            << +regs[Register::A] << std::endl
            << "F: " << std::setw(4) << +regs[Register::F] << std::endl
            << "B: " << std::setw(4) << +regs[Register::B]
            << " C: " << std::setw(4) << +regs[Register::C]
            << " BC: " << std::setw(6) << get_r16(Register::BC) << std::endl
            << "D: " << std::setw(4) << +regs[Register::D]
            << " E: " << std::setw(4) << +regs[Register::E]
            << " DE: " << std::setw(6) << get_r16(Register::DE) << std::endl
            << "H: " << std::setw(4) << +regs[Register::H]
            << " L: " << std::setw(4) << +regs[Register::L]
            << " HL: " << std::setw(6) << get_r16(Register::HL) << std::endl
            << "PC: " << std::setw(6) << pc << std::endl
            << "SP: " << std::setw(6) << sp << std::endl
            << std::endl
            << std::endl;
}

void Cpu::set_half_carry_subtract(const u8& a, const u8& b) {
  bool half_carry = (b & 0x0f) > (a & 0x0f);

  if (half_carry) {
    set_flag(FLAG_HALF_CARRY);
  } else {
    clear_flag(FLAG_HALF_CARRY);
  }
}

u8 Cpu::get_interrupts_register() const {
  return memory->at(MemoryRegister::InterruptRequest);
}

bool Cpu::interrupt_enabled(u8 interrupt) const {
  const u8 interrupts = memory->get_interrupts_enabled();
  return (interrupts & interrupt) != 0;
}

bool Cpu::has_interrupt(u8 interrupt) const {
  return (memory->get_interrupts_request() & interrupt) != 0;
}

void Cpu::request_interrupt(Interrupt interrupt) {
  const u8 interrupts = memory->at(MemoryRegister::InterruptRequest);
  memory->set(MemoryRegister::InterruptRequest, interrupts | interrupt);
}

void Cpu::clear_interrupt(const u8 interrupt) const {
  const u8 interrupts = memory->at(0xff0f);
  memory->set(0xff0f, interrupts & ~interrupt);
}

void Cpu::invalid() const {
  std::ostringstream s;
  s << "invalid instruction: " << std::hex << +memory->at(pc - 1) << std::endl;
  throw std::runtime_error(s.str());
}

void Cpu::carried_add(u8& dest, const u8& a, const u8& b) {
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

void Cpu::add(u8& dest, const u8& a, const u8& b) {
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
  const u8 val = memory->at(hl);

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
  const u8& val = regs[reg];

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
  const u8 val = memory->at(addr);

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
  u16& hl = get_r16(Register::HL);
  u16& r16 = get_r16(reg);

  const u16 res = hl + r16;

  // set_half_carry(hl, r16);

  if (((hl & 0xff) + (r16 & 0xff)) > 0xff) {
    set_flag(FLAG_HALF_CARRY);
  } else {
    clear_flag(FLAG_HALF_CARRY);
  }
  set_carry(hl, r16);
  clear_flag(FLAG_SUBTRACT);

  hl = res;
}

// ADD HL,SP
void Cpu::add_hl_sp() {
  u16& hl = get_r16(Register::HL);
  u16 res = hl + sp;

  set_half_carry(hl, sp);
  set_carry(hl, sp);
  clear_flag(FLAG_SUBTRACT);

  hl = res;
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

  set_half_carry(sp, (u16)val);

  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);

  sp = res;
}

void Cpu::and_a(const u8& val) {
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
  const u16& addr = get_r16(Register::HL);
  const u8 val = memory->at(addr);

  and_a(val);
}

// AND A,n8
void Cpu::and_a_d8() {
  u8 val = read_operand();

  and_a(val);
}

void Cpu::bit(const u8& bit_num, const u8& val) {
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
  u16& addr = get_r16(Register::HL);
  const u8 val = memory->at(addr);
  bit(bit_num, val);
}

// CALL,nn
void Cpu::call() {
  const u16 addr = read_operand<u16>();
  // u16 next_op = pc + 2;
  // u8 pc_low = (next_op & 0xff00) >> 8;
  // bu8 pc_high = (next_op & 0x00ff);
  // bmemory->set(--sp, pc_high);
  // memory->set(--sp, pc_low);
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

void Cpu::compare_a(const u8& val) {
  const u8& a = regs[Register::A];

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
  u16& val = get_r16(reg);
  val--;
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

  set_half_carry(val, (u8)1);
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
  u16& r16 = get_r16(reg);
  r16++;
}

// INC SP
void Cpu::inc_sp() {
  sp++;
}

void Cpu::jump(const u16& addr) {
  pc = addr;
}

// JP n16
void Cpu::jp_d16() {
  const u16 addr = read_operand<u16>();
  jump(addr);
}

bool Cpu::can_jump(const u8& opcode, int offset) {
  int index = ((opcode & 0x38) >> 3) - offset;

  return ((index == JumpCondition::NZ && !get_flag(FLAG_ZERO)) ||
          (index == JumpCondition::Z && get_flag(FLAG_ZERO)) ||
          (index == JumpCondition::NC && !get_flag(FLAG_CARRY)) ||
          (index == JumpCondition::C && get_flag(FLAG_CARRY)));
}

void Cpu::jump_conditional(const u16& addr, int index_offset) {
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
  const u8& val = read_operand();
  regs[dst] = val;
}

// LD r16,n16
void Cpu::ld_r16_d16(Register dst) {
  u16& r16 = get_r16(dst);
  const u16& val = read_operand<u16>();

  r16 = val;
}

// LD [HL],r8
void Cpu::ld_hl_r8(Register reg) {
  const u16& hl = get_r16(Register::HL);
  // u8* val = memory->at(hl);
  //*val = regs[reg];
  memory->set(hl, regs[reg]);
}

// LD [HL],n8
void Cpu::ld_hl_d8() {
  const u16& hl = get_r16(Register::HL);
  // u8* val = memory->at(hl);
  //*val = read_operand();
  memory->set(hl, read_operand());
}

// LD r8,[HL]
void Cpu::ld_r8_hl(Register reg) {
  const u16& hl = get_r16(Register::HL);
  const u8 val = memory->at(hl);
  regs[reg] = val;
}

// LD [r16], A
void Cpu::ld_r16_a(Register reg) {
  // u8& val = value_at_r16(reg);
  // val = regs[Register::A];
  memory->set(get_r16(reg), regs[Register::A]);
}

// LD [n16],A
void Cpu::ld_d16_a() {
  const u16& addr = read_operand<u16>();
  // u8* val = memory->at(addr);
  //*val = regs[Register::A];
  memory->set(addr, regs[Register::A]);
}

void Cpu::load_offset(const u8& offset, const u8& val) {
  memory->set(0xff00 + offset, val);
}

// LD [$FF00 + n8],A
void Cpu::ld_offset_a() {
  const u8& offset = read_operand();
  load_offset(offset, regs[Register::A]);
}

// LD [$FF00 + C],A
void Cpu::ld_offset_c_a() {
  load_offset(regs[Register::C], regs[Register::A]);
}

// LD A,[r16]
void Cpu::ld_a_r16(Register reg) {
  const u8& val = value_at_r16(reg);
  regs[Register::A] = val;
}

// LD A,[n16]
void Cpu::ld_a_d16() {
  const u16& addr = read_operand<u16>();
  const u8 val = memory->at(addr);
  regs[Register::A] = val;
}

void Cpu::read_offset_from_memory(const u8& offset, u8& dest) {
  const u8 val = memory->at(0xff00 + offset);
  dest = val;
}

// LD A,[$FF00 + n8]
void Cpu::ld_read_offset_d8() {
  const u8& offset = read_operand();
  read_offset_from_memory(offset, regs[Register::A]);
}

// LD A,[$FF00 + C]
void Cpu::ld_read_offset_c() {
  read_offset_from_memory(regs[Register::C], regs[Register::A]);
}

// TODO: fix this
void Cpu::load_hl_a() {
  const u16& hl = get_r16(Register::HL);
  memory->set(hl, regs[Register::A]);
}

void Cpu::load_a_hl() {
  const u8& val = value_at_r16(Register::HL);
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

  memory->set(addr, sp & 0xff);
  memory->set(addr + 1, sp >> 8);
}

// LD HL,SP+e8
void Cpu::ld_hl_sp_s8() {
  u16& hl = get_r16(Register::HL);

  const s8 val = read_operand();
  int res = sp + val;
  if (res & 0xffff0000) {
    set_flag(FLAG_CARRY);
  } else {
    clear_flag(FLAG_CARRY);
  }

  set_half_carry(sp, (u16)val);

  clear_flag(FLAG_ZERO);
  clear_flag(FLAG_SUBTRACT);

  hl = res;
}

// LD SP,HL
void Cpu::ld_sp_hl() {
  const u16& hl = get_r16(Register::HL);
  sp = hl;
}

void Cpu::or_a(const u8& val) {
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
  const u8 low = memory->at(sp + 1);
  const u8 high = memory->at(sp);
  sp += 2;

  reg = (((u16)low) << 8) | high;
}

// POP AF
void Cpu::pop_af() {
  u16& af = get_r16(Register::F);
  pop(af);
  regs[Register::F] &= 0xf0;
}

// POP r16
void Cpu::pop_r16(Register reg) {
  pop(get_r16(reg));
}

void Cpu::push(const u16& val) {
  u8 low = (val & 0xff00) >> 8;
  u8 high = val & 0xff;

  memory->set(--sp, low);
  memory->set(--sp, high);
}

// PUSH AF
void Cpu::push_af() {
  push(get_r16(Register::F));
}

// PUSH r16
void Cpu::push_r16(Register reg) {
  push(get_r16(reg));
}

void Cpu::set_bit(u8& dest, const u8& bit, bool set) {
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
  // u8& val = value_at_r16(Register::HL);
  // rotate_zero(val);
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
  memory->set(get_r16(Register::HL), val);
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
  memory->set(get_r16(Register::HL), val);
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

void Cpu::carried_subtract(u8& dst, const u8& src) {
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
void Cpu::set_u3_r8(const u8& bit, Register reg) {
  set_bit(regs[reg], bit, true);
}

// SET u3,[HL]
void Cpu::set_u3_hl(const u8& bit) {
  u8 val = value_at_r16(Register::HL);
  set_bit(val, bit, true);
  memory->set(get_r16(Register::HL), val);
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
  memory->set(get_r16(Register::HL), val);
}

// SRA r8
void Cpu::sra_r8(Register reg) {
  shift_arithmetic(regs[reg], false);
}

// SRA [HL]
void Cpu::sra_hl() {
  u8 val = value_at_r16(Register::HL);
  shift_arithmetic(val, false);
  memory->set(get_r16(Register::HL), val);
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
  memory->set(get_r16(Register::HL), val);
}

// STOP
void Cpu::stop() {
  stopped = true;
}

void Cpu::subtract(u8& dst, const u8& src) {
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
void Cpu::exclusive_or(u8& dst, const u8& src) {
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

// void set_16(u8 &reg_high, u8 &reg_low, u16 val) {
//  reg_high = (val & 0xFF00) >> 8;
//  reg_low = (val & 0xFF);
//}

void Cpu::load_reg_to_addr(Register dst, Register src) {
  u16 addr = regs[dst];
  memory->set(addr, src);
}

u16& Cpu::get_r16(Register reg) {
  return (u16&)*&regs[reg];
}

u8 Cpu::value_at_r16(Register reg) {
  u16& addr = get_r16(reg);
  return memory->at(addr);
}
}  // namespace gb
