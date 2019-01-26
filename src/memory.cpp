#include <iostream>
#include "memory.h"

namespace gb {
void Memory::set(const u16& addr, const u8& val) {
  if (addr == 0xff46) {
    do_dma_transfer(val);
  }

  if (addr == 0xff02 && val == 0x81) {
    std::cout << "char: " << memory[0xff01] << std::endl;
  }

  if (addr >= 0x8000) {
    memory[addr] = val;
  }
}

void Memory::do_dma_transfer(const u8& data) {
  const u16 addr = ((u16)data) << 8;
  for (u16 i = 0; i < 160; i++) {
    memory[0xfe00 + i] = memory[addr + i];
  }
}

u8* Memory::get_input_register() {
  return &memory[0xff00];
}
}  // namespace gb
