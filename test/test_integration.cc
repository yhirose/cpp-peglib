#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// =============================================================================
// Enter/Leave Handler Tests
// =============================================================================

TEST(HandlerTest, Enter_leave_ordering) {
  parser pg(R"(
    S <- A B
    A <- 'a'
    B <- 'b'
  )");
  EXPECT_TRUE(pg);

  std::vector<std::string> log;

  pg["A"].enter = [&](const Context &, const char *, size_t, std::any &) {
    log.push_back("enter_A");
  };
  pg["A"].leave = [&](const Context &, const char *, size_t, size_t, std::any &,
                      std::any &) { log.push_back("leave_A"); };
  pg["B"].enter = [&](const Context &, const char *, size_t, std::any &) {
    log.push_back("enter_B");
  };
  pg["B"].leave = [&](const Context &, const char *, size_t, size_t, std::any &,
                      std::any &) { log.push_back("leave_B"); };

  EXPECT_TRUE(pg.parse("ab"));
  ASSERT_EQ(4, log.size());
  EXPECT_EQ("enter_A", log[0]);
  EXPECT_EQ("leave_A", log[1]);
  EXPECT_EQ("enter_B", log[2]);
  EXPECT_EQ("leave_B", log[3]);
}

TEST(HandlerTest, Enter_leave_on_failed_parse) {
  parser pg(R"(
    S <- A B
    A <- 'a'
    B <- 'b'
  )");
  EXPECT_TRUE(pg);

  std::vector<std::string> log;

  pg["A"].enter = [&](const Context &, const char *, size_t, std::any &) {
    log.push_back("enter_A");
  };
  pg["A"].leave = [&](const Context &, const char *, size_t, size_t, std::any &,
                      std::any &) { log.push_back("leave_A"); };
  pg["B"].enter = [&](const Context &, const char *, size_t, std::any &) {
    log.push_back("enter_B");
  };
  pg["B"].leave = [&](const Context &, const char *, size_t, size_t, std::any &,
                      std::any &) { log.push_back("leave_B"); };

  // 'a' matches A but 'x' fails B. B's enter is called, leave may or may not be
  EXPECT_FALSE(pg.parse("ax"));
  // A should have been entered and left
  EXPECT_GE(log.size(), 2u);
  EXPECT_EQ("enter_A", log[0]);
}

TEST(HandlerTest, Enter_leave_with_backtracking) {
  parser pg(R"(
    S <- A / B
    A <- ITEM 'x'
    B <- ITEM 'y'
    ITEM <- 'a'
  )");
  EXPECT_TRUE(pg);

  int enter_count = 0;
  int leave_count = 0;

  pg["ITEM"].enter = [&](const Context &, const char *, size_t, std::any &) {
    enter_count++;
  };
  pg["ITEM"].leave = [&](const Context &, const char *, size_t, size_t,
                         std::any &, std::any &) { leave_count++; };

  EXPECT_TRUE(pg.parse("ay"));
  // ITEM is matched twice (once in A, backtrack, once in B)
  EXPECT_EQ(2, enter_count);
  EXPECT_EQ(2, leave_count);
}

TEST(HandlerTest, Enter_leave_with_state_management) {
  parser pg(R"(
    S     <- SCOPE+
    SCOPE <- '{' INNER '}'
    INNER <- [a-z]*
  )");
  EXPECT_TRUE(pg);

  int depth = 0;
  int max_depth = 0;

  pg["SCOPE"].enter = [&](const Context &, const char *, size_t, std::any &) {
    depth++;
    if (depth > max_depth) max_depth = depth;
  };
  pg["SCOPE"].leave = [&](const Context &, const char *, size_t, size_t,
                          std::any &, std::any &) { depth--; };

  EXPECT_TRUE(pg.parse("{a}{b}{c}"));
  EXPECT_EQ(0, depth);
  EXPECT_EQ(1, max_depth);
}

// =============================================================================
// User-Defined Rule Tests
// =============================================================================

// =============================================================================
// AST Optimization Tests
// =============================================================================

TEST(ASTOptTest, Optimization_preserves_structure) {
  parser pg(R"(
    EXPR <- TERM ('+' TERM)*
    TERM <- FACTOR ('*' FACTOR)*
    FACTOR <- < [0-9]+ >
  )");
  EXPECT_TRUE(pg);
  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("1+2*3", ast));

  auto opt_ast = pg.optimize_ast(ast);
  // Structure should still be valid after optimization
  EXPECT_FALSE(opt_ast->nodes.empty());
}

