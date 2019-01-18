#include <fstream>
#include <iostream>
#include <string>
#include "cpu.h"
#include "lcd.h"
#include "memory.h"
#include "system.h"

int main(int argc, const char** argv) {
  (void)argc;

  static_assert(std::is_same<next_largest_type<u16>::type, u32>::value,
                " not same");
  std::string rom_name = argv[1];

  gb::System system;

  std::cout << rom_name << std::endl;

  std::ifstream rom(rom_name, std::ios::in | std::ios::binary);
  rom.seekg(0, std::ios::end);

  int rom_size = rom.tellg();

  rom.seekg(0);

  rom.read(reinterpret_cast<char*>(&system.memory->memory[0]), rom_size);

  rom.close();
  // cpu.get_r16(gb::Cpu::Register::HL) = 0xabcd;

  system.cpu->pc = 0x100;
  system.memory->memory[0xFF05] = 0x00;
  system.memory->memory[0xFF06] = 0x00;
  system.memory->memory[0xFF07] = 0x00;
  system.memory->memory[0xFF10] = 0x80;
  system.memory->memory[0xFF11] = 0xBF;
  system.memory->memory[0xFF12] = 0xF3;
  system.memory->memory[0xFF14] = 0xBF;
  system.memory->memory[0xFF16] = 0x3F;
  system.memory->memory[0xFF17] = 0x00;
  system.memory->memory[0xFF19] = 0xBF;
  system.memory->memory[0xFF1A] = 0x7F;
  system.memory->memory[0xFF1B] = 0xFF;
  system.memory->memory[0xFF1C] = 0x9F;
  system.memory->memory[0xFF1E] = 0xBF;
  system.memory->memory[0xFF20] = 0xFF;
  system.memory->memory[0xFF21] = 0x00;
  system.memory->memory[0xFF22] = 0x00;
  system.memory->memory[0xFF23] = 0xBF;
  system.memory->memory[0xFF24] = 0x77;
  system.memory->memory[0xFF25] = 0xF3;
  system.memory->memory[0xFF26] = 0xF1;
  system.memory->memory[0xFF40] = 0x91;
  system.memory->memory[0xFF42] = 0x00;
  system.memory->memory[0xFF43] = 0x00;
  system.memory->memory[0xFF45] = 0x00;
  system.memory->memory[0xFF47] = 0xFC;
  system.memory->memory[0xFF48] = 0xFF;
  system.memory->memory[0xFF49] = 0xFF;
  system.memory->memory[0xFF4A] = 0x00;
  system.memory->memory[0xFF4B] = 0x00;
  system.memory->memory[0xFFFF] = 0x00;
  // cpu.pc = 0x25d;

  system.cpu->debug_write();
  while (true) {
    // std::cin.get();
    system.cpu->fetch_and_decode();
  }
  system.cpu->debug_write();

  // LD HL,FE00
  system.memory->memory[0] = 0xCD;
  system.memory->memory[1] = 0x00;
  system.memory->memory[2] = 0xFE;

  u16 val = *((u16*)&system.memory->memory[1]);

  std::cout << std::hex << val << std::endl;
  std::cout << std::hex << system.cpu->get_r16(gb::Cpu::Register::HL)
            << std::endl;

  return 0;
}
