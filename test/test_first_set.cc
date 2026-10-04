#include <gtest/gtest.h>
#include <peglib.h>
#include <sstream>

using namespace peg;

// Basic: alternatives with disjoint first bytes are correctly filtered
TEST(FirstSetTest, Basic_filtering) {
  parser pg(R"(
    S <- 'hello' / 'world' / 'foo'
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("hello"));
  EXPECT_TRUE(pg.parse("world"));
  EXPECT_TRUE(pg.parse("foo"));
  EXPECT_FALSE(pg.parse("bar"));
}

// Case-insensitive literals must include both cases in First-Set
TEST(FirstSetTest, Case_insensitive_literal) {
  parser pg(R"(
    S <- 'SELECT'i / 'INSERT'i / 'UPDATE'i
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("SELECT"));
  EXPECT_TRUE(pg.parse("select"));
  EXPECT_TRUE(pg.parse("Select"));
  EXPECT_TRUE(pg.parse("INSERT"));
  EXPECT_TRUE(pg.parse("insert"));
  EXPECT_TRUE(pg.parse("UPDATE"));
  EXPECT_TRUE(pg.parse("update"));
  EXPECT_FALSE(pg.parse("DELETE"));
}

// Alternatives starting with optional elements (can_be_empty) must not be
// skipped
TEST(FirstSetTest, Can_be_empty_not_skipped) {
  parser pg(R"(
    S <- 'NOT'? [a-z]+
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("NOTabc"));
  EXPECT_TRUE(pg.parse("abc"));
}

// Alternatives starting with AnyCharacter (.) must not be skipped
TEST(FirstSetTest, Any_character_not_skipped) {
  parser pg(R"(
    S <- 'hello' / .
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("hello"));
  EXPECT_TRUE(pg.parse("x"));
  EXPECT_TRUE(pg.parse("h"));
}

// CharacterClass alternatives
TEST(FirstSetTest, Character_class) {
  parser pg(R"(
    S <- [0-9]+ / [a-z]+
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("123"));
  EXPECT_TRUE(pg.parse("abc"));
  EXPECT_FALSE(pg.parse("ABC"));
}

// Negated character class sets any_char (conservative)
TEST(FirstSetTest, Negated_character_class) {
  parser pg(R"(
    S <- 'hello' / [^0-9]+
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("hello"));
  EXPECT_TRUE(pg.parse("abc"));
  EXPECT_FALSE(pg.parse("123"));
}

// Recursive rules: First-Set computation handles cycles
TEST(FirstSetTest, Recursive_rule) {
  parser pg(R"(
    EXPR   <- TERM ('+' TERM)*
    TERM   <- FACTOR ('*' FACTOR)*
    FACTOR <- '(' EXPR ')' / NUMBER
    NUMBER <- [0-9]+
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("1+2*3"));
  EXPECT_TRUE(pg.parse("(1+2)*3"));
  EXPECT_FALSE(pg.parse("+1"));
}

// Many alternatives: the core use case for First-Set performance
TEST(FirstSetTest, Many_alternatives) {
  parser pg(R"(
    S    <- KW
    KW   <- 'alpha' / 'beta' / 'gamma' / 'delta' / 'epsilon'
          / 'zeta' / 'eta' / 'theta' / 'iota' / 'kappa'
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("alpha"));
  EXPECT_TRUE(pg.parse("kappa"));
  EXPECT_TRUE(pg.parse("eta"));
  EXPECT_FALSE(pg.parse("lambda"));
}

// Error message includes expected tokens from skipped alternatives (literals)
TEST(FirstSetTest, Error_message_with_skipped_literals) {
  parser pg(R"(
    S <- 'hello' / 'world'
  )");
  ASSERT_TRUE(!!pg);

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });

  EXPECT_FALSE(pg.parse("xyz"));
  EXPECT_NE(error_msg.find("'hello'"), std::string::npos);
  EXPECT_NE(error_msg.find("'world'"), std::string::npos);
}

// Error message includes expected tokens from skipped alternatives (rule names)
TEST(FirstSetTest, Error_message_with_skipped_rules) {
  parser pg(R"(
    S    <- WORD / NUM
    NUM  <- < [0-9]+ >
    WORD <- < [a-z]+ >
  )");
  ASSERT_TRUE(!!pg);

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });

  EXPECT_FALSE(pg.parse("!!!"));
  // Both token rules should appear in the error message
  EXPECT_NE(error_msg.find("<NUM>"), std::string::npos);
  EXPECT_NE(error_msg.find("<WORD>"), std::string::npos);
}

