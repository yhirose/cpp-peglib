#include <chrono>
#include <gtest/gtest.h>
#include <peglib.h>
#include <sstream>
#include <thread>

#include "grammar_file.h"

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

// Alternatives starting with optional elements must not be
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

// Each level of the chain starts with the next one in both alternatives,
// and the last is left-recursive, so every walk into a level meets a rule
// again. The first sets are still worked out once per rule: once per path to
// a rule, loading took time doubling with each level, seconds here.
TEST(FirstSetTest, Chain_of_levels_starting_with_the_next_loads_quickly) {
  std::string grammar = "S <- E0 ';'\nE22 <- E22 '.' Id / Id\n"
                        "Id <- < [a-z]+ >\n%whitespace <- [ ]*\n";
  for (int i = 0; i < 22; i++) {
    auto e = "E" + std::to_string(i), next = "E" + std::to_string(i + 1);
    grammar += e + " <- " + next + " 'o" + std::to_string(i) + "' " + e +
               " / " + next + "\n";
  }
  auto start = std::chrono::steady_clock::now();
  parser pg(grammar);
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(1));
  ASSERT_TRUE(!!pg);

  // Without packrat parsing, the chain parses each level twice.
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg.parse("a o3 b.c o0 d;"));
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
// reports errors notes what entering it would, and still enters one that must
// be tried, such as one that gives a message.

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

// A rule that starts with `&` can start only where its operand can.
TEST(UnstartableRuleTest, Is_not_entered_where_its_lookahead_cannot_start) {
  parser pg(R"(
    S  <- Op? 'x'
    Op <- &([-+]* [~!]) [-+~!]+ / [-+]
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;

  EXPECT_TRUE(pg.parse("x"));
  EXPECT_EQ(0u, tries(pg, "Op"));
  EXPECT_TRUE(pg.parse("+!x"));
  EXPECT_EQ(1u, tries(pg, "Op"));
}

TEST(UnstartableRuleTest,
     Is_entered_only_to_give_a_message_when_errors_are_reported) {
  parser pg(R"(
    S <- A? 'x'
    A <- 'y'
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;
  std::string message;
  pg.set_logger([&](size_t, size_t, const std::string &msg) { message = msg; });

  // What A expects is listed all the same.
  EXPECT_FALSE(pg.parse("z"));
  EXPECT_EQ(0u, tries(pg, "A"));
  EXPECT_EQ("syntax error, unexpected 'z', expecting 'y', 'x'.", message);

  // A rule that gives a message is entered for it.
  pg["A"].error_message = "no y";
  EXPECT_FALSE(pg.parse("z"));
  EXPECT_EQ(1u, tries(pg, "A"));
  EXPECT_EQ("no y", message);
}

// A is not entered where it cannot start, so it counts no level, with or
// without a logger.
TEST(UnstartableRuleTest, Is_not_entered_with_a_nesting_limit) {
  parser pg(R"(
    S <- B
    B <- A? 'x'
    A <- 'y'
  )");
  ASSERT_TRUE(!!pg);

  pg.set_max_depth(2);
  EXPECT_TRUE(pg.parse("x"));
  pg.set_logger([](size_t, size_t, const std::string &) {});
  EXPECT_TRUE(pg.parse("x"));
}

TEST(FirstSetTest, Left_recursive_alternative_is_tried_with_a_nesting_limit) {
  parser pg(R"(
    S <- A / 'b'
    A <- A 'a' / 'a'
  )");
  ASSERT_TRUE(!!pg);

  // A logger has A tried where it cannot start, and so does a nesting limit
  // without one: trying A goes one level past the limit either way.
  pg.set_max_depth(1);
  EXPECT_FALSE(pg.parse("b"));
  pg.set_logger([](size_t, size_t, const std::string &) {});
  EXPECT_FALSE(pg.parse("b"));
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
        E <- 'e'?
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

// A cannot start with 'z'. Entered there, it enters the rule its body starts
// with, as a parse that enters every rule does: through a lookahead or a
// macro, after a rule that can match empty, and the macro itself when it is
// left-recursive.
TEST(UnstartableRuleTest, Enters_what_a_rule_starts_with) {
  struct {
    const char *grammar;
    const char *rule;
  } cases[] = {
      {R"(S <- A? 'z'
          A <- M('q') 'y'
          M(x) <- K x
          K <- 'k')",
       "K"},
      {R"(S <- A? 'z'
          A <- !K 'y'
          K <- 'k')",
       "K"},
      {R"(S <- A? 'z'
          A <- !(K 'w') 'y'
          K <- 'k')",
       "K"},
      {R"(S <- A? 'z'
          A <- M('q') 'y'
          M(x) <- E K x
          E <- 'e'?
          K <- 'k')",
       "K"},
      {R"(S <- A? 'z'
          A <- M('q') 'y'
          M(x) <- M(x) x / 'k')",
       "M"},
  };
  for (const auto &c : cases) {
    for (auto packrat : {false, true}) {
      parser pg(c.grammar);
      ASSERT_TRUE(!!pg) << c.grammar;
      if (packrat) { pg.enable_packrat_parsing(); }
      auto count = 0;
      pg[c.rule].enter = [&](const Context &, const char *, size_t,
                             std::any &) { count++; };

      EXPECT_TRUE(pg.parse("z")) << c.grammar;
      EXPECT_EQ(1, count) << c.grammar << "\npackrat " << packrat;
    }
  }
}

