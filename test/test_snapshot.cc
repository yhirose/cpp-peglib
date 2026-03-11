#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// =============================================================================
// Phase 2 Snapshot/Rollback Validation Tests
//
// These tests verify behaviors that Phase 2 must preserve:
// - Nested choice rollback of choice_count_/choice_
// - Sequence direct write (append removal)
// - Predicate side-effect isolation
// - CaptureScope isolation
// - Repetition partial rollback
// - Capture rollback on choice failure
// =============================================================================

TEST(SnapshotTest, Nested_choice_rollback) {
  parser pg(R"(
    S      <- A / B
    A      <- 'x' INNER 'y'
    B      <- 'x' 'z'
    INNER  <- 'a' / 'b' / 'c'
  )");
  EXPECT_TRUE(pg);
  // A fails ('y' not found) -> fallback to B -> choice=1
  pg["S"] = [](const SemanticValues &vs) {
    EXPECT_EQ(2u, vs.choice_count()); // S has 2 alternatives
    EXPECT_EQ(1u, vs.choice());       // B (0-indexed)
  };
  EXPECT_TRUE(pg.parse("xz"));
}

TEST(SnapshotTest, Sequence_direct_write) {
  parser pg(R"(
    S <- A B C
    A <- 'aaa'
    B <- 'bbb'
    C <- 'ccc'
  )");
  EXPECT_TRUE(pg);
  pg["A"] = [](const SemanticValues & /*vs*/) { return 1; };
  pg["B"] = [](const SemanticValues & /*vs*/) { return 2; };
  pg["C"] = [](const SemanticValues & /*vs*/) { return 3; };
  pg["S"] = [](const SemanticValues &vs) {
    EXPECT_EQ(3u, vs.size());
    EXPECT_EQ(1, std::any_cast<int>(vs[0]));
    EXPECT_EQ(2, std::any_cast<int>(vs[1]));
    EXPECT_EQ(3, std::any_cast<int>(vs[2]));
  };
  EXPECT_TRUE(pg.parse("aaabbbccc"));
}

TEST(SnapshotTest, Predicate_no_side_effect) {
  parser pg(R"(
    S  <- &(AB) CD
    AB <- [a-z]+ [0-9]+
    CD <- [a-z]+ [0-9]+
  )");
  EXPECT_TRUE(pg);
  pg["AB"] = [](const SemanticValues & /*vs*/) { return std::string("AB"); };
  pg["CD"] = [](const SemanticValues & /*vs*/) { return std::string("CD"); };
  // &(AB) succeeds but its result must not leak into S's semantic values
  pg["S"] = [](const SemanticValues &vs) {
    EXPECT_EQ(1u, vs.size()); // Only CD's result
    EXPECT_EQ("CD", std::any_cast<std::string>(vs[0]));
  };
  EXPECT_TRUE(pg.parse("abc123"));
}

TEST(SnapshotTest, CaptureScope_isolation) {
  parser pg(R"(
    S        <- $ref<'hello'> $(ISOLATED) $ref
    ISOLATED <- $ref<'world'>
  )");
  EXPECT_TRUE(pg);
  // $ref='world' inside $(ISOLATED) is isolated by CaptureScope
  // The final $ref should match 'hello', not 'world'
  EXPECT_TRUE(pg.parse("helloworldhello"));
  EXPECT_FALSE(pg.parse("helloworldworld"));
}

TEST(SnapshotTest, Repetition_partial_rollback) {
  parser pg(R"(
    S    <- ITEM+
    ITEM <- < [a-z]+ > ' '?
  )");
  EXPECT_TRUE(pg);
  pg["ITEM"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
  pg["S"] = [](const SemanticValues &vs) {
    EXPECT_EQ(3u, vs.size());
    EXPECT_EQ("foo", std::any_cast<std::string>(vs[0]));
    EXPECT_EQ("bar", std::any_cast<std::string>(vs[1]));
    EXPECT_EQ("baz", std::any_cast<std::string>(vs[2]));
  };
  EXPECT_TRUE(pg.parse("foo bar baz"));
}

TEST(SnapshotTest, Capture_rollback_on_choice_failure) {
  parser pg(R"(
    S <- A / B
    A <- $ref<'xx'> 'FAIL'
    B <- $ref<'yy'> $ref
  )");
  EXPECT_TRUE(pg);
  // A: sets ref='xx' -> fails on 'FAIL' -> rollback discards ref='xx'
  // B: sets ref='yy' -> $ref expects 'yy'
  EXPECT_TRUE(pg.parse("yyyy"));
  EXPECT_FALSE(pg.parse("yyxx")); // Would wrongly succeed if ref='xx' leaked
}
