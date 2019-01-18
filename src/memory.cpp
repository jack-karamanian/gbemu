#include "memory.h"

namespace gb {
void Memory::set(const u16& addr, const u8& val) {
  if (addr == 0xff46) {
    do_dma_transfer(val);
  }
  memory[addr] = val;
}

void Memory::do_dma_transfer(const u8& data) {
  const u16 addr = ((u16)data) << 8;
  for (u16 i = 0; i < 160; i++) {
    memory[0xfe00 + i] = memory[addr + i];
  }
}
}  // namespace gb