// An AST action depends on its values alone, so running it or not goes
// unnoticed.
TEST(UnstartableRuleTest, Is_not_entered_for_an_ast_action_below_it) {
  parser pg(R"(
    S <- R? 'y'
    R <- E 'x'
    E <- 'e'?
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast(true);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("y", ast));
  EXPECT_EQ(0u, tries(pg, "R"));
}

// A literal, a token boundary and a no_whitespace rule skip whitespace after
// their match, even an empty one, so the whitespace can start what follows
// them. [a] skips no whitespace, so T starts at the space.
TEST(FirstSetTest, Whitespace_after_an_empty_match) {
  for (auto grammar : {
           R"(S <- [a] T
              T <- 'y' / Sep 'x'
              Sep <- ',' / '')",
           R"(S <- [a] T
              T <- 'y' / < 'z'? > 'x')",
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

// The same holds where a rule that cannot start with the next byte is not
// entered.
TEST(UnstartableRuleTest, Is_entered_at_whitespace_after_an_empty_match) {
  parser pg(R"(
    S <- [a] U
    U <- < [-+]? > 'x'
    %whitespace <- [ ]*
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("ax"));
  EXPECT_TRUE(pg.parse("a x"));
}

// =============================================================================
// Nullability
// =============================================================================

static void expect_analyses_agree(parser &pg, const std::string &grammar) {
  for (const auto &entry : pg.get_grammar()) {
    auto &rule = pg[entry.first.c_str()];
    ComputeFirstSet::FirstSetCache cache;
    ComputeFirstSet first_set(cache);
    rule.accept(first_set);
    EXPECT_EQ(rule.can_be_empty, first_set.result_.matches_empty_on_other_bytes)
        << entry.first << " in " << grammar;
  }
}

// ComputeCanBeEmpty works out whether a rule can match empty, for the rules
// and the left recursion check, and ComputeFirstSet whether it matches empty
// on a byte it cannot start with, for the first sets. Without `&`, the two
// are the same.
TEST(NullabilityTest, Analyses_agree) {
  const char *grammars[] = {
      "S <- 'a'? 'b'?",
      "S <- 'a'? 'b'",
      "S <- ('a'?){2}",
      "S <- 'a'+ 'b'*",
      "S <- 'a'* 'b'+",
      "S <- ('a'? / 'b')",
      "S <- ('b' / 'a'?)",
      "S <- ('a' / 'b')",
      "S <- !'a'",
      "S <- ''",
      "S <- A B\nA <- ('a'?){2}\nB <- ('b'? / 'c')",
      "S <- (A / B) 'x'\nA <- 'a'\nB <- 'b'?",
  };
  for (auto grammar : grammars) {
    parser pg;
    pg.set_logger([](size_t, size_t, const std::string &) {});
    ASSERT_TRUE(pg.load_grammar(grammar)) << grammar;
    expect_analyses_agree(pg, grammar);
  }
}

// `&` does what its operand does on a byte the operand cannot start with:
// `&'a' 'b'?` can match empty, but fails on any byte but 'a'. The other way
// round, a rule that matches empty there can match empty, which the first set
// walk relies on (ComputeFirstSet::walk_into).
TEST(NullabilityTest,
     And_predicate_does_what_its_operand_does_where_it_cannot_start) {
  struct Case {
    const char *grammar;
    bool can_be_empty;
    bool matches_empty_on_other_bytes;
    const char *starts;
  };
  const Case cases[] = {
      {"S <- &'a' 'b'?", true, false, "a"},
      {"S <- &'a'? 'b'", false, false, "ab"},
      {"S <- &'a'? 'b'?", true, true, "ab"},
  };
  for (const auto &c : cases) {
    parser pg(c.grammar);
    ASSERT_TRUE(!!pg) << c.grammar;
    const auto &rule = pg["S"];
    const auto &fs = rule.first_set;

    EXPECT_EQ(c.can_be_empty, rule.can_be_empty) << c.grammar;
    EXPECT_EQ(c.matches_empty_on_other_bytes, fs.matches_empty_on_other_bytes)
        << c.grammar;
    EXPECT_FALSE(fs.any_char) << c.grammar;
    std::bitset<256> starts;
    for (auto ch : std::string(c.starts)) {
      starts.set(static_cast<unsigned char>(ch));
    }
    EXPECT_EQ(starts, fs.chars) << c.grammar;
  }
}

// A rule made with the combinators can match empty too. B is left-recursive
// through A and can match empty only through X, so where the first sets cut
// that cycle, whether R can start with 'b' comes from whether X can: without
// a logger, R was skipped on "br". The left recursion check reads it where it
// meets X again, and S was not taken to be left-recursive.
TEST(NullabilityTest, Analyses_agree_on_a_rule_made_with_the_combinators) {
  Definition X;
  X <= opt(chr('x'));
  const char *grammar = "S <- R / 'q'\nR <- B 'r'\nA <- B\nB <- A 'b' / X";
  parser pg;
  ASSERT_TRUE(pg.load_grammar(grammar, Rules{{"X", X}}));
  expect_analyses_agree(pg, grammar);
  EXPECT_TRUE(pg["R"].first_set.chars['b']);
  EXPECT_TRUE(pg.parse("br"));

  parser lr;
  ASSERT_TRUE(lr.load_grammar("S <- X X S 'a' / 'b'", Rules{{"X", X}}));
  EXPECT_TRUE(lr.parse("baa"));
}

// What a parse reports: whether it matched, and every error the logger and
// the error reporter are given.
static std::string outcome(parser &pg, const std::string &input) {
  std::string out;
  pg.set_logger([&](size_t line, size_t col, const std::string &msg,
                    const std::string &rule) {
    out += std::to_string(line) + ":" + std::to_string(col) + " [" + rule +
           "] " + msg + "\n";
  });
  pg.set_error_reporter([&](const ErrorReport &r) {
    out += std::to_string(r.position) + " [" + r.label + "] " + r.message;
    for (const auto &l : r.expected_literals) {
      out += " '" + l + "'";
    }
    for (const auto &rule : r.expected_rules) {
      out += " <" + rule + ">";
    }
    out += "\n";
  });
  out += pg.parse(input) ? "matched\n" : "failed\n";
  return out;
}

// Each input, and each input with a byte left out or a `?` put in, is
// reported as a parse that takes no shortcut reports it.
TEST(FirstSetTest, Shortcuts_go_unnoticed) {
  const std::string calc = R"(
    Additive       <- Multiplicative '+' Additive / Multiplicative
    Multiplicative <- Primary '*' Multiplicative / Primary
    Primary        <- '(' Additive ')' / Number
    Number         <- < [0-9]+ >
    %whitespace    <- [ \t]*
  )";
  struct Case {
    std::string grammar;
    std::string input;
    std::function<void(parser &)> set_up = [](parser &) {};
  };
  const Case cases[] = {
      {calc, "1 + 2 * (3 + 4)"},
      {calc, "(1 + 2) * 3",
       [](parser &pg) { pg["Number"].error_message = "a number, please"; }},
      {R"(
        List   <- '[' Item (',' Item)* ']'^close
        Item   <- Number / Word / List
        Number <- < [0-9]+ >
        Word   <- < [a-z]+ > { error_message "a word" }
        close  <- (!']' .)* { error_message "missing ']'" }
        %whitespace <- [ \t]*
      )",
       "[1, ab, [2, c], 3]"},
      {read_grammar("json.peg"),
       R"({"a": [1, -2.5e3, true, null], "b": {"c": "d\n"}})"},
      {read_grammar("pl0_left_recursive.peg"),
       "VAR x; BEGIN x := 1; WHILE x < 10 DO x := x * 2; ! x END."},
      {read_grammar("monkey.peg"),
       "let f = fn(a, b) { if (a < b) { a + b } else { [a][0] } }; f(1, 2);"},
      {read_grammar("cpp-peglib.peg"),
       "S <- A (',' A)* / 'x'\nA <- < [a-z]+ > { no_ast_opt }\n"},
      {R"(
        S <- [q] (T 'x' / 'y')
        T <- < 'a'* >
        %whitespace <- (C / [ ])*
        C <- '#' [a-z]* { error_message "a comment" }
      )",
       "q a #b x"},
      {R"(
        S <- R 'x' / 'y'
        R <- 'a'*
      )",
       "aax",
       [](parser &pg) {
         pg["R"].predicate = [](const SemanticValues &vs, const std::any &,
                                std::string &msg) {
           msg = "no a";
           return !vs.sv().empty();
         };
       }},
      {R"(
        S <- A ';'
        A <- B 'x' / 'y'
        B <- A 'z' / 'w'
      )",
       "wxzx;"},
      {R"(
        S <- E ';'
        E <- E '+' T / T
        T <- T '*' F / F
        F <- '(' E ')' / N
        N <- < [0-9]+ > { error_message "a number" }
        %whitespace <- [ ]*
      )",
       "1 + 2 * (3);"},
      {R"(
        S       <- LIST(ITEM) / '!' ITEM
        LIST(X) <- '[' (X (',' X)*)? ']' { error_message "a list" }
        ITEM    <- NUM / WORD / LIST(NUM)
        NUM     <- < [0-9]+ > { error_message "a number" }
        WORD    <- < [a-z]+ >
        %whitespace <- [ ]*
      )",
       "[1, a, [2]]"},
      {R"(
        S <- T / R? 'c'
        T <- < R 'x' >
        R <- [r]
      )",
       "d", [](parser &pg) { pg.enable_packrat_parsing(); }},
      {R"(
        S <- T1 / T2
        T1 <- < R 'x' >
        T2 <- < R 'y' >
        R <- [a]*
      )",
       "q", [](parser &pg) { pg.enable_packrat_parsing(); }},
      {R"(
        S <- $x<[a-z]> (R / 'y')
        R <- $x<'k'?> 'b' { error_message "got [%{x}]" }
      )",
       "qkb"},
      {R"(
        Stmt    <- &Keyword Command / Assign
        Command <- Name Name*
        Assign  <- Name '=' Name
        Keyword <- 'print' / 'exit'
        Name    <- < [a-z]+ >
        %whitespace <- [ ]*
      )",
       "print x"},
  };
  for (const auto &c : cases) {
    parser pg(c.grammar);
    ASSERT_TRUE(!!pg) << c.grammar;
    c.set_up(pg);
    auto check = [&](const std::string &input) {
      auto taken = outcome(pg, input);
      takes_shortcuts() = false;
      auto untaken = outcome(pg, input);
      takes_shortcuts() = true;
      EXPECT_EQ(untaken, taken) << input;
    };
    check(c.input);
    for (size_t i = 0; i <= c.input.size(); i++) {
      if (i < c.input.size()) { check(std::string(c.input).erase(i, 1)); }
      check(std::string(c.input).insert(i, "?"));
    }
  }
}

