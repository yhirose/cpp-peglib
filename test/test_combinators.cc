#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// Helper: Definition::parse returns Result, extract .ret for EXPECT
static bool def_parse(const Definition &def, const char *s) {
  return def.parse(s, strlen(s)).ret;
}

// --- opt() ---

TEST(CombinatorTest, Opt_matches_present) {
  Definition ROOT;
  ROOT <= seq(lit("hello"), opt(lit(" world")));
  EXPECT_TRUE(def_parse(ROOT, "hello world"));
}

TEST(CombinatorTest, Opt_matches_absent) {
  Definition ROOT;
  ROOT <= seq(lit("hello"), opt(lit(" world")));
  EXPECT_TRUE(def_parse(ROOT, "hello"));
}

// --- rep() ---

TEST(CombinatorTest, Rep_exact_range) {
  Definition ROOT;
  ROOT <= rep(chr('a'), 2, 4);
  EXPECT_FALSE(def_parse(ROOT, "a"));
  EXPECT_TRUE(def_parse(ROOT, "aa"));
  EXPECT_TRUE(def_parse(ROOT, "aaa"));
  EXPECT_TRUE(def_parse(ROOT, "aaaa"));
}

TEST(CombinatorTest, Rep_min_only) {
  Definition ROOT;
  ROOT <= rep(chr('x'), 1, std::numeric_limits<size_t>::max());
  EXPECT_FALSE(def_parse(ROOT, ""));
  EXPECT_TRUE(def_parse(ROOT, "x"));
  EXPECT_TRUE(def_parse(ROOT, "xxx"));
}

// --- apd() (And Predicate) ---

TEST(CombinatorTest, Apd_lookahead_success) {
  Definition ROOT;
  ROOT <= seq(apd(seq(chr('a'), chr('b'))), chr('a'), chr('b'));
  EXPECT_TRUE(def_parse(ROOT, "ab"));
}

TEST(CombinatorTest, Apd_lookahead_failure) {
  Definition ROOT;
  ROOT <= seq(apd(seq(chr('a'), chr('b'))), chr('a'), chr('c'));
  EXPECT_FALSE(def_parse(ROOT, "ac"));
}

// --- npd() (Not Predicate) ---

TEST(CombinatorTest, Npd_negative_lookahead_success) {
  Definition ROOT;
  ROOT <= seq(npd(seq(chr('a'), chr('b'))), chr('a'), chr('c'));
  EXPECT_TRUE(def_parse(ROOT, "ac"));
}

TEST(CombinatorTest, Npd_negative_lookahead_failure) {
  Definition ROOT;
  ROOT <= seq(npd(seq(chr('a'), chr('b'))), chr('a'), chr('b'));
  EXPECT_FALSE(def_parse(ROOT, "ab"));
}

// --- dic() (Dictionary) ---

TEST(CombinatorTest, Dic_matches_keyword) {
  Definition ROOT;
  ROOT <= dic({"apple", "banana", "cherry"}, false);
  EXPECT_TRUE(def_parse(ROOT, "apple"));
  EXPECT_TRUE(def_parse(ROOT, "banana"));
  EXPECT_TRUE(def_parse(ROOT, "cherry"));
  EXPECT_FALSE(def_parse(ROOT, "grape"));
}

TEST(CombinatorTest, Dic_case_insensitive) {
  Definition ROOT;
  ROOT <= dic({"hello", "world"}, true);
  EXPECT_TRUE(def_parse(ROOT, "HELLO"));
  EXPECT_TRUE(def_parse(ROOT, "World"));
}

// --- lit() / liti() ---

TEST(CombinatorTest, Lit_exact_match) {
  Definition ROOT;
  ROOT <= lit("hello");
  EXPECT_TRUE(def_parse(ROOT, "hello"));
  EXPECT_FALSE(def_parse(ROOT, "Hello"));
  EXPECT_FALSE(def_parse(ROOT, "world"));
}

TEST(CombinatorTest, Liti_case_insensitive) {
  Definition ROOT;
  ROOT <= liti("hello");
  EXPECT_TRUE(def_parse(ROOT, "hello"));
  EXPECT_TRUE(def_parse(ROOT, "HELLO"));
  EXPECT_TRUE(def_parse(ROOT, "HeLLo"));
  EXPECT_FALSE(def_parse(ROOT, "world"));
}

// --- ncls() (Negated Character Class) ---

TEST(CombinatorTest, Ncls_negated_class_string) {
  Definition ROOT;
  ROOT <= oom(ncls("abc"));
  EXPECT_TRUE(def_parse(ROOT, "xyz"));
  EXPECT_FALSE(def_parse(ROOT, "abc"));
  EXPECT_FALSE(def_parse(ROOT, "a"));
}