// Error message: mixed literals and rules
TEST(FirstSetTest, Error_message_mixed) {
  parser pg(R"(
    S <- '(' EXPR ')' / NUM
    EXPR <- NUM
    NUM  <- < [0-9]+ >
  )");
  ASSERT_TRUE(!!pg);

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });

  EXPECT_FALSE(pg.parse("abc"));
  EXPECT_NE(error_msg.find("'('"), std::string::npos);
  EXPECT_NE(error_msg.find("<NUM>"), std::string::npos);
}

// Sequence: First-Set comes from the first element
TEST(FirstSetTest, Sequence_first_element) {
  parser pg(R"(
    S <- 'SELECT' NAME / 'INSERT' NAME
    NAME <- < [a-z]+ >
    %whitespace <- [ ]*
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("SELECT foo"));
  EXPECT_TRUE(pg.parse("INSERT bar"));
  EXPECT_FALSE(pg.parse("DELETE baz"));
}

// Sequence with optional first element: both elements contribute to First-Set
TEST(FirstSetTest, Sequence_optional_first) {
  parser pg(R"(
    S <- 'NOT'? [a-z]+ / [0-9]+
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("NOTabc"));
  EXPECT_TRUE(pg.parse("abc"));
  EXPECT_TRUE(pg.parse("123"));
}