// A rule that nothing runs around, no memo holds and nobody reads the token of
// hands its value to its caller as it makes it. The trees are those of a parse
// that enters every rule the long way, in a chain of such rules and where a
// rule in the chain has something to run, a memo or a seed.
TEST(FirstSetTest, Plain_entry_goes_unnoticed) {
  struct Case {
    std::string grammar;
    std::string input;
    std::function<void(parser &)> set_up = [](parser &) {};
  };
  const std::string chain = R"(
    S  <- A (',' A)*
    A  <- B Ta*
    Ta <- '+' B
    B  <- C Tb*
    Tb <- '*' C
    C  <- < [0-9]+ > / '(' S ')'
    %whitespace <- [ ]*
  )";
  const Case cases[] = {
      {chain, "1 + 2 * (3, 4) , 5"},
      {chain, "1 + 2 * (3, 4) , 5",
       [](parser &pg) { pg.enable_packrat_parsing(); }},
      {chain, "1 + 2 * (3, 4) , 5",
       [](parser &pg) {
         pg.enable_packrat_parsing();
         pg["B"].leave = [](const Context &, const char *, size_t, size_t,
                            std::any &, std::any &) {};
       }},
      {chain, "1 + 2 * (3, 4) , 5",
       [](parser &pg) {
         pg.enable_packrat_parsing();
         pg["B"].predicate = [](const SemanticValues &, const std::any &,
                                std::string &, std::any &) { return true; };
       }},
      {chain, "1 + 2 * (3, 4) , 5",
       [](parser &pg) { pg["C"].error_message = "a number"; }},
      {R"(
        S <- E ';'
        E <- E '+' T / T
        T <- F
        F <- G
        G <- < [0-9] >
      )",
       "1+2+3;", [](parser &pg) { pg.enable_packrat_parsing(); }},
      // Tokens that are runs of ASCII characters.
      {R"(
        S   <- Item (',' Item)*
        Item <- Num / Id
        Num <- < [0-9]+ >
        Id  <- < [a-z_]i [a-z0-9_]i* >
        %whitespace <- [ ]*
      )",
       "12 , ab_1, 3x ,Q"},
      // Keywords as a choice of literals, with a %word.
      {R"(
        S   <- (Kw / Id) (',' (Kw / Id))* ';'
        Kw  <- 'IN'i / 'INNER'i / 'INT'i / 'INTO'i / 'INTERSECT'i / 'JOIN'i /
               'ON'i / 'ORDER'i / 'IN'i / 'AS'i / 'ASC'i / 'ALL'i
        Id  <- < [a-z_]i [a-z0-9_]i* >
        %whitespace <- [ ]*
        %word <- [a-zA-Z0-9_]
      )",
       "in, INNER , Into,inx, intersect ,int ,asc ,AS;",
       [](parser &pg) { pg.enable_packrat_parsing(); }},
      {R"(
        S   <- Kw+ ';'
        Kw  <- 'IN'i / 'INNER'i / 'INT'i / 'INTO'i / 'JOIN'i / 'ON'i / 'ORDER'i /
               'AS'i / 'ASC'i / 'a-b' / 'ALL'i
        %whitespace <- [ ]*
        %word <- [a-zA-Z0-9_]
      )",
       "in inner into a-b asc;", [](parser &pg) { pg.enable_ast(); }},
      // Chains where tails start, the terminal gives no value, a level is
      // entered again at the position it forwards from, and recovery runs.
      {R"(
        S   <- Add
        Add <- Mul AddT*
        AddT <- '+' Mul
        Mul <- Num MulT*
        MulT <- '*' Num
        Num <- < [0-9]+ > / '(' Add ')'
        %whitespace <- [ ]*
      )",
       "1 + 2 * (3 + 4) + 5", [](parser &pg) { pg.enable_packrat_parsing(); }},
      {R"(
        S  <- R0 'z'
        R0 <- R1 X*
        R1 <- T
        ~T <- 'a'
        X  <- 'x'
      )",
       "az", [](parser &pg) { pg.enable_packrat_parsing(); }},
      {R"(
        S  <- L0
        L0 <- L1 T*
        L1 <- E
        E  <- 'x'?
        T  <- L1 'y'
      )",
       "y", [](parser &pg) { pg.enable_packrat_parsing(); }},
      {R"(
        S   <- L0 ';'
        L0  <- L1 T*
        L1  <- E
        E   <- 'a' 'b'^bad
        bad <- (!';' .)*
        T   <- 'x'
      )",
       "ac;", [](parser &pg) { pg.enable_packrat_parsing(); }},
      {R"(
        S <- &P Q
        P <- M
        Q <- M N
        M <- B
        B <- C
        C <- < [0-9] >
        N <- < [a-z] >
      )",
       "1a", [](parser &pg) { pg.enable_packrat_parsing(); }},
  };
  for (const auto &c : cases) {
    for (auto collapse : {false, true}) {
      parser pg(c.grammar);
      ASSERT_TRUE(!!pg) << c.grammar;
      c.set_up(pg);
      pg.enable_ast(collapse);
      auto tree = [&] {
        std::shared_ptr<Ast> ast;
        auto matched = pg.parse(c.input, ast);
        return std::to_string(matched) + (ast ? ast_to_s(ast) : "");
      };
      auto taken = tree();
      takes_shortcuts() = false;
      auto untaken = tree();
      takes_shortcuts() = true;
      EXPECT_EQ(untaken, taken) << c.grammar << " collapse=" << collapse;
    }
  }
}

