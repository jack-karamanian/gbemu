#pragma once
#include <folly/container/F14Map.h>
#include <vector>
#include "gba/assembler.h"
#include "types.h"

namespace gb::advance {
class Cpu;
struct DisassemblyInfo {
  std::vector<experiments::DisassemblyEntry> disassembly;
  folly::F14FastMap<u32, u32> addr_to_index;
};

class DisassemblyView {
 public:
  DisassemblyView(const char* name, u32 instruction_size)
      : m_name{name}, m_instruction_size{instruction_size} {}
  void render(u32 base,
              const Cpu& cpu,
              const DisassemblyInfo& disassembly_info);

 private:
  const char* m_name;
  u32 m_prev_offset = 0;
  u32 m_instruction_size;
  std::array<char, 9> m_number_buffer;
  folly::F14FastMap<unsigned int, experiments::DisassemblyEntry>
      m_address_to_disassembly;
};
}  // namespace gb::advance
