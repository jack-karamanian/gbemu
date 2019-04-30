#pragma once
#include <array>
#include <optional>
#include <type_traits>
#include "types.h"
namespace gb {
template <typename... Args>
[[nodiscard]] constexpr u8 get_bits(Args... args) {
  static_assert((std::is_same_v<Args, bool> && ...));

  u8 res{0x00};
  int shift = sizeof...(args) - 1;

  for (bool set : {args...}) {
    if (set) {
      res |= 0x1 << shift;
    }
    --shift;
  }

  return res;
}

template <typename T>
[[nodiscard]] constexpr T convert_bytes(
    const std::array<u8, sizeof(T)>& bytes) {
  T res{0};

  int shift = (sizeof(T) * 8) - 8;

  for (u8 byte : bytes) {
    res |= (byte << shift);
    shift -= 8;
  }

  return res;
}

template <typename Func, std::size_t... I>
void for_static_impl(Func&& f, std::index_sequence<I...>) {
  (f(std::integral_constant<std::size_t, I>{}), ...);
}

template <std::size_t I, typename Func>
void for_static(Func&& f) {
  for_static_impl(f, std::make_index_sequence<I>());
}

template <typename T>
struct Vec2 {
  T x;
  T y;
};

template <typename T, typename Func>
void visit_optional(std::optional<T>& value, Func&& func) {
  if (value) {
    func(*value);
  }
}

template <typename T, typename Func>
void visit_optional(const std::optional<T>& value, Func&& func) {
  if (value) {
    func(*value);
  }
}

template <typename T>
[[nodiscard]] constexpr bool test_bit(T value, unsigned int bit) {
  return (value & (1 << bit)) != 0;
}

template <typename T, typename U>
[[nodiscard]] constexpr T increment_bits(T value, U mask) {
  return (value & ~mask) | (((value & mask) + 1) & mask);
}

template <typename T>
[[nodiscard]] constexpr T identity(T t) noexcept {
  return t;
}

}  // namespace gb
