#pragma once
#include <nonstd/span.hpp>
#include <string>
#include <vector>
#include "utils.h"

namespace experiments {

struct DisassemblyEntry {
  std::string text;

  explicit DisassemblyEntry(std::string text_) : text{std::move(text_)} {}
};

enum class DisassemblyMode {
  Arm,
  Thumb,
};

std::vector<DisassemblyEntry> disassemble(
    nonstd::span<gb::u8> bytes,
    DisassemblyMode arch = DisassemblyMode::Arm);
}  // namespace experiments
