#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
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

static void print_bar_chart(const vector<BenchResult> &results) {
  // Collect big.sql results for the bar chart
  struct Entry {
    string name;
    double ms;
  };
  vector<Entry> entries;

  for (const auto &r : results) {
    if (r.name.find("big.sql") != string::npos) {
      entries.push_back({r.name, r.median()});
    }
  }

  if (entries.empty()) return;

  // Sort by time (fastest first)
  sort(entries.begin(), entries.end(),
       [](const Entry &a, const Entry &b) { return a.ms < b.ms; });

  double baseline = entries.front().ms; // fastest as 1.0x
  double max_ms = entries.back().ms;

  const int max_bar_width = 30;
  const int name_width = 28;

  cout << endl << "  Bar chart (big.sql):" << endl << endl;

  for (const auto &e : entries) {
    double ratio = baseline > 0 ? e.ms / baseline : 0;
    int bar_len =
        max_ms > 0 ? static_cast<int>(e.ms / max_ms * max_bar_width) : 0;
    if (bar_len < 1) bar_len = 1;

    // Build bar with Unicode block character U+2588
    string bar_str;
    for (int i = 0; i < bar_len; i++) {
      bar_str += "\xe2\x96\x88";
    }
    // Pad with spaces to align the numbers
    string padding(max_bar_width - bar_len, ' ');

    char line[256];
    snprintf(line, sizeof(line), "%6.1f ms (%.1fx)", e.ms, ratio);

    // Print: name | bar + padding + numbers
    cout << "  " << left << setw(name_width) << e.name << " |" << bar_str
         << padding << "  " << line << endl;
  }
}

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

// Profile subcommand: per-rule self-time profiling
struct RuleStats {
  string name;
  size_t success = 0;
  size_t fail = 0;
  double self_ns = 0; // exclusive (self) time in nanoseconds
};

struct ProfileData {
  vector<RuleStats> rules;
  map<string, size_t> index;
  vector<chrono::steady_clock::time_point> enter_times;
  vector<double> child_ns; // accumulated child time at each stack level
  chrono::steady_clock::time_point start;
};

