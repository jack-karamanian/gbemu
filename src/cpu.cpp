#include "cpu.h"
#include <string>
#include <array>
#include <functional>
#include <iostream>
#include <sstream>


namespace gb {
  struct Instruction {
    std::string name;
    std::function<void()> impl;
  };

  struct InstructionTable {
    Cpu *cpu;

    std::array<Instruction, 256> instructions =
    {
      {
        { "NOOP", std::bind(&Cpu::noop, cpu) }, // 0x01
        { "LD BC, d16", std::bind(&Cpu::load_16, cpu, Cpu::Register::BC) }, // 0x02
        { "LD (BC), A", std::bind(&Cpu::load_reg_to_addr, cpu, Cpu::Register::BC, Cpu::Register::A) }, // 0x03
        { "INC BC", std::bind(&Cpu::inc_16, cpu, Cpu::Register::BC) }, // 0x04
        { "INC B", std::bind(&Cpu::inc_8, cpu, Cpu::Register::B) } // 0x05
      }
    };

    InstructionTable(Cpu &cpu) : cpu(&cpu) {}
  };

  void Cpu::decode(const Memory& memory) {
    u8 opcode = fetch();

    switch (opcode) {
      // LD
      case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x47: // LD B reg
      case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4f: // LD C reg
      case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x57: // LD D reg
      case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5f: // LD E reg
      case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x67: // LD H reg
      case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6f: // LD L reg
      case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7f: // LD A reg
      {
        uint8_t &dst = regs[opcode >> 3 & 7];
        uint8_t src = regs[opcode & 7];
        dst = src;
        break;
      }
      // ADC
      case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x87:
      case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8f:
      {
        bool carry = (opcode & 8) && get_carry();
        break;
      }

      // ADC [HL]
      case 0x8e:
      {
        break;
      }
      default:
      {
        std::ostringstream stream;
        stream << "Unsupported instruction: " << std::hex << opcode;
        throw std::runtime_error(stream.str());
      }
    }
  }
}
