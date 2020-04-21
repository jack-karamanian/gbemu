#pragma once
#include <algorithm>

// Simple versions of the std algorithms for small containers.

namespace gb::algorithm {

template <typename Itr, typename Predicate>
[[nodiscard]] constexpr Itr find_if(Itr begin, Itr end, Predicate predicate) {
  for (auto i = begin; i != end; ++i) {
    if (predicate(*i)) {
      return i;
    }
  }
  return end;
}

template <typename Itr, typename Predicate>
[[nodiscard]] constexpr bool any_of(Itr begin, Itr end, Predicate&& predicate) {
  return find_if(begin, end, std::forward<Predicate>(predicate)) != end;
}

template <typename Itr, typename Predicate>
constexpr void sort(Itr begin, Itr end, Predicate predicate) {
  constexpr auto swap = [](auto a, auto b) {
    auto tmp = *a;
    *a = *b;
    *b = tmp;
  };

  for (auto i = begin; i != end; ++i) {
    swap(i, std::min_element(i, end, predicate));
  }
}

}  // namespace gb::algorithm
