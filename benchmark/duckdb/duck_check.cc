// Parses each statement of a file of NUL-separated statements with
// duckdb_extract_statements(), and prints one verdict per line: A if DuckDB
// accepts it, S if its tokenizer or grammar rejects it (a syntax error), and T
// if it fails later, in the transformer that builds the statement. An input
// with no statement, such as a lone ';', is accepted. check_grammar.py compares
// these verdicts with the grammar's. Built once per DuckDB version, like
// duck_extract.cc.
// Usage: duck_check <statements file>

#include <duckdb.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

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

// The messages of the errors raised before the transformer runs: by the UTF-8
// check (parser.cpp), the tokenizer (parser_tokenizer.cpp), the number matcher
// (number_literal_matcher.hpp) and a failed match
// (peg_transformer_factory.cpp). Only the start is compared, because a
// transformer error can quote the query.
const std::string_view syntax_errors[] = {
    "Parser Error: syntax error at or near",
    "Parser Error: unterminated",
    "Parser Error: zero-length delimited identifier",
    "Parser Error: Invalid UTF-8 in query",
    "Parser Error: Already found scientific notation",
};

char verdict(const char *error) {
  if (!error) { return 'A'; }
  const std::string_view message(error);
  for (auto syntax_error : syntax_errors) {
    if (message.substr(0, syntax_error.size()) == syntax_error) { return 'S'; }
  }
  return 'T';
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) { fail("usage: duck_check <statements file>"); }
  const std::string statements = read_file(argv[1]);

  duckdb_database db;
  duckdb_connection con;
  if (duckdb_open(nullptr, &db) != DuckDBSuccess) {
    fail("duckdb_open failed");
  }
  if (duckdb_connect(db, &con) != DuckDBSuccess) {
    fail("duckdb_connect failed");
  }

  size_t start = 0;
  while (start <= statements.size()) {
    auto end = statements.find('\0', start);
    if (end == std::string::npos) { end = statements.size(); }
    const std::string sql = statements.substr(start, end - start);
    duckdb_extracted_statements handle = nullptr;
    duckdb_extract_statements(con, sql.c_str(), &handle);
    std::printf("%c\n", verdict(duckdb_extract_statements_error(handle)));
    duckdb_destroy_extracted(&handle);
    start = end + 1;
  }

  duckdb_disconnect(&con);
  duckdb_close(&db);
  return 0;
}
