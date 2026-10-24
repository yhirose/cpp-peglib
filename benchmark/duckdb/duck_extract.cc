// Times duckdb_extract_statements() on a whole SQL input: the statements are
// split and parsed, and the extracted statements are destroyed, all inside the
// timed region. Built once per DuckDB version (see CMakeLists.txt); the two
// builds are never linked together.
// Usage: duck_extract <sql file> <iterations>
// prints the minimum and the median time in ms, and the statement count.
// Usage: duck_extract <sql file> rss
// keeps the extracted statements and prints the peak RSS in MB, and the
// statement count.

#include <duckdb.h>
#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) { fail("usage: duck_extract <sql file> <iterations | rss>"); }
  const std::string sql = read_file(argv[1]);
  const bool rss = std::string(argv[2]) == "rss";
  const int iterations = rss ? 1 : std::atoi(argv[2]);
  if (!rss && iterations <= 0) {
    fail("iterations must be a positive integer");
  }

  duckdb_database db;
  duckdb_connection con;
  if (duckdb_open(nullptr, &db) != DuckDBSuccess) {
    fail("duckdb_open failed");
  }
  if (duckdb_connect(db, &con) != DuckDBSuccess) {
    fail("duckdb_connect failed");
  }

  // Returns the number of statements; the extracted handle is destroyed by the
  // caller.
  auto extract = [&](duckdb_extracted_statements *out) {
    idx_t count = duckdb_extract_statements(con, sql.c_str(), out);
    if (count == 0) {
      std::fprintf(stderr, "extract failed: %s\n",
                   duckdb_extract_statements_error(*out));
      std::exit(1);
    }
    return count;
  };

  if (rss) {
    duckdb_extracted_statements handle;
    idx_t count = extract(&handle);
    std::printf("%.1f %llu\n", peak_rss_mb(), (unsigned long long)count);
    duckdb_destroy_extracted(&handle);
    return 0;
  }

  idx_t count = 0;
  auto run = [&]() {
    auto start = std::chrono::steady_clock::now();
    duckdb_extracted_statements handle;
    count = extract(&handle);
    duckdb_destroy_extracted(&handle);
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
  };

  run(); // warm-up
  std::vector<double> times;
  for (int i = 0; i < iterations; i++) {
    times.push_back(run());
  }
  std::sort(times.begin(), times.end());

  std::printf("%.3f %.3f %llu\n", times.front(), times[times.size() / 2],
              (unsigned long long)count);

  duckdb_disconnect(&con);
  duckdb_close(&db);
  return 0;
}
