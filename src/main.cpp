#include <iostream>
#include "cpu.h"
#include "memory.h"

int main(int argc, const char** argv) {
  (void)argc;
  (void)argv;

  static_assert(std::is_same<next_largest_type<u16>::type, u32>::value,
                " not same");
  gb::Memory memory;
  gb::Cpu cpu(memory);

  cpu.pc = 0;

  cpu.set_16(gb::Cpu::Register::HL, 0);

  // LD HL,FE00
  memory.memory[0] = 0xCD;
  memory.memory[1] = 0x00;
  memory.memory[2] = 0xFE;

  u16 val = *((u16*)&memory.memory[1]);

  cpu.set_16(gb::Cpu::Register::HL, val);

  std::cout << std::hex << val << std::endl;
  std::cout << std::hex << cpu.get_r16(gb::Cpu::Register::HL) << std::endl;

  return 0;
}
