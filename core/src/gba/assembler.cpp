#include "gba/assembler.h"
#include <capstone/capstone.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <iostream>

namespace experiments {

std::vector<DisassemblyEntry> disassemble(nonstd::span<gb::u8> bytes,
                                          DisassemblyMode arch) {
  std::vector<DisassemblyEntry> asm_lines;

  csh handle;

  const auto mode = arch == DisassemblyMode::Arm ? CS_MODE_ARM : CS_MODE_THUMB;

  if (cs_open(CS_ARCH_ARM, mode, &handle) != CS_ERR_OK) {
    fmt::print(std::cerr, "Failed to open capstone\n");
    return asm_lines;
  }
  asm_lines.reserve(1);

  cs_insn* insn = nullptr;
  std::size_t count =
      cs_disasm(handle, bytes.data(), bytes.size(), 0, 0, &insn);

  if (count == 0 && mode == CS_MODE_THUMB) {
    count = cs_disasm(handle, bytes.data(), 4, 0, 0, &insn);
  }

  if (count == 0) {
    asm_lines.emplace_back("invalid");
  } else {
    for (std::size_t i = 0; i < count; ++i) {
      asm_lines.emplace_back(
          fmt::format("{} {}\n", insn[i].mnemonic, insn[i].op_str));
    }
    cs_free(insn, count);
  }

  cs_close(&handle);

  return asm_lines;
}
}  // namespace experiments
