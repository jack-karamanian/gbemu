#include "cpu.h"
#include "memory.h"

int main(int argc, const char **argv) {
  next_largest_type<u8>::type x = 2;
  static_assert(std::is_same<next_largest_type<u16>::type, u32>::value, " not same");
  gb::Memory memory;
  gb::Cpu cpu(memory);
  cpu.decode(memory);
  return 0;
}
