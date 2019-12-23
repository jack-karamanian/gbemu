#pragma once
#include <doctest/doctest.h>
#include <array>
#include <cstddef>

template <typename T, std::size_t Capacity>
class RingBuffer {
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");

 public:
  static constexpr std::size_t CapacityMask = Capacity - 1;

  void push_back(const T& item) {
    if (m_size >= Capacity) {
      throw std::runtime_error("buffer full");
    }
    const auto next_index = (m_end + 1) & CapacityMask;
    ++m_size;
    m_data[m_end] = item;
    m_end = next_index;
  }

  T next() {
    if (m_size > 0) {
      --m_size;
      const auto index = m_begin;
      m_begin = (m_begin + 1) & CapacityMask;
      return m_data[index];
    }
    throw std::runtime_error("no data");
  }

  [[nodiscard]] std::size_t size() const noexcept { return m_size; }

  [[nodiscard]] std::size_t capacity() const noexcept { return Capacity; }

  void clear() noexcept {
    m_size = 0;
    m_begin = 0;
    m_end = 0;
  }

 private:
  std::array<T, Capacity> m_data;
  std::size_t m_size = 0;

  std::size_t m_begin = 0;
  std::size_t m_end = 0;
};

TEST_CASE("RingBuffer should read back elements correctly") {
  RingBuffer<int, 16> buffer;

  for (int i = 0; i < 8; ++i) {
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    buffer.push_back(4);
    CHECK(buffer.size() == 4);

    CHECK(buffer.next() == 1);
    CHECK(buffer.next() == 2);
    CHECK(buffer.next() == 3);
    CHECK(buffer.next() == 4);

    CHECK(buffer.size() == 0);
  }
}