// A rule entered the plain way is counted as a packrat miss, as one entered
// the long way is.
TEST(FirstSetTest, Plain_entry_is_counted_as_a_packrat_miss) {
  parser pg("S <- R? 'y'\n R <- E 'x'\n E <- 'e'?");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg.enable_ast(true);
  pg["S"].collect_packrat_stats = true;
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse("exy", ast));
  EXPECT_EQ(1u, tries(pg, "R"));
  EXPECT_EQ(1u, tries(pg, "E"));
}

// A rule that is parsed without whitespace is not entered the plain way, also
// where that is set between two parses.
TEST(FirstSetTest, Plain_entry_follows_no_whitespace_set_between_parses) {
  parser pg(R"(
    S <- R
    R <- 'a' 'b'
    %whitespace <- [ ]*
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  auto matches = [&](const char *input) {
    std::shared_ptr<Ast> ast;
    return pg.parse(input, ast);
  };
  EXPECT_TRUE(matches("a b"));
  pg["R"].no_whitespace = true;
  EXPECT_FALSE(matches("a b"));
  EXPECT_TRUE(matches("ab"));
  pg["R"].no_whitespace = false;
  EXPECT_TRUE(matches("a b"));
}

// A value handed on by a chain of rules that forward it has the tag of the
// outermost rule, as it has where each level gives it on in turn.
TEST(FirstSetTest, Forwarded_value_has_the_tag_of_the_outermost_rule) {
  for (auto fuses : {true, false}) {
    parser pg(R"(
      S  <- R0 'z'
      R0 <- R1 X*
      R1 <- T
      T  <- 'a'
      X  <- 'x'
    )");
    ASSERT_TRUE(!!pg);
    pg.enable_packrat_parsing();
    pg["T"] = [](const SemanticValues &) { return 1; };
    pg["S"] = [](const SemanticValues &vs) {
      return static_cast<unsigned int>(vs.tags.at(0));
    };
    takes_shortcuts() = fuses;
    unsigned int tag = 0;
    EXPECT_TRUE(pg.parse("az", tag));
    takes_shortcuts() = true;
    EXPECT_EQ(str2tag("R0"), tag) << fuses;
  }
}

// Whether an AST rule collapses can be changed between two parses.
TEST(FirstSetTest, Forwarding_follows_collapse_set_between_parses) {
  const char *grammar = R"(
    S  <- R0 'z'
    R0 <- R1 X*
    R1 <- T
    T  <- < 'a' >
    X  <- 'x'
  )";
  auto tree = [](parser &pg) {
    std::shared_ptr<Ast> ast;
    EXPECT_TRUE(pg.parse("az", ast));
    return ast_to_s(ast);
  };
  parser changed(grammar);
  ASSERT_TRUE(!!changed);
  changed.enable_packrat_parsing();
  changed.enable_ast(true);
  tree(changed);
  add_ast_action<Ast>(changed["R1"], false);

  parser fresh(grammar);
  fresh.enable_packrat_parsing();
  fresh.enable_ast(true);
  add_ast_action<Ast>(fresh["R1"], false);
  EXPECT_EQ(tree(fresh), tree(changed));
}

