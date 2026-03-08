#include <gtest/gtest.h>
#include <peglib.h>

#include <sstream>

using namespace peg;

TEST(TraceTest, Enable_trace_enter_leave_callbacks) {
  parser parser(R"(
    ROOT <- 'hello' ' ' 'world'
  )");
  ASSERT_TRUE(parser);

  int enter_count = 0;
  int leave_count = 0;

  parser.enable_trace([&](auto &, auto, auto, auto &, auto &, auto &,
                          auto &) { enter_count++; },
                      [&](auto &, auto, auto, auto &, auto &, auto &, auto,
                          auto &) { leave_count++; });

  EXPECT_TRUE(parser.parse("hello world"));
  EXPECT_GT(enter_count, 0);
  EXPECT_GT(leave_count, 0);
  EXPECT_EQ(enter_count, leave_count);
}

TEST(TraceTest, Enable_trace_with_start_end_callbacks) {
  parser parser(R"(
    ROOT <- 'a' / 'b'
  )");
  ASSERT_TRUE(parser);

  bool start_called = false;
  bool end_called = false;

  parser.enable_trace(
      [&](auto &, auto, auto, auto &, auto &, auto &, auto &) {},
      [&](auto &, auto, auto, auto &, auto &, auto &, auto, auto &) {},
      [&](auto &) { start_called = true; }, [&](auto &) { end_called = true; });

  EXPECT_TRUE(parser.parse("a"));
  EXPECT_TRUE(start_called);
  EXPECT_TRUE(end_called);
}

TEST(TraceTest, Trace_data_passing) {
  parser parser(R"(
    ROOT <- 'test'
  )");
  ASSERT_TRUE(parser);

  // trace_data is initialized by tracer_start, then copied into Context.
  // tracer_enter/leave modify Context's copy; tracer_end sees the original.
  int enter_count = 0;
  bool start_called = false;
  bool end_called = false;

  parser.enable_trace(
      [&](auto &, auto, auto, auto &, auto &, auto &, std::any &trace_data) {
        // Verify trace_data was initialized (copied from tracer_start's value)
        auto val = std::any_cast<int>(trace_data);
        trace_data = val + 1;
        enter_count++;
      },
      [&](auto &, auto, auto, auto &, auto &, auto &, auto, std::any &) {},
      [&](std::any &trace_data) {
        trace_data = 42;
        start_called = true;
      },
      [&](std::any &trace_data) {
        // tracer_end sees the original trace_data (not Context's copy)
        EXPECT_EQ(std::any_cast<int>(trace_data), 42);
        end_called = true;
      });

  EXPECT_TRUE(parser.parse("test"));
  EXPECT_TRUE(start_called);
  EXPECT_TRUE(end_called);
  EXPECT_GT(enter_count, 0);
}

TEST(TraceTest, Trace_on_parse_failure) {
  parser parser(R"(
    ROOT <- 'hello'
  )");
  ASSERT_TRUE(parser);

  int enter_count = 0;
  int leave_success = 0;
  int leave_fail = 0;

  parser.enable_trace(
      [&](auto &, auto, auto, auto &, auto &, auto &, auto &) {
        enter_count++;
      },
      [&](auto &, auto, auto, auto &, auto &, auto &, auto len, auto &) {
        if (len != static_cast<size_t>(-1)) {
          leave_success++;
        } else {
          leave_fail++;
        }
      });

  EXPECT_FALSE(parser.parse("world"));
  EXPECT_GT(enter_count, 0);
  EXPECT_GT(leave_fail, 0);
}

TEST(TraceTest, Trace_position_in_callback) {
  parser parser(R"(
    ROOT <- 'ab' 'cd'
  )");
  ASSERT_TRUE(parser);

  std::vector<size_t> positions;

  parser.enable_trace(
      [&](auto &, auto s, auto, auto &, auto &c, auto &, auto &) {
        positions.push_back(static_cast<size_t>(s - c.s));
      },
      [&](auto &, auto, auto, auto &, auto &, auto &, auto, auto &) {});

  EXPECT_TRUE(parser.parse("abcd"));
  EXPECT_FALSE(positions.empty());
  // First position should be 0 (start of input)
  EXPECT_EQ(positions[0], 0u);
}

