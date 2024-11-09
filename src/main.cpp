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

template <typename T, typename Allocator = std::allocator<T>> class RingBuffer {
public:
  class BufferIterator {
  public:
    BufferIterator(RingBuffer const &buffer, bool end = false)
        : index(buffer.m_read_head), buffer(buffer), end(end) {}

    auto operator==(BufferIterator const &other) const -> bool {
      check(&this->buffer == &other.buffer,
            "Iterator does not point to same RingBuffer<T>");
      if (this->end || other.end) {
        return (this->index) % buffer.capacity() == (buffer.m_write_head);
      }
      return other.index == this->index;
    }

    auto operator!=(BufferIterator other) const -> bool {
      return !(*this == other);
    }

    BufferIterator &operator++() {
      check(*this != buffer.end(), "Cannot iterate past end");
      index++;
      return *this;
    }

    BufferIterator operator++(int) {
      BufferIterator retval = *this;
      ++(*this);
      return retval;
    }

    auto operator*() const -> T & {
      check(*this != buffer.end(), "Index out of range");
      return this->buffer.m_data[index];
    }

    using difference_type = std::size_t;
    using value_type = char;
    using reference = char;
    using iterator_category = std::forward_iterator_tag;

  private:
    RingBuffer const &buffer;
    bool end;
    std::size_t index{};
  };

  RingBuffer() : m_size(0), m_capacity(4), m_read_head(0), m_write_head(0) {
    m_data = alloc.allocate(m_capacity);
  };

  ~RingBuffer() {
    for (auto i = 0; i < m_size; i++) {
      std::destroy_at(m_data + m_read_head);
      m_read_head += 1;
    }
    alloc.deallocate(m_data, m_capacity);
  }

  auto push_back(T &&value) -> void { emplace_back(std::move(value)); }

  template <typename... Args>
    requires(std::is_constructible_v<T, Args...>)
  auto emplace_back(Args &&...args) -> void {
    if (m_size == (m_capacity - 1)) {
      resize(m_capacity * 2);
    }
    std::construct_at<T>(m_data + m_write_head, args...);
    m_write_head = (m_write_head + 1) % m_capacity;
    m_size += 1;
  }

  [[nodiscard]] auto front() -> T & { return *begin(); }

  auto pop_front() -> void {
    check(m_size > 0, "Buffer is empty");
    std::destroy_at(m_data + m_read_head);
    m_read_head = (m_read_head + 1) % m_capacity;
    m_size -= 1;
  }

  auto drop(std::size_t num) -> void {
    check(num <= m_size, "Cannot drop more items than we have");
    for (auto i = 0; i < num; i++) {
      std::destroy_at(m_data + m_read_head);
      m_read_head += 1;
    }
    m_size -= num;
  }

  auto drop(BufferIterator end) -> void {
    auto begin = this->begin();
    for (; begin != end; begin++) {
      std::destroy_at(&*begin);
      m_read_head += 1;
      m_size -= 1;
    }
  }

  auto size() const -> std::size_t { return m_size; }
  auto capacity() const -> std::size_t { return m_capacity; }

  auto resize(std::size_t size) -> void {
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

  auto operator[](std::size_t index) const -> T & {
    check(index < m_size, "Index out of bounds");
    return m_data[(m_read_head + index) % m_capacity];
  }

  [[nodiscard]] auto begin() const -> BufferIterator {
    return BufferIterator{*this};
  }
  [[nodiscard]] auto end() const -> BufferIterator {
    return BufferIterator{*this, true};
  }

private:
  Allocator alloc;
  std::size_t m_size;
  std::size_t m_capacity;
  std::size_t m_read_head;
  std::size_t m_write_head;
  T *m_data;
  friend BufferIterator;
};

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
