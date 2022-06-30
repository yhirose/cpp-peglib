//
//  symbol_check.cc
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <cstdlib>
#include <iostream>
#include <peglib.h>
#include <set>

using namespace peg;

int main(void) {
  parser parser(R"(
S           <- (Decl / Ref)*
Decl        <- 'decl' symbol(Name)
Ref         <- 'ref' symbol_reference(Name)
Name        <- < [a-zA-Z]+ >
%whitespace <- [ \t\r\n]*

symbol(s)           <- < s >
symbol_reference(s) <- < s >
)");

  std::set<std::string> dic;

  parser[R"(symbol)"].predicate =
      [&](const SemanticValues &vs, const std::any & /*dt*/, std::string &msg) {
        auto tok = vs.token_to_string();
        if (dic.find(tok) != dic.end()) {
          msg = "'" + tok + "' already exists...";
          return false;
        }
        dic.insert(tok);
        return true;
      };

  parser[R"(symbol_reference)"].predicate =
      [&](const SemanticValues &vs, const std::any & /*dt*/, std::string &msg) {
        auto tok = vs.token_to_string();
        if (dic.find(tok) == dic.end()) {
          msg = "'" + tok + "' doesn't exists...";
          return false;
        }
        return true;
      };

  parser.enable_ast();

  parser.log = [](size_t line, size_t col, const std::string &msg) {
    std::cerr << line << ":" << col << ": " << msg << "\n";
  };

  const auto source = R"(decl aaa
ref aaa
ref bbb
)";

  std::shared_ptr<Ast> ast;
  if (parser.parse(source, ast)) {
    ast = parser.optimize_ast(ast);
    std::cout << ast_to_s(ast);
    return 0;
  }

  return -1;
}
