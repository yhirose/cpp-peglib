// ABI probe for cpp-peglib.
//
// cpp-peglib is a header-only library, so there is no shared object to diff
// directly. This translation unit anchors the public ABI surface: by taking
// the address of / out-of-lining uses of the public types, the compiler emits
// their DWARF type info and out-of-line symbol copies into a shared library.
// `abidiff` then compares that library between the previous release and the
// current tree to detect ABI-breaking changes to the public types.
//
// Keep this file conservative: it must compile against older releases too
// (the CI copies the current probe over the previous checkout), so only
// reference long-standing public API.

#include "../peglib.h"

namespace peg_abi_probe {

peg::parser *make_parser() {
  auto *p = new peg::parser();
  p->load_grammar("S <- 'a'");
  return p;
}

bool run(peg::parser &p, std::string_view sv) { return p.parse(sv); }

peg::SemanticValues *make_semantic_values() {
  return new peg::SemanticValues();
}

} // namespace peg_abi_probe