TEST(ASTOptTest, Single_child_optimization) {
  parser pg(R"(
    A <- B
    B <- C
    C <- < [a-z]+ >
  )");
  EXPECT_TRUE(pg);
  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("hello", ast));

  auto opt_ast = pg.optimize_ast(ast);
  // Chain A -> B -> C should be optimized to single node
  EXPECT_TRUE(opt_ast->is_token);
  EXPECT_EQ("hello", opt_ast->token);
}

TEST(ASTOptTest, Optimization_with_multiple_children) {
  parser pg(R"(
    S <- A B C
    A <- 'a'
    B <- 'b'
    C <- 'c'
  )");
  EXPECT_TRUE(pg);
  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("abc", ast));

  auto opt_ast = pg.optimize_ast(ast);
  EXPECT_EQ(3, opt_ast->nodes.size());
}

// =============================================================================

// =============================================================================
// Ambiguity Tests
// =============================================================================

TEST(AmbiguityTest, First_choice_wins) {
  parser pg(R"(
    S <- A / B
    A <- < [a-z]+ >
    B <- < [a-z]+ >
  )");
  EXPECT_TRUE(pg);

  int choice = -1;
  pg["S"] = [&](const SemanticValues &vs) { choice = vs.choice(); };

  EXPECT_TRUE(pg.parse("hello"));
  EXPECT_EQ(0, choice); // First choice (A) should win
}

TEST(AmbiguityTest, Greedy_repetition) {
  // PEG is greedy: first [a-z]+ consumes everything,
  // leaving nothing for the second [a-z]+
  parser pg(R"(
    S <- [a-z]+ [a-z]+
  )");
  EXPECT_TRUE(pg);

  // Greedy first [a-z]+ takes all chars, second [a-z]+ fails
  EXPECT_FALSE(pg.parse("ab"));
  EXPECT_FALSE(pg.parse("a"));
}

TEST(AmbiguityTest, Ordered_choice_semantics) {
  // PEG is ordered choice, not longest match
  parser pg(R"(
    S <- KEYWORD / IDENT
    KEYWORD <- 'if' ![a-z]
    IDENT   <- < [a-z]+ >
  )");
  EXPECT_TRUE(pg);

  // 'if' matches KEYWORD first (with word boundary check)
  pg["S"] = [](const SemanticValues &vs) { EXPECT_EQ(0, vs.choice()); };
  EXPECT_TRUE(pg.parse("if"));

  // 'iffy' doesn't match KEYWORD (word boundary fails), so IDENT wins
  pg["S"] = [](const SemanticValues &vs) { EXPECT_EQ(1, vs.choice()); };
  EXPECT_TRUE(pg.parse("iffy"));
}

// =============================================================================
// Empty Pattern Edge Cases

// =============================================================================
// Empty Pattern Tests
// =============================================================================

TEST(EmptyTest, Optional_empty_literal) {
  parser pg(R"(S <- 'a' ''? 'b')");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("ab"));
}

TEST(EmptyTest, Empty_in_choice) {
  parser pg(R"(
    S <- 'a' SEP 'b'
    SEP <- ',' / ''
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("a,b"));
  EXPECT_TRUE(pg.parse("ab")); // Empty matches
}

TEST(EmptyTest, Empty_predicate_interaction) {
  parser pg(R"(
    S <- &'' 'hello'
  )");
  EXPECT_TRUE(pg);

  // &'' always succeeds (empty always matches)
  EXPECT_TRUE(pg.parse("hello"));
}

TEST(EmptyTest, Not_empty_predicate) {
  parser pg(R"(
    S <- !'' 'hello'
  )");
  EXPECT_TRUE(pg);

  // !'' always fails (empty always matches, so NOT fails)
  EXPECT_FALSE(pg.parse("hello"));
}

// =============================================================================

// =============================================================================
// Whitespace Tests
// =============================================================================

TEST(WhitespaceTest, Complex_whitespace_with_comments) {
  parser pg(R"(
    S <- ITEM+
    ITEM <- < [a-z]+ >
    %whitespace <- ([ \t\r\n] / COMMENT)*
    COMMENT <- '#' (!'\n' .)*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("hello # comment\nworld"));

  std::vector<std::string> items;
  pg["ITEM"] = [&](const SemanticValues &vs) {
    items.push_back(std::string(vs.token()));
  };

  items.clear();
  EXPECT_TRUE(pg.parse("foo # this is a comment\nbar"));
  ASSERT_EQ(2, items.size());
  EXPECT_EQ("foo", items[0]);
  EXPECT_EQ("bar", items[1]);
}

