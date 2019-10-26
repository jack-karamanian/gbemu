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
  constexpr explicit DisassemblyView(const char* name) : m_name{name} {}
  void render(u32 base,
              const Cpu& cpu,
              const DisassemblyInfo& disassembly_info);

 private:
  const char* m_name;
  u32 m_prev_offset = 0;
  std::array<char, 9> m_number_buffer{};
};
}  // namespace gb::advance
