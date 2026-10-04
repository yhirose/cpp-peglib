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

TEST(PackratTest, Packrat_rule_below_a_shared_rule_runs_once) {
  parser pg(R"(
    S <- G 'x' / G 'y'
    G <- H
    H <- [0-9]+
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  size_t h_count = 0;
  pg["H"] = [&](const SemanticValues &) {
    h_count++;
    return std::string("h");
  };

  // Both alternatives reach H only through G, so only G needs a cache entry:
  // the second alternative gets G from it and never re-enters H.
  EXPECT_TRUE(pg.parse("123y"));
  EXPECT_EQ(1, h_count);
}

TEST(PackratTest, Packrat_rule_below_a_shared_macro_runs_once) {
  parser pg(R"(
    S    <- M('a') 'x' / M('a') 'y'
    M(p) <- X p
    X    <- [0-9]+
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  size_t x_count = 0;
  pg["X"] = [&](const SemanticValues &) {
    x_count++;
    return std::string("x");
  };

  // Macros are not memoized, so M cannot stand in for X: X itself must stay
  // cached for the second alternative to reuse it.
  EXPECT_TRUE(pg.parse("123ay"));
  EXPECT_EQ(1, x_count);
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

// Gives the rules AST actions that count their runs in `runs`. Such an action
// may be skipped where nothing reads its value (see Holder::parse_core), so
// the counts show which matches built a value.
static void count_ast_actions(parser &pg, const std::vector<std::string> &names,
                              std::map<std::string, size_t> &runs) {
  for (const auto &name : names) {
    auto &rule = pg[name.c_str()];
    rule = [&runs, name](const SemanticValues &vs) {
      runs[name]++;
      return std::make_shared<Ast>("", 1, 1, name.c_str(),
                                   vs.transform<std::shared_ptr<Ast>>());
    };
    rule.action.declare_ast_action<std::shared_ptr<Ast>>(false);
  }
}

// A match builds no value where nothing reads it: in a lookahead, in a
// token rule or below a `~` rule. A tracer sees every scope, so with one
// every value is built.
TEST(PackratTest, Builds_no_value_nothing_reads) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S     <- &LOOK TOKEN DROP KEPT
      LOOK  <- 'i'
      TOKEN <- < INNER >
      INNER <- 'i'
      ~DROP <- BELOW
      BELOW <- 'b'
      KEPT  <- 'k'
    )");
    ASSERT_TRUE(!!pg);
    if (packrat) { pg.enable_packrat_parsing(); }
    const std::vector<std::string> names{"S",    "LOOK",  "TOKEN", "INNER",
                                         "DROP", "BELOW", "KEPT"};
    std::map<std::string, size_t> runs;
    count_ast_actions(pg, names, runs);
    auto built = [&]() {
      EXPECT_TRUE(pg.parse("ibk"));
      std::set<std::string> built;
      for (const auto &[name, _] : runs) {
        built.insert(name);
      }
      runs.clear();
      return built;
    };

    EXPECT_EQ((std::set<std::string>{"S", "TOKEN", "KEPT"}), built());

    // Callbacks set since are taken into account: DROP's predicate reads
    // BELOW's value.
    pg["DROP"].predicate = [](const SemanticValues &vs, const std::any &,
                              std::string &) {
      return vs.size() == 1 && vs[0].has_value();
    };
    EXPECT_EQ((std::set<std::string>{"S", "TOKEN", "DROP", "BELOW", "KEPT"}),
              built());
    pg["DROP"].predicate = Predicate();
    EXPECT_EQ((std::set<std::string>{"S", "TOKEN", "KEPT"}), built());

    pg.enable_trace(
        [](const Ope &, const char *, size_t, const SemanticValues &,
           const Context &, const std::any &, std::any &) {},
        [](const Ope &, const char *, size_t, const SemanticValues &,
           const Context &, const std::any &, size_t, std::any &) {});
    EXPECT_EQ(std::set<std::string>(names.begin(), names.end()), built());
  }
}

