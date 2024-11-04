#include "args.h"

#include <exception>

Args::ArgsIterator::ArgsIterator(size_t size, char const **data, size_t index)
    : size(size), data(data), index(index) {}

bool Args::ArgsIterator::operator==(ArgsIterator const &other) const {
  return other.index == this->index;
};

bool Args::ArgsIterator::operator!=(ArgsIterator other) const {
  return !(*this == other);
}

Args::ArgsIterator &Args::ArgsIterator::operator++() {
  index++;
  return *this;
}

Args::ArgsIterator Args::ArgsIterator::operator++(int) {
  ArgsIterator retval = *this;
  ++(*this);
  return retval;
}

std::string_view Args::ArgsIterator::operator*() const {
  assert(!(index >= size) || !(index > 0) && "Index out of range");
  if (index >= size || index < 0) {
    std::terminate();
  }
  return data[index];
}

Args::Args(int argc, char **argv)
    : m_size(argc), m_data(const_cast<char const **>(argv)){};

auto Args::operator[](std::size_t index) -> std::string_view {
  if (index >= m_size) {
    std::terminate();
  }
  return m_data[index];
};

auto Args::size() -> std::size_t { return m_size; };
Args::ArgsIterator Args::begin() const { return {m_size, m_data}; }
Args::ArgsIterator Args::end() const { return {m_size, m_data, m_size}; }
