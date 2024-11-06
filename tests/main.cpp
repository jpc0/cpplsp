#include <args.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <lexer.h>
#include <sstream>
#include <vector>

template <typename T, typename U> auto compare(T &in, U &out) -> bool {
  char in_c;
  char out_c;
  while (in.get(in_c) && out.get(out_c)) {
    if (in_c != out_c) {
      return false;
    }
  }
  if (!in.eof() && !out.eof()) {
    return false;
  }
  return true;
}

auto run_test(std::string_view test) -> bool {
  Lexer lex(test);
  std::string outfile = std::string{test} + "_out";
  if (!std::filesystem::exists(outfile)) {
    std::cerr << "Test: \"" << test << "\" No outfile available\n";
    return false;
  }
  auto token = lex.get_next_token();
  std::ifstream out{outfile};
  std::stringstream in{};
  while (token.has_value()) {
    in << *token << '\n';
    token = lex.get_next_token();
  }
  return compare(in, out);
}

auto create_out(std::string_view test) {
  Lexer lex(test);
  std::string outfile = std::string{test} + "_out";
  if (std::filesystem::exists(outfile)) {
    std::filesystem::remove(outfile);
  }
  auto token = lex.get_next_token();
  std::ofstream out{outfile};
  while (token.has_value()) {
    out << *token << '\n';
    token = lex.get_next_token();
  }
  out.flush();
  out.close();
}

constexpr std::array<std::string_view, 7> tests{
    "tests/identifier",
    "tests/ppnumber",
    "tests/hello_world",
    "tests/string_literal",
    "tests/user_defined_string_literal",
    "tests/two_string_literals",
    "tests/raw_string_literal"};

int main(int argc, char **argv) {
  Args args{argc, argv};
  if (args.size() > 2) {
    if (args[1] == "create") {
      create_out(args[2]);
    } else if (args[1] == "run") {
      if (!run_test(args[2])) {
        std::cerr << "Test for \"" << args[2] << "\" failed!\n";
        return EXIT_FAILURE;
      } else {
        std::cerr << "Test for \"" << args[2] << "\" passed!\n";
      }
    } else {
      std::cerr << "Unknown argument passed: {" << args[1] << "}\n";
      return EXIT_FAILURE;
    }
  } else {
    std::vector<std::string> failed;
    for (auto test : tests) {
      if (!run_test(test)) {
        failed.push_back(std::string{test});
      }
    }
    if (!failed.empty()) {
      std::cout << failed.size() << " of " << tests.size() << " failed:\n";
      for (auto test : failed) {
        std::cout << "\tTest for \"" << test << "\" failed!\n";
      }
      return EXIT_FAILURE;
    }
    std::cerr << "All tests passed\n";
  }

  return EXIT_SUCCESS;
};