TEST(WhitespaceTest, Whitespace_between_tokens) {
  parser pg(R"(
    S           <- ITEM (',' ITEM)*
    ITEM        <- < [a-z]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  std::vector<std::string> tokens;
  pg["ITEM"] = [&](const SemanticValues &vs) {
    tokens.push_back(std::string(vs.token()));
  };

  EXPECT_TRUE(pg.parse("hello , world"));
  ASSERT_EQ(2, tokens.size());
  EXPECT_EQ("hello", tokens[0]);
  EXPECT_EQ("world", tokens[1]);
}

TEST(WhitespaceTest, Word_boundary_prevents_merging) {
  parser pg(R"(
    S           <- 'for' NAME
    NAME        <- < [a-z]+ >
    %whitespace <- [ \t]*
    %word       <- [a-z]+
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("for x"));
  EXPECT_FALSE(pg.parse("forx")); // Word boundary prevents 'for' + 'x' merging
}

// =============================================================================
// Dictionary Edge Cases
// =============================================================================

// =============================================================================
// Integration Tests
// =============================================================================

TEST(IntegrationTest, JSON_like_parser) {
  parser pg(R"(
    VALUE   <- OBJECT / ARRAY / STRING / NUMBER / BOOL / NULL
    OBJECT  <- '{' (PAIR (',' PAIR)*)? '}'
    PAIR    <- STRING ':' VALUE
    ARRAY   <- '[' (VALUE (',' VALUE)*)? ']'
    STRING  <- '"' < (!'"' .)* > '"'
    NUMBER  <- < '-'? [0-9]+ ('.' [0-9]+)? >
    BOOL    <- 'true' / 'false'
    NULL    <- 'null'
    %whitespace <- [ \t\r\n]*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse(R"({"key": "value", "num": 42})"));
  EXPECT_TRUE(pg.parse(R"([1, 2, 3])"));
  EXPECT_TRUE(pg.parse(R"({"nested": {"a": [1, true, null]}})"));
  EXPECT_TRUE(pg.parse(R"("hello")"));
  EXPECT_TRUE(pg.parse("42"));
  EXPECT_TRUE(pg.parse("true"));
  EXPECT_TRUE(pg.parse("null"));
  EXPECT_FALSE(pg.parse(R"({"key": })"));
  EXPECT_FALSE(pg.parse(R"([,])"));
}

TEST(IntegrationTest, JSON_like_parser_with_AST) {
  parser pg(R"(
    VALUE   <- OBJECT / ARRAY / STRING / NUMBER / BOOL / NULL
    OBJECT  <- '{' (PAIR (',' PAIR)*)? '}'
    PAIR    <- STRING ':' VALUE
    ARRAY   <- '[' (VALUE (',' VALUE)*)? ']'
    STRING  <- '"' < (!'"' .)* > '"'
    NUMBER  <- < '-'? [0-9]+ ('.' [0-9]+)? >
    BOOL    <- 'true' / 'false'
    NULL    <- 'null'
    %whitespace <- [ \t\r\n]*
  )");
  EXPECT_TRUE(pg);
  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse(R"({"a": 1, "b": [2, 3]})", ast));
  EXPECT_FALSE(ast->nodes.empty());
}

TEST(IntegrationTest, Simple_programming_language) {
  parser pg(R"(
    PROGRAM    <- STMT*
    STMT       <- LET / RETURN / EXPR_STMT
    LET        <- 'let' IDENT '=' EXPR ';'
    RETURN     <- 'return' EXPR ';'
    EXPR_STMT  <- EXPR ';'
    EXPR       <- TERM (('+' / '-') TERM)*
    TERM       <- FACTOR (('*' / '/') FACTOR)*
    FACTOR     <- NUMBER / IDENT / '(' EXPR ')'
    IDENT      <- < !KEYWORD [a-zA-Z_][a-zA-Z0-9_]* >
    NUMBER     <- < [0-9]+ >
    KEYWORD    <- ('let' / 'return') ![a-zA-Z0-9_]
    %whitespace <- [ \t\r\n]*
    %word       <- [a-zA-Z0-9_]
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("let x = 1;"));
  EXPECT_TRUE(pg.parse("let y = x + 2 * 3;"));
  EXPECT_TRUE(pg.parse("return x + y;"));
  EXPECT_TRUE(pg.parse("let a = 1; let b = 2; return a + b;"));
  EXPECT_FALSE(pg.parse("let = 1;")); // Missing ident
}

