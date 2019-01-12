#pragma once
#include <cstdint>
#include <type_traits>
#include <boost/integer.hpp>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

template<typename T>
struct next_largest_type;

template <typename T>
struct next_largest_type {
  using type = typename boost::uint_t<(sizeof(T) * 8) * 2>::least;
};