// What a rule is set to can be changed between two parses by any of the ways
// it can be set: the parse after it goes by the new setting, as it does where
// the setting was made before the first parse.
TEST(FirstSetTest, Rule_settings_changed_between_parses_are_seen) {
  const char *grammar = R"(
    S  <- R0 'z'
    R0 <- R1 X*
    R1 <- T
    T  <- < 'a' >
    X  <- 'x'
  )";
  struct Outcome {
    bool matched;
    std::string tree;
    int calls;
    bool operator==(const Outcome &rhs) const {
      return matched == rhs.matched && tree == rhs.tree && calls == rhs.calls;
    }
  };
  using Setting = std::function<void(parser &, int &)>;
  const std::pair<const char *, Setting> settings[] = {
      {"enter",
       [](parser &pg, int &calls) {
         pg["R1"].enter = [&](const Context &, const char *, size_t,
                              std::any &) { calls++; };
       }},
      {"leave",
       [](parser &pg, int &calls) {
         pg["R1"].leave = [&](const Context &, const char *, size_t, size_t,
                              std::any &, std::any &) { calls++; };
       }},
      {"predicate",
       [](parser &pg, int &calls) {
         pg["T"].predicate = [&](const SemanticValues &, const std::any &,
                                 std::string &) {
           calls++;
           return false;
         };
       }},
      {"ignoreSemanticValue",
       [](parser &pg, int &) { pg["R1"].ignoreSemanticValue = true; }},
      {"disable_action",
       [](parser &pg, int &) { pg["R1"].disable_action = true; }},
      {"error_message",
       [](parser &pg, int &calls) {
         pg["T"].error_message = "a t";
         pg["T"].error_message += "!";
         pg.set_logger([&](size_t, size_t, const std::string &msg) {
           if (msg.find("a t!") != std::string::npos) { calls++; }
         });
       }},
  };
  for (const auto &[name, set] : settings) {
    for (auto input : {"az", "xz"}) {
      auto outcome = [&](bool set_before_first_parse) {
        parser pg(grammar);
        EXPECT_TRUE(!!pg);
        pg.enable_packrat_parsing();
        pg.enable_ast(true);
        int calls = 0;
        auto run = [&]() {
          std::shared_ptr<Ast> ast;
          auto matched = pg.parse(input, ast);
          return Outcome{matched, ast ? ast_to_s(ast) : "", calls};
        };
        if (set_before_first_parse) {
          set(pg, calls);
          return run();
        }
        run();
        set(pg, calls);
        calls = 0;
        return run();
      };
      auto before = outcome(true);
      auto after = outcome(false);
      EXPECT_TRUE(before == after) << name << " " << input;
    }
  }
}