TEST(CombinatorTest, Ncls_negated_class_ranges) {
  Definition ROOT;
  std::vector<std::pair<char32_t, char32_t>> ranges = {{'0', '9'}};
  ROOT <= oom(ncls(ranges));
  EXPECT_TRUE(def_parse(ROOT, "abc"));
  EXPECT_FALSE(def_parse(ROOT, "123"));
}

// --- csc() / cap() (Capture Scope / Capture) ---

TEST(CombinatorTest, Cap_capture_match) {
  Definition ROOT;
  std::string captured;
  ROOT <= cap(oom(cls("a-z")), [&](const char *s, size_t n, Context &) {
    captured = std::string(s, n);
  });
  EXPECT_TRUE(def_parse(ROOT, "hello"));
  EXPECT_EQ(captured, "hello");
}

TEST(CombinatorTest, Csc_capture_scope_with_backreference) {
  // Test capture scope with backreference: opening quote must match closing
  parser parser(R"(
    ROOT   <- QUOTED
    QUOTED <- $q< ['"'] > [a-z]+ $q
  )");
  ASSERT_TRUE(parser);
  EXPECT_TRUE(parser.parse("'hello'"));
  EXPECT_TRUE(parser.parse("\"hello\""));
  EXPECT_FALSE(parser.parse("'hello\""));
}

// --- tok() (Token Boundary) ---

TEST(CombinatorTest, Tok_token_boundary) {
  Definition ROOT;
  ROOT <= seq(tok(oom(cls("a-z"))), tok(oom(cls("a-z"))));
  ROOT.whitespaceOpe = zom(cls(" \t"));

  std::string val;
  ROOT = [&](const SemanticValues &vs) {
    val = std::string(vs.token_to_string(0)) + " " +
          std::string(vs.token_to_string(1));
  };
  EXPECT_TRUE(def_parse(ROOT, "hello world"));
  EXPECT_EQ(val, "hello world");
}

// --- ign() (Ignore) ---

TEST(CombinatorTest, Ign_ignore_semantic_value) {
  Definition ROOT;
  ROOT <= seq(lit("hello"), ign(lit(" ")), lit("world"));
  EXPECT_TRUE(def_parse(ROOT, "hello world"));
}

// --- bkr() (Back Reference) ---

TEST(CombinatorTest, Bkr_back_reference) {
  parser parser(R"(
    ROOT <- $tag< WORD > ' ' WORD ' ' $tag
    WORD <- [a-z]+
  )");
  ASSERT_TRUE(parser);
  EXPECT_TRUE(parser.parse("hello world hello"));
  EXPECT_FALSE(parser.parse("hello world goodbye"));
}

// --- pre() (Precedence Climbing) ---

TEST(CombinatorTest, Pre_precedence_climbing) {
  parser parser(R"(
    EXPRESSION                  <- ATOM (BINOP ATOM)* {
                                     precedence
                                       L + -
                                       L * /
                                   }
    ATOM                        <- NUMBER
    BINOP                       <- < '+' / '-' / '*' / '/' >
    NUMBER                      <- < [0-9]+ >
    %whitespace                 <- [ \t]*
  )");
  ASSERT_TRUE(parser);

  parser["EXPRESSION"] = [](const SemanticValues &vs) -> int {
    if (vs.size() == 1) { return std::any_cast<int>(vs[0]); }
    auto left = std::any_cast<int>(vs[0]);
    auto right = std::any_cast<int>(vs[2]);
    auto op = std::any_cast<std::string_view>(vs[1]);
    if (op == "+") return left + right;
    if (op == "-") return left - right;
    if (op == "*") return left * right;
    if (op == "/") return left / right;
    return 0;
  };
  parser["BINOP"] = [](const SemanticValues &vs) { return vs.token(); };
  parser["NUMBER"] = [](const SemanticValues &vs) {
    return std::stoi(std::string(vs.token()));
  };

  int result;
  EXPECT_TRUE(parser.parse("2+3*4", result));
  EXPECT_EQ(result, 14); // 2 + (3 * 4) = 14
}

// --- rec() (Recovery) ---

TEST(CombinatorTest, Rec_recovery) {
  parser parser(R"(
    PROGRAM  <- STMT+
    STMT     <- EXPR ';'
    EXPR     <- 'ok' / %recover([^;]*)
    %whitespace <- [ \t\n]*
  )");
  ASSERT_TRUE(parser);

  std::vector<std::string> errors;
  parser.set_logger([&](size_t, size_t, const std::string &msg,
                        const std::string &) { errors.push_back(msg); });

  auto result = parser.parse("ok; bad; ok;");
  EXPECT_FALSE(result);
  EXPECT_FALSE(errors.empty());
}

// --- cut() ---

TEST(CombinatorTest, Cut_operator) {
  parser parser(R"(
    ROOT  <- A / B
    A     <- 'a' ↑ 'b'
    B     <- 'a' 'c'
  )");
  ASSERT_TRUE(parser);

  EXPECT_TRUE(parser.parse("ab"));
  EXPECT_FALSE(parser.parse("ac"));
}
