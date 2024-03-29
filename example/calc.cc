#include <assert.h>
#include <iostream>
#include <peglib.h>

using namespace peg;
using namespace std;

int main(void) {
  // (2) Make a parser
  parser parser(R"(
        # Grammar for Calculator...
        Additive    <- Multiplicative '+' Additive / Multiplicative
        Multiplicative   <- Primary '*' Multiplicative^cond / Primary
        Primary     <- '(' Additive ')' / Number
        Number      <- < [0-9]+ >
        %whitespace <- [ \t]*
        cond <- '' { error_message "missing multiplicative" }
    )");

  assert(static_cast<bool>(parser) == true);

  // (3) Setup actions
  parser["Additive"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: // "Multiplicative '+' Additive"
      return any_cast<int>(vs[0]) + any_cast<int>(vs[1]);
    default: // "Multiplicative"
      return any_cast<int>(vs[0]);
    }
  };

  parser["Multiplicative"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: // "Primary '*' Multiplicative"
      return any_cast<int>(vs[0]) * any_cast<int>(vs[1]);
    default: // "Primary"
      return any_cast<int>(vs[0]);
    }
  };

  parser["Number"] = [](const SemanticValues &vs) {
    return vs.token_to_number<int>();
  };

  // (4) Parse
  parser.enable_packrat_parsing(); // Enable packrat parsing.

  int val = 0;
  parser.parse(" (1 + 2) * ", val);

  // assert(val == 9);
  assert(val == 0);
}
