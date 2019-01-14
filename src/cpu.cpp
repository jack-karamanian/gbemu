#include <array>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include "cpu.h"

namespace gb {
Cpu::Cpu(Memory& memory)
    : regs{0},
      sp(0),
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

void Cpu::fetch_and_decode() {}

void Cpu::debug_write() {
  std::cout << std::setfill('0') << "A: " << std::hex << std::showbase
            << std::setw(4) << +regs[Register::A] << std::endl
            << "F: " << std::setw(4) << +regs[Register::F] << std::endl
            << "B: " << std::setw(4) << +regs[Register::B]
            << " C: " << std::setw(4) << +regs[Register::C]
            << " BC: " << std::setw(6) << get_r16(Register::BC) << std::endl
            << "D: " << std::setw(4) << +regs[Register::D]
            << " E: " << std::setw(4) << +regs[Register::D]
            << " DE: " << std::setw(6) << get_r16(Register::DE) << std::endl
            << "H: " << std::setw(4) << +regs[Register::H]
            << " L: " << std::setw(4) << +regs[Register::L]
            << " HL: " << std::setw(6) << hl << std::endl;
}
}  // namespace gb
