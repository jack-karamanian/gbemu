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
      sp(0xfffe),
      pc(0),
      m(0),
      t(0),
      a(*&regs[Register::A]),
      b(*&regs[Register::B]),
      c(*&regs[Register::C]),
      bc(*(reinterpret_cast<u16*>(&regs[Register::BC]))),
      d(*&regs[Register::D]),
      e(*&regs[Register::E]),
      de(*(reinterpret_cast<u16*>(&regs[Register::DE]))),
      h(*&regs[Register::H]),
      l(*&regs[Register::L]),
      hl(*(reinterpret_cast<u16*>(&regs[Register::HL]))),
      f(*&regs[Register::F]),
      memory(memory),
      instruction_table(*this) {}

const Instruction& Cpu::fetch() {
  u8 opcode = memory.memory[pc];
  if (opcode == 0xCB) {
    pc++;
    t += 4;
    return instruction_table.cb_instructions[memory.memory[pc]];
  }
  return instruction_table.instructions[opcode];
}

void Cpu::fetch_and_decode() {
  if (stopped) {
    return;
  }
  const Instruction& inst = fetch();
  std::cout << inst.name << std::endl;
  std::cout << "opcode: " << std::hex << +memory.memory[pc] << std::endl;
  const int operands_size = (inst.size - 1);

  if (operands_size == 1) {
    std::cout << std::hex << +*memory.at(pc + 1) << std::endl;
  } else if (operands_size == 2) {
    std::cout << std::hex << *memory.at<u16>(pc + 1) << std::endl;
  }

  std::cout << std::hex << +*memory.at<u16>(pc + 2) << std::endl;
  current_opcode = memory.memory[pc];
  current_operand = &memory.memory[pc + 1];
  current_pc = pc;

  pc += inst.size;
  t += inst.cycles;

  inst.impl();

  debug_write();
}

void Cpu::handle_interrupts() {
  if (interrupts_enabled) {
    u8 interrupts_register = *get_interrupts_register();
    for (int i = 0; i < 5; i++) {
      u8 current_interrupt = interrupts_register & (0x1 << i);
      if (current_interrupt) {
        handle_interrupt((Interrupt)current_interrupt);
        break;
      }
    }
  }
}

bool Cpu::handle_interrupt(Interrupt interrupt) {
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
  }
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
            << " HL: " << std::setw(6) << hl << std::endl
            << "PC: " << std::setw(6) << pc << std::endl
            << "SP: " << std::setw(6) << sp << std::endl
            << "memory[SP]: " << std::setw(6) << (*((u16*)&memory.memory[sp]))
            << std::endl
            << std::endl;
}
}  // namespace gb