static int run_profile(const string &data_dir, int argc, char *argv[]) {
  // Determine which input to profile
  string input_name = "big.sql";
  if (argc > 0) { input_name = argv[0]; }

  string input_file;
  if (input_name == "q1") {
    input_file = data_dir + "/q1.sql";
  } else if (input_name == "tpch") {
    input_file = data_dir + "/all-tpch.sql";
  } else {
    input_file = data_dir + "/big.sql";
  }

  // Grammar: default or "optimized"
  string grammar_file = data_dir + "/sql.peg";
  for (int i = 0; i < argc; i++) {
    string arg = argv[i];
    if (arg == "optimized" || arg == "opt") {
      grammar_file = data_dir + "/sql-optimized.peg";
    }
  }
  auto sql_grammar = read_file(grammar_file);
  auto sql_input = read_file(input_file);

  parser pg(sql_grammar);
  if (!pg) {
    cerr << "Error: failed to parse SQL grammar" << endl;
    return 1;
  }
  pg.enable_packrat_parsing();

  ProfileData *profile_result = nullptr;

  pg.enable_trace(
      // enter
      [](auto &ope, auto, auto, auto &, auto &, auto &, std::any &trace_data) {
        auto holder = dynamic_cast<const peg::Holder *>(&ope);
        if (!holder) return;

        auto &pd = *std::any_cast<ProfileData *>(trace_data);
        auto &name = holder->name();
        if (pd.index.find(name) == pd.index.end()) {
          pd.index[name] = pd.rules.size();
          pd.rules.push_back({name, 0, 0, 0});
        }

        pd.enter_times.push_back(chrono::steady_clock::now());
        pd.child_ns.push_back(0);
      },
      // leave
      [](auto &ope, auto, auto, auto &, auto &, auto &, auto len,
         std::any &trace_data) {
        auto holder = dynamic_cast<const peg::Holder *>(&ope);
        if (!holder) return;

        auto &pd = *std::any_cast<ProfileData *>(trace_data);
        auto now = chrono::steady_clock::now();
        auto elapsed =
            chrono::duration<double, nano>(now - pd.enter_times.back()).count();
        auto child_time = pd.child_ns.back();
        auto self_time = elapsed - child_time;

        pd.enter_times.pop_back();
        pd.child_ns.pop_back();

        // Add elapsed to parent's child accumulator
        if (!pd.child_ns.empty()) { pd.child_ns.back() += elapsed; }

        auto &name = holder->name();
        auto idx = pd.index[name];
        auto &stat = pd.rules[idx];
        stat.self_ns += self_time;
        if (len != static_cast<size_t>(-1)) {
          stat.success++;
        } else {
          stat.fail++;
        }
      },
      // start
      [&profile_result](auto &trace_data) {
        auto pd = new ProfileData{};
        pd->start = chrono::steady_clock::now();
        trace_data = pd;
        profile_result = pd;
      },
      // end
      [](auto & /*trace_data*/) {});

  // Enable packrat stats collection
  pg["Statements"].collect_packrat_stats = true;

  cout << "Profiling parse of " << input_file << " (" << sql_input.size()
       << " bytes)..." << endl;

  auto t0 = chrono::steady_clock::now();
  pg.parse(sql_input);
  auto t1 = chrono::steady_clock::now();
  auto total_ms =
      chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;

  // Output results
  auto &pd = *profile_result;
  auto &rules = pd.rules;

  vector<size_t> order(rules.size());
  iota(order.begin(), order.end(), 0);
  sort(order.begin(), order.end(),
       [&](size_t a, size_t b) { return rules[a].self_ns > rules[b].self_ns; });

  size_t total_calls = 0;
  double total_self_ns = 0;
  for (auto &r : rules) {
    total_calls += r.success + r.fail;
    total_self_ns += r.self_ns;
  }

  cout << endl;
  cout << "Profile: " << input_file << " (" << sql_input.size() << " bytes)"
       << endl;
  cout << "Total time: " << fixed << setprecision(3) << total_ms << " ms"
       << endl;
  cout << "Total rule calls: " << total_calls << endl;
  cout << endl;

  char buf[256];
  snprintf(buf, sizeof(buf), "%4s  %-30s  %10s  %6s  %10s  %10s  %6s  %8s",
           "rank", "rule", "self(ms)", "%", "success", "fail", "fail%",
           "avg(ns)");
  cout << buf << endl;
  cout << string(100, '-') << endl;

  size_t rank = 1;
  for (auto i : order) {
    auto &r = rules[i];
    auto total = r.success + r.fail;
    if (total == 0) continue;
    auto self_ms = r.self_ns / 1e6;
    auto pct = r.self_ns / total_self_ns * 100.0;
    auto fail_pct = total > 0 ? r.fail * 100.0 / total : 0.0;
    auto avg_ns = r.self_ns / total;
    snprintf(buf, sizeof(buf),
             "%4zu  %-30s  %10.3f  %5.1f%%  %10zu  %10zu  %5.1f%%  %8.1f", rank,
             r.name.c_str(), self_ms, pct, r.success, r.fail, fail_pct, avg_ns);
    cout << buf << endl;
    rank++;
  }

  // Packrat stats
  auto &pkstats = pg["Statements"].packrat_stats_;
  if (!pkstats.empty()) {
    cout << endl;
    cout << "Packrat cache stats per rule:" << endl;
    snprintf(buf, sizeof(buf), "%4s  %-30s  %10s  %10s  %10s  %6s", "rank",
             "rule", "hits", "misses", "total", "hit%");
    cout << buf << endl;
    cout << string(80, '-') << endl;

    // Build def_id → name map from ProfileData
    map<size_t, string> defid_to_name;
    for (auto &[name, idx] : pd.index) {
      try {
        auto &rule = pg[name.c_str()];
        defid_to_name[rule.id] = name;
      } catch (...) {}
    }

    struct PkEntry {
      string name;
      size_t hits, misses;
    };
    vector<PkEntry> pk_entries;
    size_t total_hits = 0, total_misses = 0;
    for (size_t i = 0; i < pkstats.size(); i++) {
      auto &st = pkstats[i];
      if (st.hits + st.misses == 0) continue;
      auto it = defid_to_name.find(i);
      string name =
          it != defid_to_name.end() ? it->second : "id=" + to_string(i);
      pk_entries.push_back({name, st.hits, st.misses});
      total_hits += st.hits;
      total_misses += st.misses;
    }

    sort(pk_entries.begin(), pk_entries.end(), [](auto &a, auto &b) {
      return a.hits + a.misses > b.hits + b.misses;
    });

    rank = 1;
    for (auto &e : pk_entries) {
      auto total = e.hits + e.misses;
      auto hit_pct = total > 0 ? e.hits * 100.0 / total : 0.0;
      snprintf(buf, sizeof(buf), "%4zu  %-30s  %10zu  %10zu  %10zu  %5.1f%%",
               rank, e.name.c_str(), e.hits, e.misses, total, hit_pct);
      cout << buf << endl;
      rank++;
    }
    cout << endl;
    snprintf(buf, sizeof(buf),
             "Total: hits=%zu misses=%zu total=%zu hit%%=%.1f%%", total_hits,
             total_misses, total_hits + total_misses,
             (total_hits + total_misses) > 0
                 ? total_hits * 100.0 / (total_hits + total_misses)
                 : 0.0);
    cout << buf << endl;
  }

  delete profile_result;
  return 0;
}