// A memoized match is handed to every caller at its position. It builds no
// value when no match of its rule is ever read.
TEST(PackratTest, Packrat_builds_no_value_for_a_memoized_rule_nothing_reads) {
  parser pg(R"(
    S <- &A 'a' 'x' / &A 'a' 'y'
    A <- B
    B <- 'a'
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;
  std::map<std::string, size_t> runs;
  count_ast_actions(pg, {"S", "A", "B"}, runs);

  ASSERT_TRUE(pg.parse("ay"));
  EXPECT_LT(0u, pg["S"].packrat_stats_[pg["A"].id].hits);
  EXPECT_EQ(1u, runs["S"]);
  EXPECT_EQ(0u, runs["A"]);
  EXPECT_EQ(0u, runs["B"]);
}

// B was numbered apart from A and X, so its id is X's and C's tables do not
// index it. What it reads is read all the same: the memoized Z, which C
// itself uses only in a lookahead.
TEST(PackratTest, Packrat_keeps_the_value_that_a_rule_numbered_apart_reads) {
  Definition A, X, G, B, Z, C;
  X <= chr('x');
  A <= seq(X);
  Z <= chr('z');
  B <= seq(Z);
  G <= seq(B);
  ASSERT_TRUE(A.parse("x").ret);
  ASSERT_TRUE(G.parse("z").ret);
  ASSERT_EQ(X.id, B.id);

  Z = [](const SemanticValues &vs) {
    return std::make_shared<Ast>("", 1, 1, "Z",
                                 vs.transform<std::shared_ptr<Ast>>());
  };
  Z.action.declare_ast_action<std::shared_ptr<Ast>>(false);
  auto has_value = false;
  B = [&](const SemanticValues &vs) {
    has_value = vs.size() == 1 && vs[0].has_value();
  };

  C <= seq(npd(A), apd(cho(seq(Z, chr('1')), Z)), B);
  C.enablePackratParsing = true;
  C.collect_packrat_stats = true;
  ASSERT_TRUE(C.parse("z").ret);
  EXPECT_LT(0u, C.packrat_stats_[Z.id].hits);
  EXPECT_TRUE(has_value);
}

// An operator rule of a precedence is built to hand over its token, though
// its value is thrown away, so what its action reads is built too.
TEST(PackratTest, Packrat_keeps_the_values_below_an_ignored_operator_rule) {
  parser pg(R"(
    S    <- EXPR / &OP 'q'
    EXPR <- ATOM (OP ATOM)* { precedence L + }
    ~OP  <- PLUS 'z' / PLUS
    PLUS <- '+'
    ATOM <- [0-9]+
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;
  std::map<std::string, size_t> runs;
  count_ast_actions(pg, {"OP", "PLUS"}, runs);

  ASSERT_TRUE(pg.parse("1+2"));
  EXPECT_LT(0u, pg["S"].packrat_stats_[pg["PLUS"].id].hits);
  EXPECT_EQ(1u, runs["PLUS"]);
}

// What a lookahead matches is thrown away, in the scope of a precedence's
// operator too: OPM's body is parsed into it, and CM is built there only if
// its token is asked for.
TEST(PackratTest, Packrat_reads_no_token_in_a_lookahead_of_an_operator) {
  parser pg(R"(
    S      <- EXPR / &CM 'q'
    EXPR   <- ATOM (OPM(PLUS) ATOM)* { precedence L + }
    OPM(x) <- &CM x
    CM     <- D 'q' / D
    D      <- '+'
    PLUS   <- '+'
    ATOM   <- [0-9]+
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;
  std::map<std::string, size_t> runs;
  count_ast_actions(pg, {"CM", "D"}, runs);

  ASSERT_TRUE(pg.parse("1+2"));
  EXPECT_LT(0u, pg["S"].packrat_stats_[pg["D"].id].hits);
  EXPECT_EQ(0u, runs["CM"]);
  EXPECT_EQ(0u, runs["D"]);
}

// A left-recursive macro forms a scope of its own, which holds the values
// of its arguments in a lookahead too.
TEST(PackratTest, Packrat_keeps_the_value_of_a_macro_argument_in_a_lookahead) {
  parser pg(R"(
    S      <- &Sum(Num) &(Num '!' / Num) [0-9+]+
    Sum(x) <- Sum(x) '+' x / x
    Num    <- [0-9]+
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  pg["S"].collect_packrat_stats = true;
  std::map<std::string, size_t> runs;
  count_ast_actions(pg, {"Num"}, runs);
  auto has_values = true;
  pg["Sum"] = [&](const SemanticValues &vs) {
    for (const auto &v : vs) {
      if (!v.has_value()) { has_values = false; }
    }
    return 1;
  };

  ASSERT_TRUE(pg.parse("1+2"));
  EXPECT_LT(0u, pg["S"].packrat_stats_[pg["Num"].id].hits);
  EXPECT_TRUE(has_values);
}

// A match whose value is thrown away leaves no valueless cache entry for a
// later match of the rule at the same position whose value is read. Both
// alternatives start with A, so the selective packrat memoizes it.
TEST(PackratTest,
     Packrat_keeps_the_value_of_a_rule_read_after_a_dropped_match) {
  parser pg(R"(
    S <- ~A 'z' / A 'y'
    A <- B
    B <- 'a'
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_packrat_parsing();
  std::map<std::string, size_t> runs;
  count_ast_actions(pg, {"B"}, runs);
  auto has_value = false;
  pg["S"] = [&](const SemanticValues &vs) {
    has_value = vs.size() == 1 && vs[0].has_value();
  };
  ASSERT_TRUE(pg.parse("ay"));
  EXPECT_TRUE(has_value);
}

// Whether a value is read depends on the start rule. A parse from another
// start rule nested in an action must not change what the enclosing parse
// builds: S2 reads R only in a lookahead, S1 reads its value.
TEST(PackratTest, Packrat_parse_nested_from_another_start_rule_keeps_values) {
  for (auto with_predicate : {false, true}) {
    parser pg(R"(
      S1 <- N R
      S2 <- &R 'r'
      N  <- 'n'
      R  <- T
      T  <- 'r'
    )");
    ASSERT_TRUE(!!pg);
    pg.enable_packrat_parsing();

    const auto &g = pg.get_grammar();
    auto nested_ok = false;
    pg["N"] = [&](const SemanticValues &) {
      nested_ok = g.at("S2").parse("r").ret;
      return 1;
    };
    pg["T"] = [](const SemanticValues &) { return 42; };
    std::vector<int> seen;
    auto see = [&](const SemanticValues &vs) {
      seen.clear();
      for (const auto &v : vs) {
        seen.push_back(v.has_value() ? std::any_cast<int>(v) : -1);
      }
    };
    if (with_predicate) {
      pg["S1"].predicate = [&](const SemanticValues &vs, const std::any &,
                               std::string &) {
        see(vs);
        return true;
      };
    } else {
      pg["S1"] = see;
    }

    EXPECT_TRUE(pg.parse("nr")) << with_predicate;
    EXPECT_TRUE(nested_ok) << with_predicate;
    EXPECT_EQ((std::vector<int>{1, 42}), seen) << with_predicate;
  }
}

// No whitespace is skipped inside a token, a no_whitespace rule or the
// whitespace, so a rule matched there can match differently than at the same
// position elsewhere: NAME inside TYPE, and COMMENT in the whitespace after
// [a]. A memoized match from there must not stand in for one elsewhere.
TEST(PackratTest, Packrat_keeps_matches_that_skip_no_whitespace_apart) {
  struct {
    const char *grammar;
    std::vector<const char *> inputs;
  } cases[] = {
      {R"(
        S    <- DECL / EXPR
        DECL <- TYPE NAME
        TYPE <- < NAME >
        EXPR <- NAME '/' NAME
        NAME <- < [a-z]+ >
        %whitespace <- [ ]*
      )",
       {"x / y", "int x"}},
      {R"(
        S       <- 'a' 'z' / X
        X       <- [a] COMMENT 'q' / [a] COMMENT '/' 'y'
        COMMENT <- '#' 'x'
        %whitespace <- ([ ] / COMMENT)*
      )",
       {"a#x / y", "a z"}},
      {R"(
        S    <- DECL / EXPR
        DECL <- TYPE NAME
        TYPE <- NAME { no_whitespace }
        EXPR <- NAME '/' NAME
        NAME <- [a-z]+ ''
        %whitespace <- [ ]*
      )",
       {"x / y", "int x"}},
  };
  for (const auto &[grammar, inputs] : cases) {
    for (auto packrat : {false, true}) {
      parser pg(grammar);
      ASSERT_TRUE(!!pg);
      if (packrat) { pg.enable_packrat_parsing(); }
      for (auto input : inputs) {
        EXPECT_TRUE(pg.parse(input)) << input << " " << packrat;
      }
    }
  }
}

// R enters itself at the same position through the whitespace its empty
// literal skips, where no whitespace is skipped. That fails, with packrat as
// without it, however the memo keeps the two places apart.
TEST(PackratTest, Packrat_reentry_through_the_whitespace_fails) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S <- [c] T
      T <- R 'z' / R
      R <- '' 'a' / 'b'
      %whitespace <- R?
    )");
    ASSERT_TRUE(!!pg);
    if (packrat) { pg.enable_packrat_parsing(); }

    EXPECT_TRUE(pg.parse("ca")) << packrat;
    EXPECT_TRUE(pg.parse("caz")) << packrat;
  }
}

// =============================================================================
// Lookahead Predicate Tests