// A callback of a rule is copied, moved and set as a std::function is.
TEST(FirstSetTest, Rule_callbacks_copy_like_functions) {
  parser pg("S <- 'a'\n T <- 'b'\n");
  ASSERT_TRUE(!!pg);
  int calls = 0;
  pg["S"].enter = [&](const Context &, const char *, size_t, std::any &) {
    calls++;
  };
  auto &enter = pg["S"].enter;
  std::function<void(const Context &, const char *, size_t, std::any &)> plain =
      enter;
  auto copy = enter;
  decltype(pg["T"].enter) other = enter;
  pg["T"].enter = enter;
  pg["T"].enter = plain;
  pg["T"].enter = std::move(other);
  ASSERT_TRUE(!!pg["T"].enter);
  pg["T"].enter = nullptr;
  EXPECT_FALSE(!!pg["T"].enter);
  EXPECT_TRUE(!!copy);
  EXPECT_TRUE(pg.parse("a"));
  EXPECT_EQ(1, calls);
}

// Trees are made and released over and over, also by other threads than the
// ones that made them.
TEST(FirstSetTest, Trees_made_in_one_thread_are_released_in_another) {
  parser pg("S <- A (',' A)*\n A <- < [a-z]+ >\n %whitespace <- [ ]*\n");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::vector<std::shared_ptr<Ast>> trees;
  for (int round = 0; round < 50; round++) {
    for (int i = 0; i < 200; i++) {
      std::shared_ptr<Ast> ast;
      ASSERT_TRUE(pg.parse("ab, cd, ef", ast));
      trees.push_back(ast);
    }
    std::thread other([moved = std::move(trees)]() mutable { moved.clear(); });
    other.join();
    trees.clear();
  }
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse("x, y", ast));
  EXPECT_EQ(2u, ast->nodes.size());
}

