// DollarQuotedString, the one rule of the grammar make_grammar.py generates
// that the programs give as a C++ function: from the '$tag$' that opens a
// dollar-quoted string to the first '$tag$' after that. The grammar has
// checked the opening with DollarQuoteStart, so the tag ends at the next '$'.
// Grammars without the rule are not changed by it.

#pragma once

#include <peglib.h>

#include <string_view>

inline peg::Rules dollar_quote_rules() {
  return {
      {"DollarQuotedString", peg::usr([](const char *s, size_t n) -> size_t {
         const std::string_view input(s, n);
         const auto tag_end = input.find('$', 1);
         if (tag_end == std::string_view::npos) { return peg::kFail; }
         const auto open = input.substr(0, tag_end + 1);
         const auto close = input.find(open, open.size());
         if (close == std::string_view::npos) { return peg::kFail; }
         return close + open.size();
       })}};
}
