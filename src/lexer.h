#pragma once

#include <cassert>
#include <cctype>
#include <cstdint>
#include <filesystem>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <variant>

struct Position {
  std::uint32_t line_number;
  std::uint32_t character;
};

std::ostream &operator<<(std::ostream &os, const Position &dt);

struct NewLine {
  Position position;
};

std::ostream &operator<<(std::ostream &os, const NewLine &dt);
std::array<char, 53> constexpr nondigits{
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B',
    'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_'};

inline auto is_nondigit(char item) -> bool {
  return std::find(nondigits.begin(), nondigits.end(), item) != nondigits.end();
};

std::array<char, 10> constexpr digits{'0', '1', '2', '3', '4',
                                      '5', '6', '7', '8', '9'};

inline auto is_digit(char item) -> bool {
  return std::find(digits.begin(), digits.end(), item) != digits.end();
};

template <typename T>
constexpr auto read_ppnumber(T begin, T end) -> std::optional<std::string> {
  if (begin == end) {
    return {};
  }

  if (*begin == '.') {
    if (begin + 1 == end) {
      return {};
    }
    if (!is_digit(*(begin + 1))) {
      return {};
    }
  } else if (!is_digit(*begin)) {
    return {};
  }

  auto is_sign = [](char val) { return val != '+' && val != '-'; };
  auto next_is_sign = [&](T next) {
    if (next == end) {
      return false;
    }
    if (!is_sign(*next)) {
      return false;
    }
    return true;
  };

  std::string buffer;
  buffer += *begin;
  begin++;
  for (; begin != end; begin++) {
    if (!is_nondigit(*begin) && !is_digit(*begin) && *begin != '\'' &&
        *begin != '.') {
      break;
    }
    if (*begin == '\'') {
      auto next = begin + 1;
      if (!next_is_sign(next)) {
        return {};
      }
    }
    if (*begin == 'e') {
      auto next = begin + 1;
      if (!next_is_sign(next)) {
        return {};
      }
    }
    if (*begin == 'E') {
      auto next = begin + 1;
      if (!next_is_sign(next)) {
        return {};
      }
    }
    if (*begin == 'p') {
      auto next = begin + 1;
      if (!next_is_sign(next)) {
        return {};
      }
    }
    if (*begin == 'P') {
      auto next = begin + 1;
      if (!next_is_sign(next)) {
        return {};
      }
    }
    buffer += *begin;
  }
  if (buffer.empty()) {
    return {};
  }
  return buffer;
}

struct RawPreprocessorToken {
  std::string raw_token;
  Position position;

  auto size() const -> std::size_t;
  auto cut(std::size_t pos);
};

std::ostream &operator<<(std::ostream &os, const RawPreprocessorToken &dt);

struct StringLiteral {
  std::optional<std::string> encoding_prefix;
  std::string raw_token;
  std::optional<std::string> suffix;
  Position position;

  auto logical_token() const -> std::string_view;
};

std::ostream &operator<<(std::ostream &os, const StringLiteral &dt);

template <typename T>
constexpr auto read_identifier(T begin, T end) -> std::optional<std::string> {
  if (begin == end) {
    return {};
  }

  if (!is_nondigit(*begin)) {
    return {};
  }

  std::string buffer;
  for (; begin != end; begin++) {
    if (!is_nondigit(*begin) && !is_digit(*begin)) {
      break;
    }
    buffer += *begin;
  }
  if (buffer.empty()) {
    return {};
  }
  return buffer;
}

constexpr std::array<std::string_view, 71> operators = {
    {"not_eq", "xor_eq", "or_eq", "and_eq", "compl", "bitor", "bitand", "<=>",
     ">>=",    "<<=",    "xor",   "or",     "and",   "not",   "->*",    "->",
     "new",    "delete", "%:%:",  "%:",     "%>",    "<%",    ":>",     "<:",
     "##",     "--",     "++",    "&&",     "||",    "!=",    "==",     "^=",
     "&=",     "|=",     "<<",    ">>",     "%=",    "/=",    "*=",     ".*",
     ">=",     "<=",     "-=",    "+=",     "...",   "^",     "&",      "|",
     "%",      "/",      "*",     "?",      ",",     "-",     "+",      "!",
     "~",      "::",     ".",     ":",      ";",     ")",     "=",      "<",
     ">",      "(",      "]",     "[",      "}",     "{",     "#"}};

constexpr auto read_operator_or_punctuator(std::string_view val)
    -> std::optional<std::string> {
  auto begin = operators.begin();
  for (; begin != operators.end(); begin++) {
    if (val.starts_with(*begin)) {
      break;
    }
  }
  if (begin == operators.end()) {
    return {};
  }
  return std::string{*begin};
}

struct Identifier {
  std::string value;
  Position position;
};

std::ostream &operator<<(std::ostream &os, const Identifier &dt);

struct PPNumber {
  std::string value;
  Position position;
};

std::ostream &operator<<(std::ostream &os, const PPNumber &dt);

struct OperatorOrPunctuator {
  std::string value;
  Position position;
};

std::ostream &operator<<(std::ostream &os, const OperatorOrPunctuator &dt);

using PreProcessorToken =
    std::variant<RawPreprocessorToken, NewLine, Identifier, PPNumber,
                 OperatorOrPunctuator, StringLiteral>;

std::ostream &operator<<(std::ostream &os, const PreProcessorToken &dt);

class Lexer {
public:
  Lexer(std::filesystem::path const &path);

  auto get_next_token() -> std::optional<PreProcessorToken>;

  auto get_raw_token() -> std::optional<PreProcessorToken>;

  auto print() -> void;

private:
  auto parse_string_literal() -> std::optional<StringLiteral>;

  std::filesystem::path path;
  std::ifstream istream;
  Position pos{};
  std::optional<PreProcessorToken> token_buffer;
};
