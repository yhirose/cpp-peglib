//
//  indent.cc
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

// Based on https://gist.github.com/dmajda/04002578dd41ae8190fc

#include <cstdlib>
#include <iostream>
#include <peglib.h>

using namespace peg;

int main(void) {
  parser parser(R"(Start <- Statements {}
Statements <- Statement*
Statement <- Samedent (S / I)

S <- 'S' EOS { no_ast_opt }
I <- 'I' EOL Block / 'I' EOS { no_ast_opt }

Block <- Statements {}

~Samedent <- ' '* {}

~EOS <- EOL / EOF
~EOL <- '\n'
~EOF <- !.
  )");

  size_t indent = 0;

  parser["Block"].enter = [&](const char * /*s*/, size_t /*n*/,
                              std::any & /*dt*/) { indent += 2; };

  parser["Block"].leave = [&](const char * /*s*/, size_t /*n*/,
                              size_t /*matchlen*/, std::any & /*value*/,
                              std::any & /*dt*/) { indent -= 2; };

  parser["Samedent"] = [&](const SemanticValues &vs, std::any & /*dt*/) {
    if (indent != vs.sv().size()) { throw parse_error("different indent..."); }
  };

  parser.enable_ast();

  const auto source = R"(I
  S
  I
    I
      S
      S
    S
  S
)";

  std::shared_ptr<Ast> ast;
  if (parser.parse(source, ast)) {
    ast = parser.optimize_ast(ast);
    std::cout << ast_to_s(ast);
    return 0;
  }

  std::cout << "syntax error..." << std::endl;
  return -1;
}
