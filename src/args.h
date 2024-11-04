#pragma once

#include <cassert>
#include <string_view>

class Args {
public:
  class ArgsIterator {
  public:
    ArgsIterator(size_t size, char const **data, size_t index = 0);

    bool operator==(ArgsIterator const &other) const;
    bool operator!=(ArgsIterator other) const;

    ArgsIterator &operator++();
    ArgsIterator operator++(int);
    std::string_view operator*() const;
    using difference_type = std::string_view;
    using value_type = std::string_view;
    using pointer = std::string_view;
    using reference = std::string_view;
    using iterator_category = std::forward_iterator_tag;

  private:
    std::size_t index;
    std::size_t size;
    char const **data;
  };

  explicit Args(int argc, char **argv);

  auto operator[](std::size_t index) -> std::string_view;
  auto size() -> std::size_t;
  ArgsIterator begin() const;
  ArgsIterator end() const;

private:
  std::size_t m_size;
  char const **m_data;
};
