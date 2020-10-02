#include <assert.h>
#include <iostream>
#include <peglib.h>

using namespace peg;
using namespace std;

int main(void) {
  parser parser(R"(
        EXPRESSION  <- ATOM (OPERATOR ATOM)* {
                         precedence
                           L - +
                           L / *
                       }
        ATOM        <- NUMBER / '(' EXPRESSION ')'
        OPERATOR    <- < [-+/*] >
        NUMBER      <- < '-'? [0-9]+ >
        %whitespace <- [ \t\r\n]*
    )");

  parser["EXPRESSION"] = [](const SemanticValues &vs) {
    auto result = any_cast<long>(vs[0]);
    if (vs.size() > 1) {
      auto ope = any_cast<char>(vs[1]);
      auto num = any_cast<long>(vs[2]);
      switch (ope) {
      case '+': result += num; break;
      case '-': result -= num; break;
      case '*': result *= num; break;
      case '/': result /= num; break;
      }
    }
    return result;
  };
  parser["OPERATOR"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
  parser["NUMBER"] = [](const SemanticValues &vs) { return atol(vs.sv().data()); };

  long val;
  parser.parse(" -1 + (1 + 2) * 3 - -1", val);

  assert(val == 9);
}
