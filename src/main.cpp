#include <args.h>

#include <cassert>
#include <cctype>
#include <cstdint>

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

struct RawPreprocessorToken {
  std::string raw_token;
  std::string logical_token;
  Position position;
};

std::ostream &operator<<(std::ostream &os, const RawPreprocessorToken &dt) {
  os << "RawToken(" << dt.position.line_number << ":" << dt.position.character
     << ")\t\"" << dt.logical_token << "\"";
  return os;
}

struct NewLine {
  Position position;
};

std::ostream &operator<<(std::ostream &os, const NewLine &dt) {
  os << "NewLine(" << dt.position.line_number << ":" << dt.position.character
     << ")";
  return os;
}

using PreProcessorToken = std::variant<RawPreprocessorToken, NewLine>;

std::ostream &operator<<(std::ostream &os, const PreProcessorToken &dt) {
  std::visit([&](auto &&arg) { os << arg; }, dt);
  return os;
}

class Lexer {
public:
  Lexer(std::filesystem::path const &path) : path(path), istream(path){};

  auto get_next_token() -> std::optional<PreProcessorToken> {
    char c;
    std::string buffer;
    std::string raw_buffer;
    auto ret_pos = pos;
    while (istream.get(c)) {
      if (c == '\\') {
        char peek;
        if (istream.get(peek)) {
          if (peek == '\n') {
            raw_buffer += c;
            raw_buffer += peek;
            pos.character = 0;
            pos.line_number += 1;
            continue;
          }
          istream.putback(peek);
        }
      }
      if (std::isspace(c)) {
        if (c == '\n' && buffer.empty() && raw_buffer.empty()) {
          pos.character = 0;
          pos.line_number += 1;
          return NewLine{ret_pos};
        } else if (c == '\n') {
          istream.putback(c);
          break;
        } else {
          pos.character += 1;
          raw_buffer += c;
          char peek;
          if (istream.get(peek)) {
            if (std::isspace(peek)) {
              istream.putback(peek);
              continue;
            }
            istream.putback(peek);
          }
          break;
        }
      }
      buffer += c;
      raw_buffer += c;
      pos.character += 1;
    }
    if (buffer.empty() && raw_buffer.empty()) {
      return {};
    }
    return RawPreprocessorToken{raw_buffer, buffer, ret_pos};
  };

  auto print() -> void {
    char c;
    while (istream.get(c)) {
      std::cout << c;
    }
    istream.seekg(0);
  };

private:
  std::filesystem::path path;
  std::ifstream istream;
  Position pos{};
};

int main(int argc, char **argv) {
  Args args{argc, argv};
  if (args.size() < 2) {
    std::cerr << "You must supply a file" << '\n';
    return EXIT_FAILURE;
  }

  Lexer lex(args[1]);
  auto token = lex.get_next_token();

  while (token.has_value()) {
    std::cout << *token << '\n';
    token = lex.get_next_token();
  }

  return EXIT_SUCCESS;
};
