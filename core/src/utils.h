#pragma once
#include <array>
#include <nonstd/span.hpp>
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
    nonstd::span<const u8, sizeof(T)> bytes) {
  T res{0};

  int shift = (sizeof(T) * 8) - 8;

  for (u8 byte : bytes) {
    res |= (byte << shift);
    shift -= 8;
  }

  return res;
}

template <typename T>
[[nodiscard]] constexpr T convert_bytes_endian(
    nonstd::span<const u8, sizeof(T)> bytes) {
  T res{0};
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  for (int i = sizeof(T) - 1; i >= 0; --i) {
    res |= bytes[i] << (i * 8);
  }
#else
#error "Big endian is not implemented"
#endif
  return res;
}

template <typename T>
[[nodiscard]] constexpr std::array<u8, sizeof(T)> to_bytes(T val) {
  std::array<u8, sizeof(T)> res;

  for (std::size_t i = 0; i < sizeof(T); ++i) {
    const std::size_t shift = i * 8;
    res[i] = (val & (0xff << shift)) >> shift;
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

template <std::size_t Offset, std::size_t... I>
constexpr auto tuple_from_range_impl(std::index_sequence<I...>) {
  return std::make_tuple(std::integral_constant<std::size_t, Offset + I>{}...);
}
template <std::size_t Begin, std::size_t End>
constexpr auto tuple_from_range() {
  return tuple_from_range_impl<Begin>(
      std::make_index_sequence<End - Begin + 1>());
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

template <typename T, typename... Args>
[[nodiscard]] constexpr T set_bits(const Args... bits) {
  return ((1 << bits) | ...);
}

template <typename Itr, typename Func>
[[nodiscard]] constexpr Itr constexpr_find(Itr begin, Itr end, Func func) {
  for (; begin != end; ++begin) {
    if (func(*begin)) {
      return begin;
    }
  }
  return end;
}

[[nodiscard]] constexpr u32 rotate_right(u32 val, u32 amount) {
  constexpr std::size_t end_shift = sizeof(val) * 8 - 1;

  for (unsigned i = 0; i < amount; ++i) {
    val = ((val & 1) << end_shift) | (val >> 1);
  }

  return val;
}

[[nodiscard]] constexpr u32 arithmetic_shift_right(u32 val, u32 amount) {
  constexpr std::size_t end_shift = sizeof(val) * 8 - 1;
  return (val & (1 << end_shift)) | (val >> amount);
}

template <int from, int to, typename T>
[[nodiscard]] constexpr T convert_space(T value) {
  return (value * to) / from;
}

template <typename T>
class Integer {
  static_assert(std::is_integral_v<T>);

 public:
  static constexpr Integer<T> from_bytes(
      nonstd::span<const u8, sizeof(T)> bytes) {
    return Integer<T>{convert_bytes_endian<T>(bytes)};
  }

  constexpr explicit Integer(T value_) : m_value{value_} {}

  [[nodiscard]] constexpr u8 read_byte(unsigned int byte) const {
    const u32 shift = byte * 8;
    return (m_value & (0xff << shift)) >> shift;
  }

  constexpr void write_byte(unsigned int byte, u8 value) {
    const u32 shift = byte * 8;
    m_value = (m_value & ~(0xff << shift)) | (value << shift);
  }

  [[nodiscard]] constexpr std::size_t size_bytes() const { return sizeof(T); }

  [[nodiscard]] constexpr T data() const { return m_value; }

  constexpr void set_data(T value) { m_value = value; }

  [[nodiscard]] nonstd::span<u8> byte_span() {
    return {reinterpret_cast<u8*>(&m_value), sizeof(T)};
  }

  [[nodiscard]] constexpr bool test_bit(unsigned int bit) const {
    return gb::test_bit(m_value, bit);
  }

  constexpr void set_bit(unsigned int bit, bool set) {
    const T mask = static_cast<T>(1) << bit;
    m_value = (m_value & ~mask) | (set ? mask : 0);
  }

 protected:
  T m_value;
};
template <typename Itr, typename Func>
constexpr void constexpr_sort(Itr begin, Itr end, Func func) {
  constexpr auto swap = [](auto a, auto b) {
    auto tmp = *a;
    *a = *b;
    *b = tmp;
  };

  for (auto i = begin; i != end; ++i) {
    swap(i, std::min_element(i, end, func));
  }
}
};

}  // namespace gb
