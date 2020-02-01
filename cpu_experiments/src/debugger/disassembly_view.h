#pragma once
#include <vector>
#include "gba/assembler.h"
#include "types.h"

namespace gb::advance {
class Cpu;

struct DisassemblyInfo {
  std::vector<std::string> disassembly_cache;
};

class DisassemblyView {
 public:
  constexpr explicit DisassemblyView(const char* name) : m_name{name} {}
  void render(u32 base,
              int instr_size,
              const Cpu& cpu,
              DisassemblyInfo& disassembly_info);

 private:
  const char* m_name;
  u32 m_prev_offset = 0;
  std::array<char, 9> m_number_buffer{};
  int m_prev_instr_size = 0;
  int m_prev_size = 0;
  int m_num_potential_instructions = 0;
};
}  // namespace gb::advance