TEST(IntegrationTest, Calculator_with_all_features) {
  // Test combining: AST, packrat, whitespace, macros, actions
  parser pg(R"(
    EXPR         <- _ LIST(TERM, TERM_OP)
    TERM         <- LIST(FACTOR, FACTOR_OP)
    FACTOR       <- NUMBER / T('(') EXPR T(')')
    TERM_OP      <- T([-+])
    FACTOR_OP    <- T([*/])
    NUMBER       <- T([0-9]+)
    LIST(I, D)   <- I (D I)*
    T(S)         <- < S > _
    ~_           <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  pg.enable_packrat_parsing();

  auto reduce = [](const SemanticValues &vs) {
    auto result = std::any_cast<long>(vs[0]);
    for (auto i = 1u; i < vs.size(); i += 2) {
      auto num = std::any_cast<long>(vs[i + 1]);
      auto ope = std::any_cast<char>(vs[i]);
      switch (ope) {
      case '+': result += num; break;
      case '-': result -= num; break;
      case '*': result *= num; break;
      case '/': result /= num; break;
      }
    }
    return result;
  };

  pg["EXPR"] = reduce;
  pg["TERM"] = reduce;
  pg["TERM_OP"] = [](const SemanticValues &vs) {
    return static_cast<char>(*vs.sv().data());
  };
  pg["FACTOR_OP"] = [](const SemanticValues &vs) {
    return static_cast<char>(*vs.sv().data());
  };
  pg["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  long val = 0;
  EXPECT_TRUE(pg.parse(" 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ", val));
  EXPECT_EQ(-3, val);
}

// =============================================================================
// Repetition Edge Cases
// =============================================================================

// =============================================================================
// Feature Interaction Tests
// =============================================================================

TEST(InteractionTest, Packrat_with_cut) {
  parser pg(R"(
    S <- A / B
    A <- 'a' ↑ 'x'
    B <- 'a' 'y'
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("ax"));
  EXPECT_FALSE(pg.parse("ay")); // Cut prevents B
}

TEST(InteractionTest, AST_with_whitespace_and_token) {
  parser pg(R"(
    S           <- ITEM (',' ITEM)*
    ITEM        <- < [a-z]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);
  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("hello , world , foo", ast));
  // S should have 3 ITEM children (commas are literals, not captured)
  EXPECT_EQ(3, ast->nodes.size());
}

TEST(InteractionTest, Predicate_with_packrat) {
  parser pg(R"(
    S   <- A / B
    A   <- NUM
    B   <- NUM
    NUM <- < [0-9]+ >
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  pg["A"].predicate = [](const SemanticValues &vs, const std::any &,
                         std::string &msg) {
    if (vs.token_to_number<int>() > 10) {
      msg = "too large for A";
      return false;
    }
    return true;
  };

  EXPECT_TRUE(pg.parse("5"));   // A matches
  EXPECT_TRUE(pg.parse("100")); // A fails predicate, B matches
}

TEST(InteractionTest, Left_recursion_with_whitespace) {
  parser pg(R"(
    EXPR <- EXPR '+' TERM / TERM
    TERM <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("1 + 2 + 3"));
  EXPECT_TRUE(pg.parse("42"));
}

TEST(InteractionTest, Left_recursion_with_AST) {
  parser pg(R"(
    EXPR <- EXPR '+' TERM / TERM
    TERM <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);
  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("1 + 2 + 3", ast));
  EXPECT_FALSE(ast->nodes.empty());
}

TEST(InteractionTest, Left_recursion_with_packrat) {
  parser pg(R"(
    EXPR <- EXPR '+' TERM / TERM
    TERM <- < [0-9]+ >
  )");
  pg.enable_packrat_parsing();
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("1+2+3"));
}

TEST(InteractionTest, Macro_with_left_recursion) {
  parser pg(R"(
    EXPR        <- EXPR '+' TERM / TERM
    TERM        <- T([0-9]+)
    T(S)        <- < S > _
    ~_          <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("1+2+3"));
}
