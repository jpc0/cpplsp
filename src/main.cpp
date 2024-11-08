#include <args.h>
#include <bit>
#include <cassert>
#include <exception>
#include <iostream>
#include <iterator>
#include <lexer.h>
#include <memory>
#include <source_location>
#include <type_traits>

constexpr auto check(bool condition, char const *message,
                     std::source_location source_location =
                         std::source_location::current()) -> void {
  if (!condition) {
    std::cerr << '\n'
              << source_location.file_name() << '(' << source_location.line()
              << ':' << source_location.column() << ')' << " Check failed in "
              << source_location.function_name() << ": " << message << '\n';
    std::terminate();
  }
}

template <typename T> class RingBuffer {
public:
  class BufferIterator {
  public:
    BufferIterator(RingBuffer const &, bool = false);

    auto operator==(BufferIterator const &) const -> bool;
    auto operator!=(BufferIterator) const -> bool;

    BufferIterator &operator++();
    BufferIterator operator++(int);
    auto operator*() const -> T &;

    using difference_type = std::size_t;
    using value_type = char;
    using reference = char;
    using iterator_category = std::forward_iterator_tag;

  private:
    RingBuffer const &buffer;
    bool end;
    std::size_t index{};
  };

  RingBuffer();

  ~RingBuffer();

  auto push_back(T &&value) -> void;
  template <typename... Args>
    requires(std::is_constructible_v<T, Args...>)
  auto emplace_back(Args &&...args) -> void;

  auto pop_back() -> T;
  auto pop_front() -> T;

  auto drop(std::size_t num) -> void;

  auto size() const -> std::size_t;
  auto capacity() const -> std::size_t;
  auto resize(std::size_t size) -> void;
  auto operator[](std::size_t index) const -> T &;

  auto begin() const -> BufferIterator;
  auto end() const -> BufferIterator;

private:
  std::allocator<T> alloc;
  std::size_t m_size;
  std::size_t m_capacity;
  std::size_t m_read_head;
  std::size_t m_write_head;
  T *m_data;
  friend BufferIterator;
};

template <typename T>
RingBuffer<T>::RingBuffer()
    : m_size(0), m_capacity(4), m_read_head(0), m_write_head(0) {
  m_data = alloc.allocate(m_capacity);
};

template <typename T> RingBuffer<T>::~RingBuffer() {
  for (auto i = 0; i < m_size; i++) {
    std::destroy_at(m_data + m_read_head);
    m_read_head += 1;
  }
  alloc.deallocate(m_data, m_capacity);
}

template <typename T> auto RingBuffer<T>::resize(std::size_t size) -> void {
  check(std::bit_ceil(size) >= m_capacity,
        "New capacity cannot be smaller than current capacity");
  if (m_size == 0) {
    alloc.deallocate(m_data, m_capacity);
    m_capacity = std::bit_ceil(size);
    m_data = alloc.allocate(m_capacity);
    return;
  }
  auto new_capacity = std::bit_ceil(size);
  T *new_data = alloc.allocate(new_capacity);
  std::size_t j = 0;
  for (auto i = m_read_head; i < m_write_head; i = (i + 1) % m_capacity) {
    if constexpr (std::is_move_assignable_v<T>) {
      std::construct_at(new_data + j, std::move(m_data[i]));
    } else {
      std::construct_at(new_data + j, m_data[i]);
    }
    std::destroy_at(m_data + i);
    j++;
  }
  alloc.deallocate(m_data, m_capacity);
  m_data = new_data;
  m_capacity = new_capacity;
  m_read_head = 0;
  m_write_head = m_size;
}

template <typename T>
auto RingBuffer<T>::operator[](std::size_t index) const -> T & {
  check(index < m_size, "Index out of bounds");
  return m_data[(m_read_head + index) % m_capacity];
}

template <typename T> auto RingBuffer<T>::push_back(T &&value) -> void {
  if (m_size == (m_capacity - 1)) {
    resize(m_capacity * 2);
  }
  std::construct_at(m_data + m_write_head, std::forward<T>(value));
  m_write_head = (m_write_head + 1) % m_capacity;
  m_size += 1;
}

template <typename T>
template <typename... Args>
  requires(std::is_constructible_v<T, Args...>)