// A keyword of ignore-case letters is a word only where both cases of its
// letters are word characters.
TEST(FirstSetTest, Ignore_case_keywords_with_a_word_of_lower_case_only) {
  parser pg(R"(
    S <- K 'X'?
    K <- 'select'i / 'from'i / 'where'i / 'and'i / 'or'i / 'not'i / 'in'i / 'is'i
    %word <- [a-z]
  )");
  ASSERT_TRUE(!!pg);
  for (const char *input : {"SELECT", "Select", "SELECTX", "select"}) {
    EXPECT_TRUE(pg.parse(input)) << input;
  }
}

// Analyses made and dropped one after another can be at one address.
TEST(FirstSetTest, Rule_indexing_is_not_kept_for_a_reused_analysis_address) {
  Definition R, Q, T;
  Q <= chr('a');
  R <= seq(chr('a'), chr('a'));
  T <= seq(Q, chr('z'));
  T.enablePackratParsing = true;
  auto once = [&](bool second) {
    Definition S;
    if (!second) {
      S <= seq(R, chr('y'));
    } else {
      S <= cho(seq(Q, chr('x')), seq(R, chr('y')));
    }
    S.enablePackratParsing = true;
    return S.parse("aay").ret;
  };
  EXPECT_TRUE(once(false));
  EXPECT_TRUE(T.parse("az").ret);
  EXPECT_TRUE(once(true));
}

