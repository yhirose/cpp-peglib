// Parses each statement of a file of NUL-separated statements with a grammar,
// with packrat parsing as peg_parse.cc does, and prints one verdict per line: A
// if the grammar accepts it, R if not. check_grammar.py compares these verdicts
// with DuckDB's. The parses run on a thread with a large stack under a depth
// limit, so that the deeply nested statements of DuckDB's tests are parsed
// rather than overflowing the stack.
// Usage: peg_check [--trees] <grammar.peg> <statements file>
// With --trees, an accepted statement is followed by the hash of its AST, so
// that two grammars can be compared by the trees they build. Usage: peg_check
// --dropped-subtrees <grammar.peg> prints the rules that cpp-peglib builds as
// AST tokens although they refer to other rules outside a token boundary, so
// that the AST leaves out what those rules match.

#include <peglib.h>
#include <pthread.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "dollar_quote.h"

namespace {

constexpr size_t stack_size = 512 << 20;
constexpr size_t max_depth = 100000;

std::string read_file(const char *path) {
  std::ifstream f(path, std::ios::binary);
  std::ostringstream s;
  s << f.rdbuf();
  return s.str();
}

[[noreturn]] void fail(const char *message) {
  std::fprintf(stderr, "%s\n", message);
  std::exit(1);
}

// Whether a rule refers to another rule outside its token boundaries and
// predicates, as TokenChecker looks for one.
struct RefersToRule : public peg::TraversalVisitor {
  using peg::TraversalVisitor::visit;
  void visit(peg::TokenBoundary &) override {}
  void visit(peg::AndPredicate &) override {}
  void visit(peg::NotPredicate &) override {}
  void visit(peg::Reference &ope) override {
    if (!ope.is_macro_) {
      result = true;
      return;
    }
    for (const auto &arg : ope.args_) {
      arg->accept(*this);
    }
  }

  bool result = false;
};

struct Job {
  peg::parser *parser;
  const std::string *statements;
  bool trees;
};

void *parse_statements(void *arg) {
  const auto &job = *static_cast<Job *>(arg);
  const auto &statements = *job.statements;
  size_t start = 0;
  while (start <= statements.size()) {
    auto end = statements.find('\0', start);
    if (end == std::string::npos) { end = statements.size(); }
    const std::string sql = statements.substr(start, end - start);
    if (job.trees) {
      std::shared_ptr<peg::Ast> ast;
      if (job.parser->parse(sql, ast)) {
        std::printf("A %016zx\n", std::hash<std::string>{}(peg::ast_to_s(ast)));
      } else {
        std::printf("R\n");
      }
    } else {
      std::printf("%c\n", job.parser->parse(sql) ? 'A' : 'R');
    }
    start = end + 1;
  }
  return nullptr;
}

} // namespace

int main(int argc, char **argv) {
  const bool trees = argc > 1 && std::strcmp(argv[1], "--trees") == 0;
  if (trees) {
    argc--;
    argv++;
  }
  if (argc != 3) {
    fail("usage: peg_check [--trees] <grammar.peg> <statements file>\n"
         "       peg_check --dropped-subtrees <grammar.peg>");
  }
  const bool dropped_subtrees = std::strcmp(argv[1], "--dropped-subtrees") == 0;
  peg::parser parser(read_file(argv[dropped_subtrees ? 2 : 1]),
                     dollar_quote_rules());
  if (!parser) { fail("failed to load the grammar"); }

  if (dropped_subtrees) {
    for (const auto &[name, rule] : parser.get_grammar()) {
      RefersToRule visitor;
      rule.get_core_operator()->accept(visitor);
      if (rule.is_token() && visitor.result) {
        std::printf("%s\n", name.c_str());
      }
    }
    return 0;
  }

  parser.enable_packrat_parsing();
  if (trees) { parser.enable_ast(true); }
  parser.set_max_depth(max_depth);
  const std::string statements = read_file(argv[2]);
  Job job{&parser, &statements, trees};

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, stack_size);
  pthread_t thread;
  if (pthread_create(&thread, &attr, parse_statements, &job) != 0) {
    fail("pthread_create failed");
  }
  pthread_join(thread, nullptr);
  pthread_attr_destroy(&attr);
  return 0;
}
