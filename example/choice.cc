//
//  choice.cc
//
//  Copyright (c) 2023 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <cstdlib>
#include <iostream>
#include <peglib.h>

using namespace peg;

int main(void) {
  parser parser(R"(
type <- 'string' / 'int' / 'double'
%whitespace <- [ \t\r\n]*
  )");

  parser["type"] = [](const SemanticValues &vs) {
    std::cout << vs.choice() << std::endl;
  };

  if (parser.parse("int")) { return 0; }

  std::cout << "syntax error..." << std::endl;
  return -1;
}