// Every way of setting a rule's callback or message is seen by the next
// parse, each on a parser that has parsed before.
TEST(FirstSetTest, Rule_settings_set_by_copy_or_nothing_are_seen) {
  auto make = [] {
    auto pg = std::make_unique<parser>("S <- A 'x'\n A <- 'a'\n");
    EXPECT_TRUE(!!*pg);
    EXPECT_TRUE(pg->parse("ax"));
    return pg;
  };

  int entered = 0;
  Definition donor;
  donor.enter = [&](const Context &, const char *, size_t, std::any &) {
    entered++;
  };
  auto copied = make();
  (*copied)["A"].enter = donor.enter;
  EXPECT_TRUE(copied->parse("ax"));
  EXPECT_EQ(1, entered);
  (*copied)["A"].enter = {};
  EXPECT_TRUE(copied->parse("ax"));
  EXPECT_EQ(1, entered);

  std::string reported;
  Definition message_donor;
  message_donor.error_message = "A is missing";
  auto messaged = make();
  messaged->set_logger(
      [&](size_t, size_t, const std::string &msg) { reported = msg; });
  (*messaged)["A"].error_message = message_donor.error_message;
  EXPECT_FALSE(messaged->parse("bx"));
  EXPECT_EQ("A is missing", reported);
}

// A parse reuses the value frames the last parse of the thread left, also
// while another parse of the thread is under way.
TEST(FirstSetTest, Parses_nested_in_actions_do_not_share_value_frames) {
  parser inner("S <- < [a-z]+ > ' ' < [a-z]+ >\n");
  ASSERT_TRUE(!!inner);
  inner["S"] = [](const SemanticValues &vs) { return vs.token_to_string(1); };

  parser outer("S <- A ' ' A\n A <- < [0-9]+ >\n");
  ASSERT_TRUE(!!outer);
  outer["A"] = [&](const SemanticValues &vs) {
    std::string tail;
    EXPECT_TRUE(inner.parse("ab cd", tail));
    return vs.token_to_string() + tail;
  };
  outer["S"] = [](const SemanticValues &vs) {
    return std::any_cast<std::string>(vs[0]) + "," +
           std::any_cast<std::string>(vs[1]);
  };
  for (int i = 0; i < 3; i++) {
    std::string result;
    ASSERT_TRUE(outer.parse("12 34", result));
    EXPECT_EQ("12cd,34cd", result);
  }
}

// A rule's message is read and cleared as a string would be, and clearing it
// is seen by the next parse.
TEST(FirstSetTest, Rule_message_reads_and_clears_like_a_string) {
  parser pg("S <- A 'x'\n A <- 'a'\n");
  ASSERT_TRUE(!!pg);
  std::string reported;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { reported = msg; });
  pg["A"].error_message = "A is missing";
  EXPECT_FALSE(pg.parse("bx"));
  EXPECT_EQ("A is missing", reported);

  EXPECT_EQ(12u, pg["A"].error_message.length());
  std::string_view view = pg["A"].error_message;
  EXPECT_EQ("A is missing", view);

  pg["A"].error_message.clear();
  EXPECT_TRUE(pg["A"].error_message.empty());
  EXPECT_FALSE(pg.parse("bx"));
  EXPECT_NE("A is missing", reported);
}
