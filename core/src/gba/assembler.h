#pragma once
#include <nonstd/span.hpp>
#include <string>
#include <vector>
#include "utils.h"

namespace experiments {

std::vector<gb::u8> assemble(const std::string& src);
struct DisassemblyEntry {
  std::string text;
  unsigned int loc;

  DisassemblyEntry(std::string text_, unsigned int loc_)
      : text{std::move(text_)}, loc{loc_} {}
};

std::vector<DisassemblyEntry> disassemble(nonstd::span<gb::u8> bytes,
                                          std::string_view arch = "arm");
}  // namespace experiments
