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
[[nodiscard]] nonstd::span<u8> to_bytes(T& val) noexcept {
  return {reinterpret_cast<u8*>(&val), sizeof(T)};
}

template <typename Func, std::size_t... I>
constexpr void for_static_impl(Func&& f, std::index_sequence<I...>) {
  (f(std::integral_constant<std::size_t, I>{}), ...);
}

template <std::size_t I, typename Func>
constexpr void for_static(Func&& f) {
  for_static_impl(std::forward<Func>(f), std::make_index_sequence<I>());
}

template <typename T>
struct Vec2 {
  T x;
  T y;

  constexpr bool operator==(Vec2<T> rhs) const noexcept {
    return x == rhs.x && y == rhs.y;
  }

  constexpr Vec2<T>& operator+=(Vec2<T> rhs) noexcept {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }
};

template <typename T>
struct Rect {
  T width;
  T height;

  template <typename Rhs>
  [[nodiscard]] constexpr Rect operator/(Rhs rhs) const {
    return Rect{width / rhs, height / rhs};
  }
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
[[nodiscard]] constexpr bool test_bit(T value, unsigned int bit) noexcept {
  const T mask = static_cast<T>(1) << bit;
  return (value & mask) != 0;
}

template <typename T, T I>
[[nodiscard]] constexpr bool test_bit(
    [[maybe_unused]] std::integral_constant<T, I> _,
    unsigned int bit) noexcept {
  return test_bit(I, bit);
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
  return static_cast<s32>(val) >> amount;
}

template <int from, int to, typename T>
[[nodiscard]] constexpr T convert_space(T value) {
  return (value * to) / from;
}

template <typename T>
T write_byte(T value, unsigned int byte, u8 byte_value) {
  const unsigned int shift = byte * 8;
  return (value & ~(0xff << shift)) | (byte_value << shift);
}

template <typename Callback>
class ScopeGuard {
 public:
  ScopeGuard(Callback callback) : m_callback{std::move(callback)} {}
  ~ScopeGuard() { m_callback(); }

  ScopeGuard(ScopeGuard<Callback>&&) = delete;
  ScopeGuard<Callback>& operator=(ScopeGuard<Callback>&&) = delete;

  ScopeGuard(const ScopeGuard<Callback>&) = delete;
  ScopeGuard<Callback>& operator=(const ScopeGuard<Callback>&) = delete;

 private:
  Callback m_callback;
};

template <typename T>
class Integer {
  static_assert(std::is_integral_v<T>);

 public:
  using Type = T;
  static constexpr std::size_t Size = sizeof(T);

  constexpr explicit Integer(T value_) : m_value{value_} {}

  [[nodiscard]] constexpr u8 read_byte(unsigned int byte) const {
    const u32 shift = byte * 8;
    return (m_value & (0xff << shift)) >> shift;
  }

  constexpr void write_byte(unsigned int byte, u8 value) {
    // m_value = (m_value & ~(0xff << shift)) | (value << shift);
    m_value = gb::write_byte(m_value, byte, value);
  }

  constexpr void on_after_write() const {}

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

template <typename... Funcs>
struct Overloaded : Funcs... {
  Overloaded(Funcs&&... funcs) : Funcs(std::forward<Funcs>(funcs))... {}

  using Funcs::operator()...;
};

[[nodiscard]] constexpr auto operator"" _kb(
    unsigned long long int kilobytes) noexcept {
  return kilobytes * 1024;
}

template <typename F>
class FunctionRef;

template <typename Ret, typename... Args>
class FunctionRef<Ret(Args...)> {
 public:
  template <typename F>
  FunctionRef(F&& func) noexcept : m_impl{&func} {
    m_func_ptr = [](void* impl, Args... args) -> Ret {
      return (*reinterpret_cast<std::add_pointer_t<F>>(impl))(
          std::forward<Args>(args)...);
    };
  }

  Ret operator()(Args... args) const {
    return m_func_ptr(m_impl, std::forward<Args>(args)...);
  }

 private:
  using FuncPtr = Ret (*)(void*, Args...);
  void* m_impl;
  FuncPtr m_func_ptr;
};

template <typename T, typename = void>
struct IsInteger : std::false_type {};

template <typename T>
struct IsInteger<T,
                 std::void_t<decltype(std::declval<T>().byte_span()),
                             decltype(std::declval<T>().write_byte(0, 0)),
                             decltype(std::declval<T>().size_bytes())>>
    : std::true_type {};

class IntegerRef {
 public:
  template <
      typename T,
      typename = std::enable_if_t<!std::is_same_v<IntegerRef, std::decay_t<T>>>>
  IntegerRef(T& integer) noexcept : m_ptr{static_cast<void*>(&integer)} {
    using DecayedT = std::decay_t<T>;

    using IsIntegerT = IsInteger<DecayedT>;

    if constexpr (IsIntegerT::value) {
      m_byte_span = integer.byte_span();
      m_size_bytes = integer.size_bytes();
    } else {
      m_byte_span = to_bytes(integer);
      m_size_bytes = sizeof(DecayedT);
    }

    m_write_byte = [](void* self, unsigned int offset,
                      nonstd::span<const u8> bytes) {
      constexpr bool is_integer = IsIntegerT::value;
      auto* integer_self = reinterpret_cast<DecayedT*>(self);

      const auto size = bytes.size() & 0x7;

      for (long i = 0; i < size; ++i) {
        if constexpr (is_integer) {
          integer_self->write_byte(offset + i, bytes[i]);
        } else {
          static_assert(std::is_integral_v<DecayedT>);
          static_assert(!std::is_pointer_v<DecayedT>);
          *integer_self = gb::write_byte(*integer_self, offset + i, bytes[i]);
        }
      }
      if constexpr (is_integer) {
        integer_self->on_after_write();
      }
    };
  }

  void write_byte(unsigned int offset, nonstd::span<const u8> bytes) const {
    m_write_byte(m_ptr, offset, bytes);
  }

  [[nodiscard]] nonstd::span<u8> byte_span() const noexcept {
    return m_byte_span;
  }

  [[nodiscard]] std::size_t size_bytes() const noexcept { return m_size_bytes; }

 private:
  using WriteBytePtr = void (*)(void*, unsigned int, nonstd::span<const u8>);
  WriteBytePtr m_write_byte;
  void* m_ptr;
  std::size_t m_size_bytes;
  nonstd::span<u8> m_byte_span;
};

template <typename Container, typename Func>
constexpr void constexpr_for_each(Container& container, Func&& func) {
  for (auto& element : container) {
    func(element);
  }
}

template <typename Container, typename T>
constexpr void constexpr_fill(Container& container, const T& value) {
  for (auto& element : container) {
    element = value;
  }
}

}  // namespace gb
