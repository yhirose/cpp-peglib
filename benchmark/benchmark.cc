#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include <peglib.h>

#ifdef HAS_PG_QUERY
extern "C" {
#include <pg_query.h>
}
#endif

using namespace peg;
using namespace std;

static string read_file(const string &path) {
  ifstream ifs(path, ios::in | ios::binary);
  if (!ifs) {
    cerr << "Error: cannot open " << path << endl;
    exit(1);
  }
  return string((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
}

struct BenchResult {
  string name;
  vector<double> durations_ms;

  double median() const {
    auto sorted = durations_ms;
    sort(sorted.begin(), sorted.end());
    auto n = sorted.size();
    return n % 2 == 0 ? (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0
                      : sorted[n / 2];
  }

  double mean() const {
    return accumulate(durations_ms.begin(), durations_ms.end(), 0.0) /
           durations_ms.size();
  }

  double min_val() const {
    return *min_element(durations_ms.begin(), durations_ms.end());
  }

  double max_val() const {
    return *max_element(durations_ms.begin(), durations_ms.end());
  }
};

static void print_result(const BenchResult &r) {
  cout << "  " << left << setw(30) << r.name << right << "  median: " << fixed
       << setprecision(3) << setw(10) << r.median()
       << " ms   mean: " << setw(10) << r.mean() << " ms   min: " << setw(10)
       << r.min_val() << " ms   max: " << setw(10) << r.max_val() << " ms"
       << "   (n=" << r.durations_ms.size() << ")" << endl;
}

template <typename F>
BenchResult bench(const string &name, int iterations, F func) {
  BenchResult result;
  result.name = name;

  // Warmup
  func();

  for (int i = 0; i < iterations; i++) {
    auto start = chrono::high_resolution_clock::now();
    func();
    auto end = chrono::high_resolution_clock::now();
    double ms =
        chrono::duration_cast<chrono::microseconds>(end - start).count() /
        1000.0;
    result.durations_ms.push_back(ms);
  }

  return result;
}

// cpp-peglib benchmarks
static BenchResult bench_sql_grammar_load(const string &sql_grammar,
                                          int iterations) {
  return bench("PEG: grammar load", iterations, [&]() {
    parser pg(sql_grammar);
    if (!pg) {
      cerr << "Error: failed to parse SQL grammar" << endl;
      exit(1);
    }
  });
}

static BenchResult bench_sql_parse(const string &name,
                                   const string &sql_grammar,
                                   const string &sql_input, int iterations) {
  parser pg(sql_grammar);
  if (!pg) {
    cerr << "Error: failed to parse SQL grammar" << endl;
    exit(1);
  }
  pg.enable_packrat_parsing();

  return bench(name, iterations, [&]() { pg.parse(sql_input); });
}

// PostgreSQL YACC (libpg_query) benchmarks
#ifdef HAS_PG_QUERY
static BenchResult bench_yacc_parse(const string &name, const string &sql_input,
                                    int iterations) {
  return bench(name, iterations, [&]() {
    PgQueryParseResult result = pg_query_parse(sql_input.c_str());
    if (result.error) {
      cerr << "YACC parse error: " << result.error->message << endl;
    }
    pg_query_free_parse_result(result);
  });
}
#endif

int main(int argc, char *argv[]) {
  int iterations = 10;
  if (argc > 1) {
    iterations = atoi(argv[1]);
    if (iterations <= 0) {
      cerr << "Error: iterations must be a positive integer" << endl;
      return 1;
    }
  }

  string data_dir = BENCHMARK_DATA_DIR;

  auto sql_grammar = read_file(data_dir + "/sql.gram");
  auto q1_sql = read_file(data_dir + "/q1.sql");
  auto tpch_sql = read_file(data_dir + "/all-tpch.sql");
  auto big_sql = read_file(data_dir + "/big.sql");

  cout << "cpp-peglib SQL benchmark (" << iterations << " iterations)" << endl;
#ifdef HAS_PG_QUERY
  cout << "(with libpg_query YACC comparison)" << endl;
#endif
  cout << string(70, '=') << endl;

  vector<BenchResult> results;
  int test_num = 1;

  // PEG benchmarks
  cout << endl << "--- cpp-peglib (PEG) ---" << endl;

  cout << endl
       << "[" << test_num++ << "] PEG: grammar load (" << sql_grammar.size()
       << " bytes)" << endl;
  results.push_back(bench_sql_grammar_load(sql_grammar, iterations));

  cout << endl
       << "[" << test_num++ << "] PEG: TPC-H Q1 (" << q1_sql.size() << " bytes)"
       << endl;
  results.push_back(
      bench_sql_parse("PEG: TPC-H Q1", sql_grammar, q1_sql, iterations));

  cout << endl
       << "[" << test_num++ << "] PEG: all TPC-H (" << tpch_sql.size()
       << " bytes)" << endl;
  results.push_back(
      bench_sql_parse("PEG: all TPC-H", sql_grammar, tpch_sql, iterations));

  cout << endl
       << "[" << test_num++ << "] PEG: big.sql (" << big_sql.size() << " bytes)"
       << endl;
  results.push_back(
      bench_sql_parse("PEG: big.sql (~1MB)", sql_grammar, big_sql, iterations));

  // YACC benchmarks
#ifdef HAS_PG_QUERY
  cout << endl << "--- PostgreSQL YACC (libpg_query) ---" << endl;

  cout << endl
       << "[" << test_num++ << "] YACC: TPC-H Q1 (" << q1_sql.size()
       << " bytes)" << endl;
  results.push_back(bench_yacc_parse("YACC: TPC-H Q1", q1_sql, iterations));

  cout << endl
       << "[" << test_num++ << "] YACC: all TPC-H (" << tpch_sql.size()
       << " bytes)" << endl;
  results.push_back(bench_yacc_parse("YACC: all TPC-H", tpch_sql, iterations));

  cout << endl
       << "[" << test_num++ << "] YACC: big.sql (" << big_sql.size()
       << " bytes)" << endl;
  results.push_back(
      bench_yacc_parse("YACC: big.sql (~1MB)", big_sql, iterations));
#endif

  cout << endl << string(70, '=') << endl;
  cout << "Summary:" << endl;
  for (const auto &r : results) {
    print_result(r);
  }

#ifdef HAS_PG_QUERY
  // Print ratio for big.sql
  auto peg_big = results[3].median();
  auto yacc_big = results[6].median();
  cout << endl
       << "  Ratio (big.sql): PEG/YACC = " << fixed << setprecision(1)
       << peg_big / yacc_big << "x" << endl;
#endif

  return 0;
}