// Macro grammars work correctly with First-Set
TEST(FirstSetTest, Macro_compatibility) {
  parser pg(R"(
    S          <- EXPR
    EXPR       <- INFIX(ATOM, OP)
    INFIX(A,O) <- A (O A)* { precedence L + - L * / }
    ATOM       <- < [0-9]+ >
    OP         <- < '+' / '-' / '*' / '/' >
    %whitespace <- [ ]*
  )");
  ASSERT_TRUE(!!pg);

  int val = 0;
  pg["ATOM"] = [](const SemanticValues &vs) {
    return vs.token_to_number<int>();
  };
  pg["OP"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
  pg["INFIX"] = [](const SemanticValues &vs) {
    auto result = std::any_cast<int>(vs[0]);
    for (auto i = 1u; i < vs.size(); i += 2) {
      auto op = std::any_cast<std::string>(vs[i]);
      auto num = std::any_cast<int>(vs[i + 1]);
      if (op == "+")
        result += num;
      else if (op == "-")
        result -= num;
      else if (op == "*")
        result *= num;
      else if (op == "/")
        result /= num;
    }
    return result;
  };
  pg["EXPR"] = [](const SemanticValues &vs) {
    return std::any_cast<int>(vs[0]);
  };
  pg["S"] = [&](const SemanticValues &vs) { val = std::any_cast<int>(vs[0]); };

  EXPECT_TRUE(pg.parse("1 + 2 * 3"));
  EXPECT_EQ(val, 7);
}

// Packrat + First-Set combination
TEST(FirstSetTest, Packrat_compatibility) {
  parser pg(R"(
    S    <- STMT+
    STMT <- 'if' / 'for' / 'while' / ID
    ID   <- < [a-z]+ >
    %whitespace <- [ \n]*
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();

  EXPECT_TRUE(pg.parse("if for while foo bar"));
  EXPECT_FALSE(pg.parse("123"));
}

// Dictionary operator is not affected by First-Set
TEST(FirstSetTest, Dictionary_operator) {
  parser pg(R"(
    S <- 'alpha' | 'beta' | 'gamma'
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("alpha"));
  EXPECT_TRUE(pg.parse("beta"));
  EXPECT_TRUE(pg.parse("gamma"));
  EXPECT_FALSE(pg.parse("delta"));
}

// First-Set setup shares one visitor across all rules, so a sub-rule referenced
// from many rules has its first-set computed once and reused. Verify the reused
// first-set still filters correctly for every referencing rule.
TEST(FirstSetTest, Shared_subrule_across_rules) {
  parser pg(R"(
    Start <- A / B / C
    A     <- 'a' Term
    B     <- 'b' Term
    C     <- 'c' Term
    Term  <- Num / Name
    Num   <- [0-9]+
    Name  <- [a-z]+
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("a123"));
  EXPECT_TRUE(pg.parse("bxyz"));
  EXPECT_TRUE(pg.parse("c42"));
  EXPECT_FALSE(pg.parse("a@")); // Term cannot start with '@'
  EXPECT_FALSE(pg.parse("d1")); // no 'd' alternative
}

// A left-recursive rule referenced from multiple rules: the shared first-set
// setup must handle the cycle correctly for every referencing rule.
TEST(FirstSetTest, Left_recursive_rule_referenced_from_multiple_rules) {
  parser pg(R"(
    Start <- P / Q
    P     <- 'p' Expr
    Q     <- 'q' Expr
    Expr  <- Expr '+' Num / Num
    Num   <- [0-9]+
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("p1+2+3"));
  EXPECT_TRUE(pg.parse("q9"));
  EXPECT_FALSE(pg.parse("p+1")); // Expr cannot start with '+'
}

// A left-recursive rule that can match empty lets what follows its recursive
// reference start a match: A can start with 'x'.
TEST(FirstSetTest, Left_recursive_rule_that_can_be_empty) {
  parser pg(R"(
    S <- C / 'q'
    C <- A 'z'
    A <- A 'x' / ''
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("z"));
  EXPECT_TRUE(pg.parse("xxz"));
  EXPECT_TRUE(pg.parse("q"));
}

// A cut before the first byte stops the enclosing choice even when A then
// fails, so A must be tried although it cannot start with 'b'.
TEST(FirstSetTest, Leading_cut_is_not_skipped) {
  parser pg(R"(
    S <- A / 'b'
    A <- ↑ 'a'
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("a"));
  EXPECT_FALSE(pg.parse("b"));
}

// =============================================================================
// Unstartable Rule Tests
// =============================================================================

// A rule that cannot start with the next byte is not entered, wherever it is
// used, just as a choice skips an alternative that cannot, where entering it
// would run no callback, so that skipping it goes unnoticed. A parse that
// reports errors still enters it, to tell what it expected.

// How many times the parses from S so far tried the rule, as packrat
// statistics count them.
static size_t tries(parser &pg, const char *name) {
  const auto &stats = pg["S"].packrat_stats_;
  auto id = pg[name].id;
  return id < stats.size() ? stats[id].hits + stats[id].misses : 0;
}

TEST(UnstartableRuleTest, Is_not_entered) {
  parser pg(R"(
    S <- A? 'x'
    A <- 'y'
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;

  EXPECT_TRUE(pg.parse("x"));
  EXPECT_EQ(0u, tries(pg, "A"));
  EXPECT_TRUE(pg.parse("yx"));
  EXPECT_EQ(1u, tries(pg, "A"));
}

TEST(UnstartableRuleTest, Is_entered_when_errors_are_reported) {
  parser pg(R"(
    S <- A? 'x'
    A <- 'y'
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;
  std::string message;
  pg.set_logger([&](size_t, size_t, const std::string &msg) { message = msg; });

  EXPECT_FALSE(pg.parse("z"));
  EXPECT_EQ(1u, tries(pg, "A"));
  EXPECT_EQ("syntax error, unexpected 'z', expecting 'y', 'x'.", message);
}

TEST(UnstartableRuleTest, Is_entered_with_a_nesting_limit) {
  parser pg(R"(
    S <- B
    B <- A? 'x'
    A <- 'y'
  )");
  ASSERT_TRUE(!!pg);

  // Trying A goes one level past the limit, with or without a logger.
  pg.set_max_depth(2);
  EXPECT_FALSE(pg.parse("x"));
}

TEST(UnstartableRuleTest, Is_entered_to_run_its_enter_and_leave) {
  parser pg(R"(
    S <- A? 'x'
    A <- 'y'
  )");
  ASSERT_TRUE(!!pg);

  auto enters = 0;
  auto leaves = 0;
  pg["A"].enter = [&](const Context &, const char *, size_t, std::any &) {
    enters++;
  };
  pg["A"].leave = [&](const Context &, const char *, size_t, size_t, std::any &,
                      std::any &) { leaves++; };

  EXPECT_TRUE(pg.parse("x"));
  EXPECT_EQ(1, enters);
  EXPECT_EQ(1, leaves);
}

// E matches empty before R fails on 'y', also where R enters it through Q,
// and G reads what its action did, so skipping R would make the parse fail.
TEST(UnstartableRuleTest, Is_entered_to_run_an_action_below_it) {
  for (auto grammar : {
           R"(R <- E 'x')",
           R"(R <- Q 'x'
              Q <- E 'z')",
       }) {
    for (auto with_logger : {false, true}) {
      parser pg(R"(
        S <- R? 'y' G
        E <- ''
        G <- 'g'
      )" + std::string(grammar));
      ASSERT_TRUE(!!pg);

      auto count = 0;
      pg["E"] = [&](const SemanticValues &) { count++; };
      pg["G"].predicate = [&](const SemanticValues &, const std::any &,
                              std::string &) { return count > 0; };
      if (with_logger) {
        pg.set_logger([](size_t, size_t, const std::string &) {});
      }

      EXPECT_TRUE(pg.parse("yg")) << grammar << with_logger;
      EXPECT_EQ(1, count) << grammar << with_logger;
    }
  }
}

// A lookahead may match anything: K matches 'x' inside A.
TEST(UnstartableRuleTest, Is_entered_to_run_an_action_below_a_lookahead) {
  parser pg(R"(
    S <- A? 'x'
    A <- !K 'y'
    K <- 'x'
  )");
  ASSERT_TRUE(!!pg);

  auto count = 0;
  pg["K"] = [&](const SemanticValues &) { count++; };

  EXPECT_TRUE(pg.parse("x"));
  EXPECT_EQ(1, count);
}

// The whitespace after 'z' is skipped inside A's lookahead, and again after
// S's 'z'.
TEST(UnstartableRuleTest, Is_entered_to_run_an_action_of_the_whitespace) {
  parser pg(R"(
    S <- A? 'z'
    A <- &'z' 'y'
    %whitespace <- W*
    W <- ' '
  )");
  ASSERT_TRUE(!!pg);

  auto count = 0;
  pg["W"] = [&](const SemanticValues &) { count++; };

  EXPECT_TRUE(pg.parse("z  "));
  EXPECT_EQ(4, count);
}

// W's enter makes S enter A. X, parsed from an action of B, skips no
// whitespace and may skip A, but that holds for X's parses alone.
TEST(UnstartableRuleTest, Is_entered_after_a_nested_parse_that_skips_it) {
  parser pg(R"(
    S <- B A? 'x'
    A <- '' 'y'
    B <- 'b'
    X <- A? 'z'
    %whitespace <- W*
    W <- ' '
  )");
  ASSERT_TRUE(!!pg);

  auto enters = 0;
  pg["W"].enter = [&](const Context &, const char *, size_t, std::any &) {
    enters++;
  };
  pg["B"] = [&](const SemanticValues &) {
    EXPECT_TRUE(pg["X"].parse("z").ret);
  };

  EXPECT_TRUE(pg.parse("bx"));
  EXPECT_EQ(4, enters);
}

// An AST action depends on its values alone, so running it or not goes
// unnoticed.
TEST(UnstartableRuleTest, Is_not_entered_for_an_ast_action_below_it) {
  parser pg(R"(
    S <- R? 'y'
    R <- E 'x'
    E <- ''
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast(true);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("y", ast));
  EXPECT_EQ(0u, tries(pg, "R"));
}

// A precedence rule whose atom matches empty can start with an operator.
TEST(FirstSetTest, Operator_after_an_empty_atom) {
  for (auto start : {"S <- X / 'q'", "S <- X?"}) {
    parser pg(std::string(start) + R"(
      X <- E 'x'
      E <- T (O T)* {
        precedence
          L +
      }
      T <- 't'?
      O <- '+'
    )");
    ASSERT_TRUE(!!pg) << start;

    EXPECT_TRUE(pg.parse("+x")) << start;
    EXPECT_TRUE(pg.parse("t+tx")) << start;
  }
}

// A literal, a token boundary and a no_whitespace rule skip whitespace after
// their match, even an empty one, so the whitespace can start what follows
// them. [a] skips no whitespace, so T starts at the space.
TEST(FirstSetTest, Whitespace_after_an_empty_match) {
  for (auto grammar : {
           R"(S <- [a] T
              T <- 'y' / '' 'x')",
           R"(S <- [a] T
              T <- 'y' / < 'z'? > 'x')",
           R"(S <- [a] T
              T <- 'y' / E 'x'
              E <- '')",
           R"(S <- [a] T
              T <- 'y' / N 'x'
              N <- 'z'? { no_whitespace })",
       }) {
    parser pg(std::string(grammar) + "\n%whitespace <- [ ]*\n");
    ASSERT_TRUE(!!pg) << grammar;

    EXPECT_TRUE(pg.parse("ax")) << grammar;
    EXPECT_TRUE(pg.parse("a x")) << grammar;
  }
}

// A cut in a lookahead reaches the choice around the rule, which then tries
// no other alternative.
TEST(UnstartableRuleTest, Is_entered_to_run_a_cut_in_a_lookahead) {
  parser pg(R"(
    S <- 'x' R / 'x' 'c'
    R <- !E 'b'
    E <- ↑ 'a'
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_FALSE(pg.parse("xc"));
  pg.set_logger([](size_t, size_t, const std::string &) {});
  EXPECT_FALSE(pg.parse("xc"));
}

// The same holds where a rule that cannot start with the next byte is not
// entered.
TEST(UnstartableRuleTest, Is_entered_at_whitespace_after_an_empty_match) {
  parser pg(R"(
    S <- [a] U
    U <- '' 'x'
    %whitespace <- [ ]*
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("ax"));
  EXPECT_TRUE(pg.parse("a x"));
}
