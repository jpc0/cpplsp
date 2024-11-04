#include <args.h>
#include <iostream>
#include <lexer.h>

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
