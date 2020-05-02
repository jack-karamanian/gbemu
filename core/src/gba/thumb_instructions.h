#pragma once
#include <array>
#include "gba/hardware.h"
#include "types.h"

namespace gb::advance {

using ThumbInstFunc = u32 (*)(Cpu&, u16);

struct ThumbInstructionTables {
  std::array<ThumbInstFunc, 1024> interpreter_table{};
  std::array<ThumbInstFunc, 1024> compiler_table{};
};

extern const ThumbInstructionTables thumb_instruction_tables;

namespace thumb {
u32 conditional_branch(Cpu&, u16 instruction);
}

}  // namespace gb::advance
