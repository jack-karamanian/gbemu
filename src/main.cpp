#include "cpu.h"
#include "memory.h"

int main(int argc, const char **argv) {
  gb::Memory memory;
  gb::Cpu cpu(memory);
  cpu.decode(memory);
  return 0;
}
