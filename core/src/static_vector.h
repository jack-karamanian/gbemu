#pragma once
#include <array>

template <typename T, std::size_t Capacity>
class StaticVector {
 public:
  using iterator = T*;
  using const_iterator = const T*;
  using reference = T&;
  using const_reference = const T&;

  using reverse_iterator = std::reverse_iterator<iterator>;

  constexpr StaticVector() = default;

  [[nodiscard]] constexpr iterator begin() noexcept {
    return std::launder(reinterpret_cast<T*>(&m_data[0]));
  }

  [[nodiscard]] constexpr iterator end() noexcept {
    return std::launder(reinterpret_cast<T*>(&m_data[m_size]));
  }

  [[nodiscard]] constexpr reverse_iterator rbegin() {
    return std::make_reverse_iterator(end());
  }

  [[nodiscard]] constexpr reverse_iterator rend() {
    return std::make_reverse_iterator(begin());
  }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
    return std::launder(reinterpret_cast<const T*>(&m_data[0]));
  }

  [[nodiscard]] constexpr const_iterator cend() const noexcept {
    return std::launder(reinterpret_cast<const T*>(&m_data[m_size]));
  }

  constexpr void push_back(const T& item) {
    if (m_size >= Capacity) {
      throw std::runtime_error("vector full");
    }
    new (&m_data[m_size]) T(item);
    ++m_size;
  }

  constexpr void insert(iterator pos, const T& value) {
    ++m_size;
    T tmp = value;

    while (pos < end()) {
      std::swap(*pos, tmp);
      ++pos;
    }
  }

  [[nodiscard]] constexpr std::size_t size() const noexcept { return m_size; }

  [[nodiscard]] constexpr reference back() {
    return *std::launder(reinterpret_cast<T*>(&m_data[m_size - 1]));
  }
  [[nodiscard]] constexpr const_reference back() const {
    return *std::launder(reinterpret_cast<const T*>(&m_data[m_size - 1]));
  }

  [[nodiscard]] constexpr bool is_empty() const noexcept { return size() == 0; }

  template <typename... Args>
  constexpr void emplace_back(Args&&... args) {
    if (m_size >= Capacity) {
      throw std::runtime_error("no space");
    }
    if constexpr (std::is_aggregate_v<T>) {
      new (&m_data[m_size++]) T{std::forward<Args>(args)...};
    } else {
      new (&m_data[m_size++]) T(std::forward<Args>(args)...);
    }
  }

  constexpr void clear() noexcept { m_size = 0; }

  [[nodiscard]] constexpr std::size_t capacity() const noexcept {
    return Capacity;
  }

  constexpr reference operator[](std::size_t index) {
    return *std::launder(reinterpret_cast<T*>(&m_data[index]));
  }

 private:
  std::size_t m_size = 0;
  std::array<std::aligned_storage_t<sizeof(T), alignof(T)>, Capacity> m_data;
};
