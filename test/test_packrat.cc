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

TEST(PackratTest, Packrat_cache_across_shared_consuming_prefix) {
  parser pg(R"(
    S <- '(' A ',' A ')' / '(' A ',' ')'
    A <- 'a'
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  size_t a_count = 0;
  pg["A"] = [&](const SemanticValues &) {
    a_count++;
    return std::string("a");
  };

  // The first alternative matches A, fails at the second one, and the second
  // alternative queries A again at the same position. The shared `'('` in
  // front of it must not hide that from the packrat filter.
  EXPECT_TRUE(pg.parse("(a,)"));
  EXPECT_EQ(1, a_count);
}

TEST(PackratTest, Packrat_shared_consuming_prefix_is_not_exponential) {
  parser pg(R"(
    S <- '(' S ',' S ')' / '(' S ',' ')' / 'x'
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  // Each level re-parses S from the second alternative, so an unmemoized S
  // costs 2^depth. Completing at all is the assertion.
  std::string input;
  for (auto i = 0; i < 100; i++) {
    input += '(';
  }
  input += 'x';
  for (auto i = 0; i < 100; i++) {
    input += ",)";
  }
  EXPECT_TRUE(pg.parse(input));
}

TEST(PackratTest, Packrat_after_another_start_rule_reassigned_ids) {
  parser pg(R"(
    Top  <- (W / Expr) ';' / W '!'
    W    <- 'w'
    Expr <- Expr '+' Num / Num
    Num  <- [0-9]+
    Sub  <- A B C D E Expr
    A <- 'a'
    B <- 'b'
    C <- 'c'
    D <- 'd'
    E <- 'e'
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("1+2;"));
  EXPECT_TRUE(pg.get_grammar().at("Sub").parse("abcde1+2").ret);
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg.parse("1+2;"));
}

TEST(PackratTest, Packrat_after_another_start_rule_gave_two_rules_one_id) {
  parser pg(R"(
    Top  <- W 'x' / W 'y' / Expr ';'
    W    <- [0-9a-z]+ '!'
    Expr <- Num '+' Num / Num
    Num  <- [0-9]+
    Sub  <- Expr
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("1+2;"));

  const auto &g = pg.get_grammar();
  EXPECT_TRUE(g.at("Sub").parse("1+2").ret);

  EXPECT_TRUE(pg.parse("1+2;"));
}

TEST(PackratTest, Packrat_parse_nested_in_an_action_keeps_the_outer_ids) {
  parser pg(R"(
    Top  <- A (W 'x' / W 'y' / Expr ';')
    A    <- 'a'
    W    <- [0-9a-z]+ '!'
    Expr <- Num '+' Num / Num
    Num  <- [0-9]+
    Sub  <- P Expr / Expr
    P    <- 'p'
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  const auto &g = pg.get_grammar();
  auto nested_ok = false;
  pg["A"] = [&](const SemanticValues &) {
    nested_ok = g.at("Sub").parse("1+2").ret;
  };

  EXPECT_TRUE(pg.parse("a1+2;"));
  EXPECT_TRUE(nested_ok);
  EXPECT_NE(g.at("Expr").id, g.at("W").id);

  EXPECT_TRUE(g.at("Sub").parse("1+2").ret);
}

// B is tried twice at the start, where it captures 'a'. Memoized, the second
// try would not capture it again after the first try's capture was rolled
// back, and the $o at the end would have nothing to match.
TEST(PackratTest, Packrat_keeps_the_captures_of_a_retried_rule) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S <- B 'x' / B 'y' B
      B <- $o<[a-z]> / '=' $o
    )");
    ASSERT_TRUE(!!pg);
    if (packrat) { pg.enable_packrat_parsing(); }

    EXPECT_TRUE(pg.parse("ay=a")) << packrat;
    EXPECT_FALSE(pg.parse("ay=b")) << packrat;
  }
}

// The '+' after "1*2" ends the inner level and is parsed again at the outer
// one, which has to capture it again.
TEST(PackratTest, Packrat_keeps_the_captures_of_a_reparsed_operator) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S     <- BINOP 'z' / BINOP 'y' / EXPR ';' BINOP
      EXPR  <- ATOM (BINOP ATOM)* {
        precedence
          L +
          L *
      }
      ATOM  <- [0-9]
      BINOP <- $o<[-+*]> / '=' $o
    )");
    ASSERT_TRUE(!!pg);
    if (packrat) { pg.enable_packrat_parsing(); }

    EXPECT_TRUE(pg.parse("1*2+3;=+")) << packrat;
    EXPECT_FALSE(pg.parse("1*2+3;=*")) << packrat;
  }
}

// The whitespace after 'a' captures w, so every rule that skips whitespace
// captures, B included, although its body does not. Memoized, B's second try
// would not capture w again after the first try's capture was rolled back.
TEST(PackratTest, Packrat_keeps_the_captures_of_the_whitespace) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S <- B 'x' / B 'y'
      B <- 'a'
      %whitespace <- $w<[ ]+> / [|] $w / ''
    )");
    ASSERT_TRUE(!!pg);
    if (packrat) { pg.enable_packrat_parsing(); }

    EXPECT_TRUE(pg.parse("a  y|  ")) << packrat;
    EXPECT_FALSE(pg.parse("a  y| ")) << packrat;
  }
}

// =============================================================================
// Lookahead Predicate Tests