TEST(TraceTest, Set_verbose_trace) {
  parser parser(R"(
    ROOT <- 'hello'
  )");
  ASSERT_TRUE(parser);

  // Just verify it doesn't crash — verbose_trace affects is_traceable
  parser.set_verbose_trace(true);

  int enter_count = 0;
  parser.enable_trace(
      [&](auto &, auto, auto, auto &, auto &, auto &, auto &) {
        enter_count++;
      },
      [&](auto &, auto, auto, auto &, auto &, auto &, auto, auto &) {});

  EXPECT_TRUE(parser.parse("hello"));
  // With verbose trace, more operations should be traced
  int verbose_count = enter_count;

  enter_count = 0;
  parser.set_verbose_trace(false);
  EXPECT_TRUE(parser.parse("hello"));
  int non_verbose_count = enter_count;

  EXPECT_GE(verbose_count, non_verbose_count);
}

TEST(TraceTest, Enable_tracing_helper) {
  parser parser(R"(
    ROOT <- GREETING
    GREETING <- 'hello' ' ' 'world'
  )");
  ASSERT_TRUE(parser);

  std::ostringstream os;
  enable_tracing(parser, os);

  EXPECT_TRUE(parser.parse("hello world"));

  auto output = os.str();
  EXPECT_FALSE(output.empty());
  // Should contain Enter and Leave markers
  EXPECT_NE(output.find("E "), std::string::npos);
  EXPECT_NE(output.find("L "), std::string::npos);
}

TEST(TraceTest, Enable_tracing_on_failure) {
  parser parser(R"(
    ROOT <- 'hello'
  )");
  ASSERT_TRUE(parser);

  std::ostringstream os;
  enable_tracing(parser, os);

  EXPECT_FALSE(parser.parse("world"));

  auto output = os.str();
  EXPECT_FALSE(output.empty());
}

TEST(TraceTest, Enable_profiling_helper) {
  parser parser(R"(
    ROOT <- NUMBER ('+' NUMBER)*
    NUMBER <- [0-9]+
  )");
  ASSERT_TRUE(parser);

  std::ostringstream os;
  enable_profiling(parser, os);

  EXPECT_TRUE(parser.parse("1+2+3"));

  auto output = os.str();
  EXPECT_FALSE(output.empty());
  // Should contain duration info
  EXPECT_NE(output.find("duration:"), std::string::npos);
  // Should contain rule names
  EXPECT_NE(output.find("ROOT"), std::string::npos);
  EXPECT_NE(output.find("NUMBER"), std::string::npos);
}

TEST(TraceTest, Enable_profiling_shows_success_fail_counts) {
  parser parser(R"(
    ROOT <- ITEM+
    ITEM <- 'a' / 'b'
  )");
  ASSERT_TRUE(parser);

  std::ostringstream os;
  enable_profiling(parser, os);

  EXPECT_TRUE(parser.parse("ab"));

  auto output = os.str();
  // Should contain success/fail summary
  EXPECT_NE(output.find("success"), std::string::npos);
  EXPECT_NE(output.find("fail"), std::string::npos);
}

TEST(TraceTest, Trace_with_packrat) {
  parser parser(R"(
    ROOT <- A / B
    A <- 'x' 'y'
    B <- 'x' 'z'
  )");
  ASSERT_TRUE(parser);
  parser.enable_packrat_parsing();

  int enter_count = 0;
  parser.enable_trace(
      [&](auto &, auto, auto, auto &, auto &, auto &, auto &) {
        enter_count++;
      },
      [&](auto &, auto, auto, auto &, auto &, auto &, auto, auto &) {});

  EXPECT_TRUE(parser.parse("xz"));
  EXPECT_GT(enter_count, 0);
}

TEST(TraceTest, Trace_with_left_recursion) {
  parser parser(R"(
    E <- E '+' T / T
    T <- [0-9]+
  )");
  ASSERT_TRUE(parser);

  int enter_count = 0;
  parser.enable_trace(
      [&](auto &, auto, auto, auto &, auto &, auto &, auto &) {
        enter_count++;
      },
      [&](auto &, auto, auto, auto &, auto &, auto &, auto, auto &) {});

  EXPECT_TRUE(parser.parse("1+2"));
  EXPECT_GT(enter_count, 0);
}
