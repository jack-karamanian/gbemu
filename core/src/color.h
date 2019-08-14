#pragma once
#include "types.h"
namespace gb {
struct Color {
  u8 r = 0;
  u8 g = 0;
  u8 b = 0;
  u8 a = 0;

  constexpr Color(u8 r_, u8 g_, u8 b_, u8 a_) : r{r_}, g{g_}, b{b_}, a{a_} {}
  constexpr Color(u8 r_, u8 g_, u8 b_) : r{r_}, g{g_}, b{b_} {}
  constexpr Color() = default;
};
}  // namespace gb
