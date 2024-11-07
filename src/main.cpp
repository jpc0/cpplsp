#include <args.h>
#include <bit>
#include <cassert>
#include <exception>
#include <iostream>
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

  auto size() const -> std::size_t { return m_size; }
  auto capacity() const -> std::size_t { return m_capacity; }

private:
  std::size_t m_size;
  std::size_t m_capacity;
  std::size_t m_read_head;
  std::size_t m_write_head;
  char *m_data;
};

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