auto RingBuffer<T>::emplace_back(Args &&...args) -> void {
  if (m_size == (m_capacity - 1)) {
    resize(m_capacity * 2);
  }
  std::construct_at<T>(m_data + m_write_head, args...);
  m_write_head = (m_write_head + 1) % m_capacity;
  m_size += 1;
}

template <typename T> auto RingBuffer<T>::pop_back() -> T {
  check(m_size > 0, "Buffer is empty");
  if constexpr (std::is_move_assignable_v<T>) {
    if (m_write_head == 0) {
      auto out = std::move(m_data[m_capacity - 1]);
      std::destroy_at(m_data + (m_capacity - 1));
      m_write_head = m_capacity - 1;
      m_size -= 1;
      return out;
    }
    auto out = std::move(m_data[m_write_head - 1]);
    std::destroy_at(m_data + (m_write_head - 1));
    m_write_head = m_write_head - 1;
    m_size -= 1;
    return out;
  }
  if (m_write_head == 0) {
    auto out = m_data[m_capacity - 1];
    std::destroy_at(m_data + (m_capacity - 1));
    m_write_head = m_capacity - 1;
    m_size -= 1;
    return out;
  }
  auto out = m_data[m_write_head - 1];
  std::destroy_at(m_data + (m_write_head - 1));
  m_write_head = m_write_head - 1;
  m_size -= 1;
  return out;
}

template <typename T> auto RingBuffer<T>::pop_front() -> T {
  check(m_size > 0, "Buffer is empty");
  if constexpr (std::is_move_assignable_v<T>) {
    auto out = std::move(m_data[m_read_head]);
    std::destroy_at(m_data + m_read_head);
    m_read_head = (m_read_head + 1) % m_capacity;
    m_size -= 1;
    return out;
  }
  auto out = m_data[m_read_head];
  std::destroy_at(m_data + m_read_head);
  m_read_head = (m_read_head + 1) % m_capacity;
  m_size -= 1;
  return out;
}

template <typename T> auto RingBuffer<T>::drop(std::size_t num) -> void {
  check(num <= m_size, "Cannot drop more items than we have");
  for (auto i = 0; i < num; i++) {
    std::destroy_at(m_data + m_read_head);
    m_read_head += 1;
  }
  m_size -= num;
}

template <typename T> auto RingBuffer<T>::size() const -> std::size_t {
  return m_size;
}

template <typename T> auto RingBuffer<T>::capacity() const -> std::size_t {
  return m_capacity;
}

template <typename T> auto RingBuffer<T>::begin() const -> BufferIterator {
  return BufferIterator{*this};
}

template <typename T> auto RingBuffer<T>::end() const -> BufferIterator {
  return BufferIterator{*this, true};
}

template <typename T>
RingBuffer<T>::BufferIterator::BufferIterator(RingBuffer const &buffer,
                                              bool end)
    : buffer(buffer), end(end) {}

template <typename T>
bool RingBuffer<T>::BufferIterator::operator==(
    BufferIterator const &other) const {
  check(&this->buffer == &other.buffer,
        "Iterator does not point to same CharRingBuffer");
  if (this->end || other.end) {
    return this->index == buffer.size();
  }
  return other.index == this->index;
};

template <typename T>
bool RingBuffer<T>::BufferIterator::operator!=(BufferIterator other) const {
  return !(*this == other);
}

template <typename T>
RingBuffer<T>::BufferIterator &RingBuffer<T>::BufferIterator::operator++() {
  index++;
  return *this;
}

template <typename T>
RingBuffer<T>::BufferIterator RingBuffer<T>::BufferIterator::operator++(int) {
  BufferIterator retval = *this;
  ++(*this);
  return retval;
}

template <typename T>
auto RingBuffer<T>::BufferIterator::operator*() const -> T & {
  check((index <= this->buffer.size()), "Index out of range");
  return this->buffer[index];
}

template <typename T>
std::ostream &operator<<(std::ostream &os, const RingBuffer<T> &dt) {
  for (auto &i : dt) {
    os << i;
  }
  return os;
}

int main(int argc, char **argv) {
  Args args{argc, argv};
  if (args.size() < 2) {
    std::cerr << "You must supply a file" << '\n';
    return EXIT_FAILURE;
  }

  Lexer lex(args[1]);
  lex.print();
  auto token = lex.get_next_token();
  while (token.has_value()) {
    std::cout << *token << '\n';
    token = lex.get_next_token();
  }

  return EXIT_SUCCESS;
};
