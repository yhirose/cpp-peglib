// Times cpp-peglib parsing a whole SQL input with a given grammar, building the
// AST, and freeing it, all inside the timed region. Compares with
// duck_extract.cc on the same input.
// Usage: peg_parse <grammar.peg> <sql file> <iterations>
// prints the minimum and the median time in ms, and the statement count.
// Usage: peg_parse <grammar.peg> <sql file> rss
// keeps the AST and prints the peak RSS in MB, and the statement count.

#include <peglib.h>
#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

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

double peak_rss_mb() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss / (1024.0 * 1024.0); // macOS reports bytes
}

// The grammars make the root node a list with one child per statement (the node
// names differ between grammars, so the children are counted, not their names).
size_t count_statements(const peg::Ast &root) { return root.nodes.size(); }

} // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    fail("usage: peg_parse <grammar.peg> <sql file> <iterations | rss>");
  }
  const std::string grammar = read_file(argv[1]);
  const std::string sql = read_file(argv[2]);
  const bool rss = std::string(argv[3]) == "rss";
  const int iterations = rss ? 1 : std::atoi(argv[3]);
  if (!rss && iterations <= 0) {
    fail("iterations must be a positive integer");
  }

  peg::parser parser(grammar);
  if (!parser) { fail("failed to load the grammar"); }
  parser.enable_packrat_parsing();
  parser.enable_ast(true);

  // Returns the number of statements in the AST; the AST is released by the
  // caller.
  auto parse = [&](std::shared_ptr<peg::Ast> &ast) {
    if (!parser.parse(sql, ast)) { fail("parse failed"); }
    return count_statements(*ast);
  };

  if (rss) {
    std::shared_ptr<peg::Ast> ast;
    size_t count = parse(ast);
    std::printf("%.1f %zu\n", peak_rss_mb(), count);
    return 0;
  }

  size_t count = 0;
  auto run = [&]() {
    auto start = std::chrono::steady_clock::now();
    std::shared_ptr<peg::Ast> ast;
    count = parse(ast);
    ast.reset();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
  };

  run(); // warm-up
  std::vector<double> times;
  for (int i = 0; i < iterations; i++) {
    times.push_back(run());
  }
  std::sort(times.begin(), times.end());

  std::printf("%.3f %.3f %zu\n", times.front(), times[times.size() / 2], count);
  return 0;
}
