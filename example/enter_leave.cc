//
//  enter_leave.cc
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
    S <- A+
    A <- 'A'
  )");

  parser["A"].enter = [](const Context & /*c*/, const char * /*s*/,
                         size_t /*n*/, std::any & /*dt*/) {
    std::cout << "enter" << std::endl;
  };

  parser["A"] = [](const SemanticValues & /*vs*/, std::any & /*dt*/) {
    std::cout << "action!" << std::endl;
  };

  parser["A"].leave = [](const Context & /*c*/, const char * /*s*/,
                         size_t /*n*/, size_t /*matchlen*/,
                         std::any & /*value*/, std::any & /*dt*/) {
    std::cout << "leave" << std::endl;
  };

  if (parser.parse("A")) { return 0; }

  std::cout << "syntax error..." << std::endl;
  return -1;
}
