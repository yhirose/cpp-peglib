//
//  sequence.cc
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
START       <- SEQUENCE_A
SEQUENCE_A  <- SEQUENCE('A')
SEQUENCE(X) <- X (',' X)*
  )");

  parser["SEQUENCE_A"] = [](const SemanticValues & /*vs*/) {
    std::cout << "SEQUENCE_A" << std::endl;
  };

  if (parser.parse("A,A")) { return 0; }

  std::cout << "syntax error..." << std::endl;
  return -1;
}