int main(int argc, char *argv[]) {
  string data_dir = BENCHMARK_DATA_DIR;

  // Check for subcommands
  if (argc > 1 && strcmp(argv[1], "profile") == 0) {
    return run_profile(data_dir, argc - 2, argv + 2);
  }

  int iterations = 10;
  if (argc > 1) {
    iterations = atoi(argv[1]);
    if (iterations <= 0) {
      cerr << "Error: iterations must be a positive integer" << endl;
      return 1;
    }
  }

  auto sql_grammar = read_file(data_dir + "/sql.peg");
  auto q1_sql = read_file(data_dir + "/q1.sql");
  auto tpch_sql = read_file(data_dir + "/all-tpch.sql");
  auto big_sql = read_file(data_dir + "/big.sql");

  cout << "cpp-peglib SQL benchmark (" << iterations << " iterations)";
#ifdef HAS_PG_QUERY
  cout << "(with libpg_query YACC comparison)";
#endif
  cout << endl;
  cout << string(80, '=') << endl;

  vector<BenchResult> results;
  int test_num = 1;

  // PEG benchmarks
  cout << "--- cpp-peglib (PEG) ---" << endl;

  cout << "[" << test_num++ << "] PEG: grammar load (" << sql_grammar.size()
       << " bytes)" << endl;
  results.push_back(bench_sql_grammar_load(sql_grammar, iterations));

  cout << "[" << test_num++ << "] PEG: TPC-H Q1 (" << q1_sql.size() << " bytes)"
       << endl;
  results.push_back(
      bench_sql_parse("PEG: TPC-H Q1", sql_grammar, q1_sql, iterations));

  cout << "[" << test_num++ << "] PEG: all TPC-H (" << tpch_sql.size()
       << " bytes)" << endl;
  results.push_back(
      bench_sql_parse("PEG: all TPC-H", sql_grammar, tpch_sql, iterations));

  cout << "[" << test_num++ << "] PEG: big.sql (" << big_sql.size() << " bytes)"
       << endl;
  results.push_back(
      bench_sql_parse("PEG: big.sql (~1MB)", sql_grammar, big_sql, iterations));

  // Optimized grammar benchmarks
  {
    auto opt_grammar = read_file(data_dir + "/sql-optimized.peg");
    cout << endl << "--- cpp-peglib (PEG, optimized grammar) ---" << endl;

    cout << "[" << test_num++ << "] PEG-opt: big.sql (" << big_sql.size()
         << " bytes)" << endl;
    results.push_back(bench_sql_parse("PEG-opt: big.sql (~1MB)", opt_grammar,
                                      big_sql, iterations));
  }

  // YACC benchmarks
#ifdef HAS_PG_QUERY
  cout << endl << "--- PostgreSQL YACC (libpg_query) ---" << endl;

  cout << "[" << test_num++ << "] YACC: TPC-H Q1 (" << q1_sql.size()
       << " bytes)" << endl;
  results.push_back(bench_yacc_parse("YACC: TPC-H Q1", q1_sql, iterations));

  cout << "[" << test_num++ << "] YACC: all TPC-H (" << tpch_sql.size()
       << " bytes)" << endl;
  results.push_back(bench_yacc_parse("YACC: all TPC-H", tpch_sql, iterations));

  cout << "[" << test_num++ << "] YACC: big.sql (" << big_sql.size()
       << " bytes)" << endl;
  results.push_back(
      bench_yacc_parse("YACC: big.sql (~1MB)", big_sql, iterations));
#endif

  cout << endl << string(80, '=') << endl;
  cout << "Summary:" << endl;
  for (const auto &r : results) {
    print_result(r);
  }

  // Print ratios for big.sql
  auto find_result = [&](const string &name) -> double {
    for (const auto &r : results) {
      if (r.name == name) return r.median();
    }
    return 0.0;
  };

#ifdef HAS_PG_QUERY
  auto peg_big = find_result("PEG: big.sql (~1MB)");
  auto yacc_big = find_result("YACC: big.sql (~1MB)");
  if (peg_big > 0 && yacc_big > 0) {
    cout << endl
         << "  Ratio (big.sql): PEG/YACC = " << fixed << setprecision(1)
         << peg_big / yacc_big << "x" << endl;
  }
#endif

  print_bar_chart(results);

  return 0;
}
