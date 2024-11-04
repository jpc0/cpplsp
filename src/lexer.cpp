#include "lexer.h"

std::ostream &operator<<(std::ostream &os, const Position &dt) {
  os << std::setw(31) << "Position(" << dt.line_number << ":" << dt.character
     << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os, const NewLine &dt) {
  os << std::setw(31) << "NewLine(" << dt.position.line_number << ":"
     << dt.position.character << ")";
  return os;
}

auto RawPreprocessorToken::logical_token() const -> std::string_view {
  auto begin = raw_token.begin();
  for (; begin != raw_token.end(); begin++) {
    auto c = *begin;
    if (c == '\\') {
      auto peek = begin + 1;
      if (peek != raw_token.end()) {
        if (*peek == '\n') {
          std::advance(begin, 2);
        }
      }
    }
    if (std::isspace(c)) {
      break;
    }
  }
  return {raw_token.begin(), begin};
}

auto RawPreprocessorToken::size() const -> std::size_t {
  return logical_token().size();
}
auto RawPreprocessorToken::cut(std::size_t pos) {
  raw_token = raw_token.substr(pos);
}

std::ostream &operator<<(std::ostream &os, const RawPreprocessorToken &dt) {
  os << std::setw(31) << "RawToken(" << dt.position.line_number << ":"
     << dt.position.character << ")\t\"" << dt.logical_token() << "\"";
  return os;
}

auto StringLiteral::logical_token() const -> std::string_view {
  auto begin = raw_token.begin();
  for (; begin != raw_token.end(); begin++) {
    auto c = *begin;
    if (c == '\\') {
      auto peek = begin + 1;
      if (peek != raw_token.end()) {
        if (*peek == '\n') {
          std::advance(begin, 2);
        }
      }
    }
  }
  return {raw_token.begin(), begin};
}

std::ostream &operator<<(std::ostream &os, const StringLiteral &dt) {
  os << std::setw(31) << "StringLiteral(" << dt.position.line_number << ":"
     << dt.position.character << ")\t\"" << dt.logical_token() << "\"";
  return os;
}

std::ostream &operator<<(std::ostream &os, const Identifier &dt) {
  os << std::setw(31) << "Identifier(" << dt.position.line_number << ":"
     << dt.position.character << ")\t\"" << dt.value << "\"";
  return os;
}

std::ostream &operator<<(std::ostream &os, const PPNumber &dt) {
  os << std::setw(31) << "PPNumber(" << dt.position.line_number << ":"
     << dt.position.character << ")\t\"" << dt.value << "\"";
  return os;
}

std::ostream &operator<<(std::ostream &os, const OperatorOrPunctuator &dt) {
  os << std::setw(31) << "OperatorOrPunctuator(" << dt.position.line_number
     << ":" << dt.position.character << ")\t\"" << dt.value << "\"";
  return os;
}

using PreProcessorToken =
    std::variant<RawPreprocessorToken, NewLine, Identifier, PPNumber,
                 OperatorOrPunctuator, StringLiteral>;

std::ostream &operator<<(std::ostream &os, const PreProcessorToken &dt) {
  std::visit([&](auto &&arg) { os << arg; }, dt);
  return os;
}

Lexer::Lexer(std::filesystem::path const &path) : path(path), istream(path){};

auto Lexer::get_next_token() -> std::optional<PreProcessorToken> {
  if (token_buffer.has_value()) {
    if (auto *nl = std::get_if<NewLine>(&*token_buffer)) {
      auto ret = token_buffer;
      token_buffer.reset();
      return ret;
    } else if (auto *string_literal =
                   std::get_if<StringLiteral>(&*token_buffer)) {
      auto ret = token_buffer;
      token_buffer.reset();
      return ret;
    } else if (auto *raw_token =
                   std::get_if<RawPreprocessorToken>(&*token_buffer)) {
      if (raw_token->logical_token().empty()) {
        token_buffer.reset();
        return get_next_token();
      }
      auto ret_pos = raw_token->position;
      {
        auto maybe_token = read_identifier(raw_token->logical_token().begin(),
                                           raw_token->logical_token().end());
        if (maybe_token.has_value()) {
          auto value = maybe_token.value();
          if (value.length() == raw_token->size()) {
            token_buffer.reset();
          } else {
            raw_token->cut(value.length());
            raw_token->position.character += value.length();
          }
          return Identifier{std::move(value), ret_pos};
        }
      }
      {
        auto maybe_token = read_ppnumber(raw_token->logical_token().begin(),
                                         raw_token->logical_token().end());
        if (maybe_token.has_value()) {
          auto value = maybe_token.value();
          if (value.length() == raw_token->size()) {
            token_buffer.reset();
          } else {
            raw_token->cut(value.length());
            raw_token->position.character += value.length();
          }
          return PPNumber{std::move(value), ret_pos};
        }
      }
      {
        auto maybe_token =
            read_operator_or_punctuator(raw_token->logical_token());
        if (maybe_token.has_value()) {
          auto value = maybe_token.value();
          if (value.length() == raw_token->size()) {
            token_buffer.reset();
          } else {
            raw_token->cut(value.length());
            raw_token->position.character += value.length();
          }
          return OperatorOrPunctuator{std::move(value), ret_pos};
        }
      }
      auto ret = token_buffer;
      token_buffer.reset();
      return ret;
    }
  }
  token_buffer = get_raw_token();
  if (!token_buffer.has_value()) {
    return {};
  }
  return get_next_token();
}

auto Lexer::get_raw_token() -> std::optional<PreProcessorToken> {
  char c;
  std::string raw_buffer;
  auto ret_pos = pos;
  bool string_literal{false};
  while (istream.get(c)) {
    if (c == '\"') {
      if (string_literal) {
        raw_buffer += c;
        pos.character += 1;
        return StringLiteral{raw_buffer, ret_pos};
      }
      if (!raw_buffer.empty()) {
        istream.putback(c);
      }
      string_literal = !string_literal;
    }
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
    if (string_literal) {
      raw_buffer += c;
      pos.character += 1;
      continue;
    }
    if (std::isspace(c)) {
      if (c == '\n' && raw_buffer.empty()) {
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
    raw_buffer += c;
    pos.character += 1;
  }
  if (raw_buffer.empty()) {
    return {};
  }
  return RawPreprocessorToken{raw_buffer, ret_pos};
};

auto Lexer::print() -> void {
  char c;
  while (istream.get(c)) {
    std::cout << c;
  }
  istream = std::ifstream(path);
};
