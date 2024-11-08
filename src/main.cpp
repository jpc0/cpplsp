#include <args.h>
#include <bit>
#include <cassert>
#include <exception>
#include <iostream>
#include <iterator>
#include <lexer.h>
#include <source_location>

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

class CharRingBuffer {
public:
  class BufferIterator {
  public:
    BufferIterator(CharRingBuffer const &, bool = false);

    auto operator==(BufferIterator const &) const -> bool;
    auto operator!=(BufferIterator) const -> bool;

    BufferIterator &operator++();
    BufferIterator operator++(int);
    auto operator*() const -> char;

    using difference_type = std::size_t;
    using value_type = char;
    using reference = char;
    using iterator_category = std::forward_iterator_tag;

  private:
    CharRingBuffer const &buffer;
    bool end;
    std::size_t index{};
  };

  CharRingBuffer() : m_size(0), m_capacity(4), m_read_head(0), m_write_head(0) {
    m_data = new char[m_capacity];
  };

  ~CharRingBuffer() { delete[] m_data; }

  auto resize(std::size_t size) -> void {
    check(std::bit_ceil(size) >= m_capacity,
          "New capacity cannot be smaller than current capacity");
    if (m_size == 0) {
      delete[] m_data;
      m_capacity = std::bit_ceil(size);
      m_data = new char[m_capacity];
      return;
    }
    auto new_capacity = std::bit_ceil(size);
    auto *new_data = new char[new_capacity];
    std::size_t j = 0;
    for (auto i = m_read_head; i < m_write_head; i = (i + 1) % m_capacity) {
      new_data[j] = m_data[i];
      j++;
    }
    delete[] m_data;
    m_data = new_data;
    m_capacity = new_capacity;
    m_read_head = 0;
    m_write_head = m_size;
  }

  auto operator[](std::size_t index) const -> char {
    check(index < m_size, "Index out of bounds");
    return m_data[(m_read_head + index) % m_capacity];
  }

  auto push_back(char value) -> void {
    if (m_size == (m_capacity - 1)) {
      resize(m_capacity * 2);
    }
    m_data[m_write_head] = value;
    m_write_head = (m_write_head + 1) % m_capacity;
    m_size += 1;
  }

  auto pop_back() -> char {
    check(m_size > 0, "Buffer is empty");
    if (m_write_head == 0) {
      auto out = m_data[m_capacity - 1];
      m_write_head = m_capacity - 1;
      m_size -= 1;
      return out;
    }
    auto out = m_data[m_write_head - 1];
    m_write_head = m_write_head - 1;
    m_size -= 1;
    return out;
  }

  auto pop_front() -> char {
    check(m_size > 0, "Buffer is empty");
    auto out = m_data[m_read_head];
    m_read_head = (m_read_head + 1) % m_capacity;
    m_size -= 1;
    return out;
  }

  auto drop(std::size_t num) -> void {
    check(num <= m_size, "Cannot drop more items than we have");
    m_read_head += num;
    m_size -= num;
  }

  auto size() const -> std::size_t { return m_size; }
  auto capacity() const -> std::size_t { return m_capacity; }

  auto begin() const -> BufferIterator { return BufferIterator{*this}; }
  auto end() const -> BufferIterator { return BufferIterator{*this, true}; }

private:
  std::size_t m_size;
  std::size_t m_capacity;
  std::size_t m_read_head;
  std::size_t m_write_head;
  char *m_data;
  friend BufferIterator;
};

CharRingBuffer::BufferIterator::BufferIterator(CharRingBuffer const &buffer,
                                               bool end)
    : buffer(buffer), end(end) {}

bool CharRingBuffer::BufferIterator::operator==(
    BufferIterator const &other) const {
  check(&this->buffer == &other.buffer,
        "Iterator does not point to same CharRingBuffer");
  if (this->end || other.end) {
    return this->index == buffer.size();
  }
  return other.index == this->index;
};

bool CharRingBuffer::BufferIterator::operator!=(BufferIterator other) const {
  return !(*this == other);
}

CharRingBuffer::BufferIterator &CharRingBuffer::BufferIterator::operator++() {
  index++;
  return *this;
}

CharRingBuffer::BufferIterator CharRingBuffer::BufferIterator::operator++(int) {
  BufferIterator retval = *this;
  ++(*this);
  return retval;
}

char CharRingBuffer::BufferIterator::operator*() const {
  check((index <= this->buffer.size()), "Index out of range");
  return this->buffer[index];
}

std::ostream &operator<<(std::ostream &os, const CharRingBuffer &dt) {
  for (auto i : dt) {
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
