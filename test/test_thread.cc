#include <gtest/gtest.h>
#include <peglib.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace peg;

namespace {

// Runs f(t) on `threads` threads, all started together.
template <typename F> void run_threads(size_t threads, F f) {
  std::atomic<size_t> ready{0};
  std::vector<std::thread> pool;
  for (size_t t = 0; t < threads; t++) {
    pool.emplace_back([&, t] {
      ready++;
      while (ready < threads) {
        std::this_thread::yield();
      }
      f(t);
    });
  }
  for (auto &th : pool) {
    th.join();
  }
}

} // namespace

// Packrat statistics of parses on several threads add up to the statistics of
// the same parses one after another.
TEST(ThreadTest, PackratStatisticsAddUpAcrossThreads) {
  const char *grammar = R"(
    S  <-  (A / B)*
    A  <-  'a' 'x'
    B  <-  'a' 'y'
  )";
  std::vector<std::string> inputs;
  for (int i = 0; i < 200; i++) {
    inputs.push_back(i % 2 ? "axay" : "aya");
  }

  auto total = [](const peg::parser &parser) {
    size_t sum = 0;
    for (const auto &s : parser["S"].packrat_stats_) {
      sum += s.hits + s.misses;
    }
    return sum;
  };

  peg::parser sequential(grammar);
  sequential.enable_packrat_parsing();
  sequential["S"].collect_packrat_stats = true;
  for (const auto &in : inputs) {
    sequential.parse(in);
  }
  auto expected = total(sequential);
  EXPECT_GT(expected, 0u);

  peg::parser parallel(grammar);
  parallel.enable_packrat_parsing();
  parallel["S"].collect_packrat_stats = true;
  run_threads(4, [&](size_t t) {
    for (size_t i = t; i < inputs.size(); i += 4) {
      parallel.parse(inputs[i]);
    }
  });
  EXPECT_EQ(expected, total(parallel));
}

// A logger that keeps state of its own is called one at a time, however many
// threads parse with the parser.
TEST(ThreadTest, LoggerIsCalledOneAtATime) {
  peg::parser parser(R"(S <- 'a')");
  std::atomic<int> inside{0};
  std::atomic<bool> overlapped{false};
  std::atomic<size_t> calls{0};
  parser.set_logger([&](size_t, size_t, const std::string &) {
    if (inside++ != 0) { overlapped = true; }
    std::this_thread::sleep_for(std::chrono::microseconds(20));
    calls++;
    inside--;
  });
  run_threads(4, [&](size_t) {
    for (int i = 0; i < 10; i++) {
      parser.parse("b");
    }
  });
  EXPECT_FALSE(overlapped);
  EXPECT_EQ(40u, calls);
}

// A tracer that keeps state of its own is called one at a time, too.
TEST(ThreadTest, TracerIsCalledOneAtATime) {
  peg::parser parser(R"(S <- 'a' 'b')");
  std::atomic<int> inside{0};
  std::atomic<bool> overlapped{false};
  auto enter = [&](const Ope &, const char *, size_t, const SemanticValues &,
                   const Context &, const std::any &, std::any &) {
    if (inside++ != 0) { overlapped = true; }
    std::this_thread::yield();
    inside--;
  };
  auto leave = [&](const Ope &, const char *, size_t, const SemanticValues &,
                   const Context &, const std::any &, size_t, std::any &) {
    if (inside++ != 0) { overlapped = true; }
    std::this_thread::yield();
    inside--;
  };
  parser.enable_trace(enter, leave);
  run_threads(4, [&](size_t) {
    for (int i = 0; i < 20; i++) {
      parser.parse("ab");
    }
  });
  EXPECT_FALSE(overlapped);
}
