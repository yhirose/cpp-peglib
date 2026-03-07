#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// =============================================================================
// Packrat Parsing Tests
// =============================================================================

TEST(PackratTest, Packrat_parser_test_with_whitespace) {
  peg::parser parser(R"(
        ROOT         <-  'a'
        %whitespace  <-  SPACE*
        SPACE        <-  ' '
    )");

  parser.enable_packrat_parsing();

  auto ret = parser.parse("a");
  EXPECT_TRUE(ret);
}

TEST(PackratTest, Packrat_parser_test_with_macro) {
  parser parser(R"(
        EXPRESSION       <-  _ LIST(TERM, TERM_OPERATOR)
        TERM             <-  LIST(FACTOR, FACTOR_OPERATOR)
        FACTOR           <-  NUMBER / T('(') EXPRESSION T(')')
        TERM_OPERATOR    <-  T([-+])
        FACTOR_OPERATOR  <-  T([*/])
        NUMBER           <-  T([0-9]+)
		~_               <-  [ \t]*
		LIST(I, D)       <-  I (D I)*
		T(S)             <-  < S > _
	)");

  parser.enable_packrat_parsing();

  auto ret = parser.parse(" 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ");
  EXPECT_TRUE(ret);
}

TEST(PackratTest, Packrat_parser_test_with_precedence_expression_parser) {
  peg::parser parser(R"(
    Expression  <- Atom (Operator Atom)* { precedence L + - L * / }
    Atom        <- _? Number _?
    Number      <- [0-9]+
    Operator    <- '+' / '-' / '*' / '/'
    _           <- ' '+
  )");

  bool ret = parser;
  EXPECT_TRUE(ret);

  parser.enable_packrat_parsing();

  ret = parser.parse(" 1 + 2 * 3 ");
  EXPECT_TRUE(ret);
}

// =============================================================================
// Packrat Correctness Tests
// =============================================================================

TEST(PackratTest, Packrat_correctness_same_result) {
  auto grammar = R"(
    S    <- A / B
    A    <- X 'a'
    B    <- X 'b'
    X    <- 'x' 'y'?
  )";

  // Without packrat
  parser pg1(grammar);
  EXPECT_TRUE(pg1);

  // With packrat
  parser pg2(grammar);
  pg2.enable_packrat_parsing();
  EXPECT_TRUE(pg2);

  std::vector<std::string> inputs = {"xa", "xb", "xya", "xyb", "xc", ""};
  for (const auto &input : inputs) {
    EXPECT_EQ(pg1.parse(input.c_str()), pg2.parse(input.c_str()))
        << "Mismatch for input: '" << input << "'";
  }
}

TEST(PackratTest, Packrat_with_semantic_actions) {
  auto grammar = R"(
    EXPR <- TERM (('+' / '-') TERM)*
    TERM <- < [0-9]+ >
  )";

  // Without packrat
  parser pg1(grammar);
  EXPECT_TRUE(pg1);

  // With packrat
  parser pg2(grammar);
  pg2.enable_packrat_parsing();
  EXPECT_TRUE(pg2);

  // Verify parse results match with and without packrat
  std::vector<std::string> inputs = {"1+2-3", "1+2", "5", "1-2+3-4"};
  for (const auto &input : inputs) {
    EXPECT_EQ(pg1.parse(input.c_str()), pg2.parse(input.c_str()))
        << "Mismatch for input: '" << input << "'";
  }
}

TEST(PackratTest, Packrat_cache_with_choice_backtracking) {
  parser pg(R"(
    S <- A B / A C
    A <- 'hello'
    B <- 'world'
    C <- 'there'
    %whitespace <- [ ]*
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  // A is parsed once due to packrat caching, result reused for second attempt
  size_t a_count = 0;
  pg["A"] = [&](const SemanticValues &) {
    a_count++;
    return std::string("hello");
  };

  EXPECT_TRUE(pg.parse("hello there"));
  EXPECT_EQ(1, a_count); // Packrat should cache A's result
}

// =============================================================================
// Lookahead Predicate Tests
