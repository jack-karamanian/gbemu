#pragma once
#include <nonstd/span.hpp>
#include <string>
#include <vector>
#include "utils.h"

namespace experiments {
std::vector<gb::u8> assemble(const std::string& src);
std::vector<std::tuple<std::string, gb::u32>> disassemble(
    nonstd::span<gb::u8> bytes);
}  // namespace experiments
