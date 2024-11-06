#include "lexer.h"
#include <algorithm>

std::ostream &operator<<(std::ostream &os, const Position &dt) {
  os << "Position(" << dt.line_number << ":" << dt.character << ")";
  return os;
}

std::ostream &operator<<(std::ostream &os, const NewLine &dt) {
  os << std::setw(31) << "NewLine(" << dt.position.line_number << ":"
     << dt.position.character << ")";
  return os;
}

auto RawPreprocessorToken::size() const -> std::size_t {
  return raw_token.size();
}
auto RawPreprocessorToken::cut(std::size_t pos) {
  raw_token = raw_token.substr(pos);
}

std::ostream &operator<<(std::ostream &os, const RawPreprocessorToken &dt) {
  os << std::setw(31) << "RawToken(" << dt.position.line_number << ":"
     << dt.position.character << ")\t\"" << dt.raw_token << "\"";
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
  if (dt.encoding_prefix.has_value()) {
    os << " With encoding_prefix " << "\"" << dt.encoding_prefix.value()
       << "\"";
  }
  if (dt.suffix.has_value()) {
    os << " With suffix " << "\"" << dt.suffix.value() << "\"";
  }
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

auto Lexer::parse_string_literal() -> std::optional<StringLiteral> {
  std::string buffer;
  bool is_string_literal{false};
  bool string_literal{false};
  auto current_pos = pos;
  auto new_pos = pos;
  char c;
  while (istream.get(c)) {
    if (c == '\\') {
      char peek;
      if (istream.get(peek)) {
        if (peek == '\"' && !string_literal) {
          istream.putback(peek);
          break;
        } else if (peek == '\n' || peek == '\"') {
          buffer += c;
          buffer += peek;
          new_pos.character = 0;
          new_pos.line_number += 2;
          continue;
        }
        istream.putback(peek);
      }
    }
    if (c == '\"') {
      if (is_string_literal == true) {
        istream.putback(c);
        break;
      }
      if (string_literal == true) {
        is_string_literal = true;
      }
      string_literal = !string_literal;
      buffer += c;
      new_pos.character += 1;
      continue;
    }

    if ((std::isspace(c) || !(is_digit(c) || is_nondigit(c))) &&
        !string_literal) {
      istream.putback(c);
      break;
    }
    buffer += c;
    new_pos.character += 1;
  }

  if (buffer.empty()) {
    return {};
  }

  if (is_string_literal == false) {
    std::for_each(buffer.rbegin(), buffer.rend(),
                  [&](char c) { istream.putback(c); });
    return {};
  }
  auto prefix = read_identifier(buffer.begin(), buffer.end());
  if (buffer.front() != '\"' && !prefix.has_value()) {
    std::for_each(buffer.rbegin(), buffer.rend(),
                  [&](char c) { istream.putback(c); });
    // TODO: We should probably error here
    std::cerr << "Invalid prefix value in " << buffer << '\n';
    return {};
  }
  auto begin_of_string = buffer.find_first_of("\"");
  auto end_of_string = buffer.find_last_of("\"");
  std::string string{
      buffer.substr(begin_of_string, end_of_string - begin_of_string + 1)};

  std::optional<std::string> suffix;
  if ((buffer.begin() + end_of_string + 1) != buffer.end()) {
    auto maybe_suffix =
        read_identifier(buffer.begin() + end_of_string + 1, buffer.end());
    if (!maybe_suffix.has_value()) {
      std::for_each(buffer.rbegin(), buffer.rend(),
                    [&](char c) { istream.putback(c); });
      // TODO: We should probably error here
      std::cerr << "Invalid suffix value in " << buffer << '\n';
      return {};
    }
    suffix = std::move(maybe_suffix);
  }

  pos = new_pos;
  return StringLiteral{prefix, string, suffix, current_pos};
}

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
      if (raw_token->raw_token.empty()) {
        token_buffer.reset();
        return get_next_token();
      }
      auto ret_pos = raw_token->position;
      {
        auto maybe_token = read_identifier(raw_token->raw_token.begin(),
                                           raw_token->raw_token.end());
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
        auto maybe_token = read_ppnumber(raw_token->raw_token.begin(),
                                         raw_token->raw_token.end());
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
        auto maybe_token = read_operator_or_punctuator(raw_token->raw_token);
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
  auto string_literal = parse_string_literal();
  if (string_literal.has_value()) {
    return string_literal;
  }
  while (istream.get(c)) {
    if (c == '\\') {
      char peek;
      if (istream.get(peek)) {
        if (peek == '\n') {
          pos.character = 0;
          pos.line_number += 1;
          continue;
        }
        istream.putback(peek);
      }
    }
    if (std::isspace(c)) {
      if (c == '\n' && raw_buffer.empty()) {
        pos.character = 0;
        pos.line_number += 1;
        return NewLine{ret_pos};
      } else if (c == '\n') {
        istream.putback(c);
        break;
      }
      pos.character += 1;
      if (raw_buffer.empty()) {
        ret_pos.character += 1;
        continue;
      }
      break;
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
