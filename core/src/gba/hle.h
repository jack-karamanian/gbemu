#pragma once
#include <cmath>
#include <nonstd/span.hpp>
#include "types.h"

namespace gb::advance {
class Mmu;
namespace hle::bios {
void cpu_set(Mmu& mmu, u32 source, u32 dest, u32 control);

void cpu_fast_set(Mmu& mmu, u32 source, u32 dest, u32 control);

s16 arctan2(s16 x, s16 y) noexcept;

struct DivResult {
  s32 div;
  s32 mod;
  s32 abs_div;
};

inline DivResult divide(s32 num, s32 denom) noexcept {
  const s32 res = num / denom;
  return {res, num % denom, std::abs(res)};
}

void lz77_decompress(nonstd::span<const u8> src,
                     nonstd::span<u8> dest,
                     u32 type_size);
}  // namespace hle::bios
}  // namespace gb::advance
