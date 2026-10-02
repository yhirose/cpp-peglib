#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// =============================================================================
// Macro Tests
// =============================================================================

TEST(MacroTest, Macro_simple_test) {
  parser parser(R"(
		S     <- HELLO WORLD
		HELLO <- T('hello')
		WORLD <- T('world')
		T(a)  <- a [ \t]*
	)");

  EXPECT_TRUE(parser.parse("hello \tworld "));
}

TEST(MacroTest, Macro_two_parameters) {
  parser parser(R"(
		S           <- HELLO_WORLD
		HELLO_WORLD <- T('hello', 'world')
		T(a, b)     <- a [ \t]* b [ \t]*
	)");

  EXPECT_TRUE(parser.parse("hello \tworld "));
}

TEST(MacroTest, Macro_syntax_error) {
  parser parser(R"(
		S     <- T('hello')
		T (a) <- a [ \t]*
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(MacroTest, Macro_missing_argument) {
  parser parser(R"(
		S       <- T ('hello')
		T(a, b) <- a [ \t]* b
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(MacroTest, Macro_reference_syntax_error) {
  parser parser(R"(
		S    <- T ('hello')
		T(a) <- a [ \t]*
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(MacroTest, Macro_invalid_macro_reference_error) {
  parser parser(R"(
		S <- T('hello')
		T <- 'world'
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(MacroTest, Macro_calculator) {
  // Create a PEG parser
  parser parser(R"(
    # Grammar for simple calculator...
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

  // Setup actions
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

  parser["EXPRESSION"] = reduce;
  parser["TERM"] = reduce;
  parser["TERM_OPERATOR"] = [](const SemanticValues &vs) {
    return static_cast<char>(*vs.sv().data());
  };
  parser["FACTOR_OPERATOR"] = [](const SemanticValues &vs) {
    return static_cast<char>(*vs.sv().data());
  };
  parser["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  bool ret = parser;
  EXPECT_TRUE(ret);

  auto expr = " 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ";
  long val = 0;
  ret = parser.parse(expr, val);

  EXPECT_TRUE(ret);
  EXPECT_EQ(-3, val);
}

TEST(MacroTest, Macro_expression_arguments) {
  parser parser(R"(
		S             <- M('hello' / 'Hello', 'world' / 'World')
		M(arg0, arg1) <- arg0 [ \t]+ arg1
	)");

  EXPECT_TRUE(parser.parse("Hello world"));
}

TEST(MacroTest, Macro_recursive) {
  parser parser(R"(
		S    <- M('abc')
		M(s) <- !s / s ' ' M(s / '123') / s
	)");

  EXPECT_TRUE(parser.parse(""));
  EXPECT_TRUE(parser.parse("abc"));
  EXPECT_TRUE(parser.parse("abc abc"));
  EXPECT_TRUE(parser.parse("abc 123 abc"));
}

TEST(MacroTest, Macro_recursive2) {
  auto syntaxes = std::vector<const char *>{
      "S <- M('abc') M(s) <- !s / s ' ' M(s* '-' '123') / s",
      "S <- M('abc') M(s) <- !s / s ' ' M(s+ '-' '123') / s",
      "S <- M('abc') M(s) <- !s / s ' ' M(s? '-' '123') / s",
      "S <- M('abc') M(s) <- !s / s ' ' M(&s s+ '-' '123') / s",
      "S <- M('abc') M(s) <- !s / s ' ' M(s '-' !s '123') / s",
      "S <- M('abc') M(s) <- !s / s ' ' M(< s > '-' '123') / s",
      "S <- M('abc') M(s) <- !s / s ' ' M(~s '-' '123') / s",
  };

  for (const auto &syntax : syntaxes) {
    parser parser(syntax);
    EXPECT_TRUE(parser.parse("abc abc-123"));
  }
}

TEST(MacroTest, Macro_exclusive_modifiers) {
  parser parser(R"(
		S                   <- Modifiers(!"") _
		Modifiers(Appeared) <- (!Appeared) (
								   Token('public') Modifiers(Appeared / 'public') /
								   Token('static') Modifiers(Appeared / 'static') /
								   Token('final') Modifiers(Appeared / 'final') /
								   "")
		Token(t)            <- t _
		_                   <- [ \t\r\n]*
	)");

  EXPECT_TRUE(parser.parse("public"));
  EXPECT_TRUE(parser.parse("static"));
  EXPECT_TRUE(parser.parse("final"));
  EXPECT_TRUE(parser.parse("public static final"));
  EXPECT_FALSE(parser.parse("public public"));
  EXPECT_FALSE(parser.parse("public static public"));
}

TEST(MacroTest, Macro_token_check_test) {
  parser parser(R"(
        # Grammar for simple calculator...
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

  EXPECT_FALSE(parser["EXPRESSION"].is_token());
  EXPECT_FALSE(parser["TERM"].is_token());
  EXPECT_FALSE(parser["FACTOR"].is_token());
  EXPECT_TRUE(parser["FACTOR_OPERATOR"].is_token());
  EXPECT_TRUE(parser["NUMBER"].is_token());
  EXPECT_TRUE(parser["_"].is_token());
  EXPECT_FALSE(parser["LIST"].is_token());
  EXPECT_TRUE(parser["T"].is_token());
}

TEST(MacroTest, Macro_passes_an_arg_to_another_macro) {
  parser parser(R"(
        A    <- B(C)
        B(D) <- D
        C    <- 'c'
	)");

  EXPECT_TRUE(parser.parse("c"));
}

TEST(MacroTest, Unreferenced_rule) {
  parser parser(R"(
        A    <- B(C)
        B(D) <- D
        C    <- 'c'
        D    <- 'd'
	)");

  bool ret = parser;
  EXPECT_TRUE(ret); // This is OK, because it's a warning, not an erro...
}

TEST(MacroTest, Nested_macro_call) {
  parser parser(R"(
        A    <- B(T)
        B(X) <- C(X)
        C(Y) <- Y
        T    <- 'val'
	)");

  EXPECT_TRUE(parser.parse("val"));
}

TEST(MacroTest, Nested_macro_call2) {
  parser parser(R"(
        START           <- A('TestVal1', 'TestVal2')+
        A(Aarg1, Aarg2) <- B(Aarg1) '#End'
        B(Barg1)        <- '#' Barg1
	)");

  EXPECT_TRUE(parser.parse("#TestVal1#End"));
}

// =============================================================================
// Macro Edge Case Tests
// =============================================================================

TEST(MacroEdgeTest, Macro_with_complex_arguments) {
  parser pg(R"(
    S           <- PAIR('key', 'value')
    PAIR(K, V)  <- K '=' V
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("key=value"));
  EXPECT_FALSE(pg.parse("key=other"));
}

TEST(MacroEdgeTest, Macro_with_whitespace) {
  parser pg(R"(
    S          <- LIST(ITEM, ',')
    LIST(I, D) <- I (D I)*
    ITEM       <- < [a-z]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  std::vector<std::string> items;
  pg["ITEM"] = [&](const SemanticValues &vs) {
    items.push_back(std::string(vs.token()));
  };

  EXPECT_TRUE(pg.parse("a , b , c"));
  ASSERT_EQ(3, items.size());
  EXPECT_EQ("a", items[0]);
  EXPECT_EQ("b", items[1]);
  EXPECT_EQ("c", items[2]);
}

// =============================================================================
// Context/State Tests

// =============================================================================
// Precedence Tests
// =============================================================================

TEST(PrecedenceTest, Precedence_climbing) {
  parser parser(R"(
        START            <-  _ EXPRESSION
        EXPRESSION       <-  ATOM (OPERATOR ATOM)* {
                               precedence
                                 L + -
                                 L * /
                             }
        ATOM             <-  NUMBER / T('(') EXPRESSION T(')')
        OPERATOR         <-  T([-+*/])
        NUMBER           <-  T('-'? [0-9]+)
		~_               <-  [ \t]*
		T(S)             <-  < S > _
	)");

  EXPECT_TRUE(!!parser);

  parser.enable_packrat_parsing();

  // Setup actions
  parser["EXPRESSION"] = [](const SemanticValues &vs) -> long {
    auto result = std::any_cast<long>(vs[0]);
    if (vs.size() > 1) {
      auto ope = std::any_cast<char>(vs[1]);
      auto num = std::any_cast<long>(vs[2]);
      switch (ope) {
      case '+': result += num; break;
      case '-': result -= num; break;
      case '*': result *= num; break;
      case '/': result /= num; break;
      }
    }
    return result;
  };
  parser["OPERATOR"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
  parser["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  bool ret = parser;
  EXPECT_TRUE(ret);

  {
    auto expr = " 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ";
    long val = 0;
    ret = parser.parse(expr, val);

    EXPECT_TRUE(ret);
    EXPECT_EQ(-3, val);
  }

  {
    auto expr = "-1+-2--3"; // -1 + -2 - -3 = 0
    long val = 0;
    ret = parser.parse(expr, val);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0, val);
  }
}

TEST(PrecedenceTest, Precedence_climbing_with_literal_operator) {
  parser parser(R"(
        START            <-  _ EXPRESSION
        EXPRESSION       <-  ATOM (OPERATOR ATOM)* {
                               precedence
                                 L '#plus#' -     # weaker
                                 L '#multiply#' / # stronger
                             }
        ATOM             <-  NUMBER / T('(') EXPRESSION T(')')
        OPERATOR         <-  T('#plus#' / '#multiply#' / [-/])
        NUMBER           <-  T('-'? [0-9]+)
		~_               <-  [ \t]*
		T(S)             <-  < S > _
	)");

  EXPECT_TRUE(!!parser);

  parser.enable_packrat_parsing();

  // Setup actions
  parser["EXPRESSION"] = [](const SemanticValues &vs) -> long {
    auto result = std::any_cast<long>(vs[0]);
    if (vs.size() > 1) {
      auto ope = std::any_cast<std::string>(vs[1]);
      auto num = std::any_cast<long>(vs[2]);
      if (ope == "#plus#") {
        result += num;
      } else if (ope == "-") {
        result -= num;
      } else if (ope == "#multiply#") {
        result *= num;
      } else if (ope == "/") {
        result /= num;
      }
    }
    return result;
  };
  parser["OPERATOR"] = [](const SemanticValues &vs) {
    return vs.token_to_string();
  };
  parser["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  bool ret = parser;
  EXPECT_TRUE(ret);

  {
    auto expr =
        " 1 #plus#  2 #multiply# 3 #multiply# (4 - 5 #plus# 6) / 7 - 8 ";
    long val = 0;
    ret = parser.parse(expr, val);

    EXPECT_TRUE(ret);
    EXPECT_EQ(-3, val);
  }

  {
    auto expr = "-1#plus#-2--3"; // -1 + -2 - -3 = 0
    long val = 0;
    ret = parser.parse(expr, val);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0, val);
  }
}

TEST(PrecedenceTest, Precedence_climbing_with_macro) {
  // Create a PEG parser
  parser parser(R"(
        EXPRESSION             <-  INFIX_EXPRESSION(ATOM, OPERATOR)
        INFIX_EXPRESSION(A, O) <-  A (O A)* {
                                     precedence
                                       L + -
                                       L * /
                                   }
        ATOM                   <-  NUMBER / '(' EXPRESSION ')'
        OPERATOR               <-  < [-+*/] >
        NUMBER                 <-  < '-'? [0-9]+ >
        %whitespace            <-  [ \t]*
	)");

  parser.enable_packrat_parsing();

  bool ret = parser;
  EXPECT_TRUE(ret);

  // Setup actions
  parser["INFIX_EXPRESSION"] = [](const SemanticValues &vs) -> long {
    auto result = std::any_cast<long>(vs[0]);
    if (vs.size() > 1) {
      auto ope = std::any_cast<char>(vs[1]);
      auto num = std::any_cast<long>(vs[2]);
      switch (ope) {
      case '+': result += num; break;
      case '-': result -= num; break;
      case '*': result *= num; break;
      case '/': result /= num; break;
      }
    }
    return result;
  };
  parser["OPERATOR"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
  parser["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  {
    auto expr = " 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ";
    long val = 0;
    ret = parser.parse(expr, val);

    EXPECT_TRUE(ret);
    EXPECT_EQ(-3, val);
  }

  {
    auto expr = "-1+-2--3"; // -1 + -2 - -3 = 0
    long val = 0;
    ret = parser.parse(expr, val);

    EXPECT_TRUE(ret);
    EXPECT_EQ(0, val);
  }
}

TEST(PrecedenceTest, Precedence_climbing_error1) {
  parser parser(R"(
        START            <-  _ EXPRESSION
        EXPRESSION       <-  ATOM (OPERATOR ATOM1)* {
                               precedence
                                 L + -
                                 L * /
                             }
        ATOM             <-  NUMBER / T('(') EXPRESSION T(')')
        ATOM1            <-  NUMBER / T('(') EXPRESSION T(')')
        OPERATOR         <-  T([-+*/])
        NUMBER           <-  T('-'? [0-9]+)
		~_               <-  [ \t]*
		T(S)             <-  < S > _
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(PrecedenceTest, Precedence_climbing_error2) {
  parser parser(R"(
        START            <-  _ EXPRESSION
        EXPRESSION       <-  ATOM OPERATOR ATOM {
                               precedence
                                 L + -
                                 L * /
                             }
        ATOM             <-  NUMBER / T('(') EXPRESSION T(')')
        OPERATOR         <-  T([-+*/])
        NUMBER           <-  T('-'? [0-9]+)
		~_               <-  [ \t]*
		T(S)             <-  < S > _
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(PrecedenceTest, Precedence_climbing_error3) {
  parser parser(R"(
        EXPRESSION               <-  PRECEDENCE_PARSING(ATOM, OPERATOR)
        PRECEDENCE_PARSING(A, O) <-  A (O A)+ {
                                       precedence
                                         L + -
                                         L * /
                                     }
        ATOM                     <-  NUMBER / '(' EXPRESSION ')'
        OPERATOR                 <-  < [-+*/] >
        NUMBER                   <-  < '-'? [0-9]+ >
        %whitespace              <-  [ \t]*
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(PrecedenceTest, Precedence_climbing_does_not_refer_to_grammar_text) {
  std::string grammar = R"(
        EXPRESSION  <-  ATOM (OPERATOR ATOM)* {
                          precedence
                            L + -
                            L * /
                        }
        ATOM        <-  < [0-9]+ >
        OPERATOR    <-  < [-+*/] >
	)";
  parser parser(grammar);
  ASSERT_TRUE(!!parser);

  // The grammar text may be gone once the grammar is loaded.
  std::fill(grammar.begin(), grammar.end(), ' ');

  EXPECT_TRUE(parser.parse("1+2*3-4"));
}

// An operator tried but not used leaves no capture behind, as in the
// repetition that the rule is without its precedence instruction: BINOP
// captures an empty `o` before failing at ';', and the BINOP after it can
// match "x" only with an `o` that a used operator left. Without a logger,
// BINOP would not even be entered at ';'.
TEST(PrecedenceTest,
     Precedence_climbing_drops_the_captures_of_unused_operators) {
  for (std::string precedence : {"", "{ precedence L + }"}) {
    for (auto with_logger : {false, true}) {
      auto grammar = R"(
        S     <- EXPR ';' BINOP
        EXPR  <- ATOM (BINOP ATOM)* )" +
                     precedence + R"(
        ATOM  <- [0-9]
        BINOP <- $o<!'x'>? ('+' / 'x' $o)
      )";
      parser pg(grammar);
      ASSERT_TRUE(!!pg) << precedence;
      if (with_logger) {
        pg.set_logger([](size_t, size_t, const std::string &) {});
      }

      EXPECT_FALSE(pg.parse("1;x")) << precedence << with_logger;
      EXPECT_TRUE(pg.parse("1+2;x")) << precedence << with_logger;
    }
  }
}

// A precedence action sees what the repetition `atom (binop atom)*` would
// produce: each value with its tag, and no value for an operand without one.
// Its result carries the rule's tag.
TEST(PrecedenceTest, Precedence_climbing_hands_over_values_with_their_tags) {
  struct {
    std::string op;
    std::vector<std::string> folds;
  } cases[] = {
      {"OP", {"(2/n */o 3/n )", "(1/n +/o (2/n */o 3/n )/e )"}},
      {"~OP", {"(2/n 3/n )", "(1/n (2/n 3/n )/e )"}},
  };
  for (const auto &[op, expected] : cases) {
    parser pg((R"(
      E    <- NUM (OP NUM)* { precedence L + L * }
      NUM  <- < [0-9]+ >
      )" + op + R"( <- < [+*] >
      %whitespace <- [ ]*
    )")
                  .c_str());
    ASSERT_TRUE(!!pg) << op;

    std::vector<std::string> folds;
    pg["NUM"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
    pg["OP"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
    pg["E"] = [&](const SemanticValues &vs) {
      EXPECT_EQ(vs.size(), vs.tags.size());
      std::string r = "(";
      for (size_t i = 0; i < vs.size(); i++) {
        auto tag = vs.tags.at(i);
        r += std::any_cast<std::string>(vs[i]) + "/" +
             (tag == str2tag("NUM")  ? "n"
              : tag == str2tag("OP") ? "o"
              : tag == str2tag("E")  ? "e"
                                     : "?") +
             " ";
      }
      folds.push_back(r + ")");
      return r + ")";
    };

    std::string val;
    EXPECT_TRUE(pg.parse("1 + 2 * 3", val)) << op;
    EXPECT_EQ(expected, folds) << op;
    EXPECT_EQ(expected.back(), val) << op;
  }

  // Without an action the first operand's value, with its tag, stands for
  // the fold, as a predicate on the rule sees.
  parser pg(R"(
    E    <- NUM (OP NUM)* { precedence L + }
    NUM  <- < [0-9]+ >
    OP   <- < '+' >
    %whitespace <- [ ]*
  )");
  ASSERT_TRUE(!!pg);
  pg["NUM"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
  pg["E"].predicate = [](const SemanticValues &vs, const std::any &,
                         std::string &) {
    EXPECT_EQ(1u, vs.size());
    EXPECT_EQ(1u, vs.tags.size());
    EXPECT_EQ("1", std::any_cast<std::string>(vs[0]));
    EXPECT_EQ(str2tag("NUM"), vs.tags.at(0));
    return true;
  };
  EXPECT_TRUE(pg.parse("1 + 2 + 3"));
}

// An operator rule hands its token to the precedence rule also when its own
// value is thrown away and nothing else would make it build one.
TEST(PrecedenceTest, Precedence_climbing_with_an_ignored_operator_rule) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      E    <- A (O A)* { precedence L + - L * }
      ~O   <- P
      P    <- < [-+*] >
      A    <- < [0-9]+ >
    )");
    ASSERT_TRUE(!!pg);
    pg.enable_ast(true);
    if (packrat) { pg.enable_packrat_parsing(); }

    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("1+2*3", ast)) << packrat;
    EXPECT_EQ(R"(+ E
  - A (1)
  + E
    - A (2)
    - A (3)
)",
              ast_to_s(ast))
        << packrat;
  }
}

// Such an operator rule still runs its action as it hands over its token, on
// the values of the rules below it, which must be built.
TEST(PrecedenceTest,
     Precedence_climbing_with_an_ignored_operator_rule_above_another) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      E    <- A (O A)* { precedence L + - L * }
      ~O   <- X
      X    <- P
      P    <- < [-+*] >
      A    <- < [0-9]+ >
    )");
    ASSERT_TRUE(!!pg);
    pg.enable_ast(true);
    if (packrat) { pg.enable_packrat_parsing(); }

    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("1+2*3", ast)) << packrat;
    EXPECT_EQ(R"(+ E
  - A (1)
  + E
    - A (2)
    - A (3)
)",
              ast_to_s(ast))
        << packrat;
  }
}

// An operator rule hands its token to the precedence rule also when the
// packrat cache already holds its match at that position. Both alternatives
// of F start with O, so the selective packrat memoizes O, and F's lookahead
// caches O's match before the precedence rule parses it.
TEST(PrecedenceTest, Precedence_climbing_with_a_cached_operator_rule) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      E    <- A (O A)* { precedence L + - L * }
      A    <- N F
      F    <- &(O 'x') / &(O 'y') / ''
      N    <- < [0-9]+ >
      O    <- < [-+*] >
    )");
    ASSERT_TRUE(!!pg);
    pg.enable_ast(true);
    if (packrat) { pg.enable_packrat_parsing(); }

    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("1+2*3", ast)) << packrat;
    EXPECT_EQ(R"(+ E
  + A
    - N (1)
    - F/2 ()
  - O (+)
  + E
    + A
      - N (2)
      - F/2 ()
    - O (*)
    + A
      - N (3)
      - F/2 ()
)",
              ast_to_s(ast))
        << packrat;
  }
}

// A left-recursive operator rule hands over the token of the match it grows
// to, also when its memo already holds that match from a lookahead.
TEST(PrecedenceTest, Precedence_climbing_with_a_left_recursive_operator_rule) {
  struct {
    const char *grammar;
    const char *input;
    const char *expected;
  } cases[] = {
      {R"(
        E <- A (O A)* { precedence L + - L * }
        O <- O '!' / < [-+*] >
        A <- N F
        F <- &(O 'x') / ''
        N <- < [0-9] >
      )",
       "1+2*3", "(1+(2*3))"},
      {R"(
        E <- A (O A)* { precedence L + - L * }
        O <- O '!' / < [-+*] >
        A <- F N
        F <- &('2' O) / ''
        N <- < [0-9] >
      )",
       "1+2*3", "(1+(2*3))"},
      {R"(
        E <- A (O A)* { precedence L * L + L ** }
        O <- O '*' / [-+*]
        A <- < [0-9] >
      )",
       "1**2+3", "((1**2)+3)"},
  };
  for (const auto &[grammar, input, expected] : cases) {
    for (auto packrat : {false, true}) {
      parser pg(grammar);
      ASSERT_TRUE(!!pg);
      if (packrat) { pg.enable_packrat_parsing(); }
      pg["A"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
      pg["O"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
      pg["E"] = [](const SemanticValues &vs) {
        if (vs.size() == 1) { return std::any_cast<std::string>(vs[0]); }
        return "(" + std::any_cast<std::string>(vs[0]) +
               std::any_cast<std::string>(vs[1]) +
               std::any_cast<std::string>(vs[2]) + ")";
      };

      std::string val;
      ASSERT_TRUE(pg.parse(input, val)) << input << " " << packrat;
      EXPECT_EQ(expected, val) << input << " " << packrat;
    }
  }
}

// A macro's body parses on its caller's values; its precedence fold must
// leave the values before it alone, also when the alternative it is in fails
// after the fold.
TEST(PrecedenceTest, Precedence_climbing_in_a_macro_keeps_the_callers_values) {
  parser pg(R"(
    S          <- A (EXPR(NUM, OP) '!' / REST) C
    EXPR(X, O) <- X (O X)* {
                    precedence
                      L +
                  }
    A          <- 'a'
    REST       <- < [0-9+ ]+ >
    C          <- 'c'
    NUM        <- < [0-9]+ >
    OP         <- < '+' >
    %whitespace <- [ \t]*
  )");
  ASSERT_TRUE(!!pg);

  pg["A"] = [](const SemanticValues &) { return std::string("A"); };
  pg["REST"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
  pg["C"] = [](const SemanticValues &) { return std::string("C"); };
  pg["EXPR"] = [](const SemanticValues &vs) {
    EXPECT_EQ(3u, vs.size());
    return std::string("E");
  };
  pg["S"] = [](const SemanticValues &vs) {
    std::string r;
    for (size_t i = 0; i < vs.size(); i++) {
      r += std::any_cast<std::string>(vs[i]) + ";";
    }
    return r;
  };

  std::string val;
  EXPECT_TRUE(pg.parse("a 1 + 2 ! c", val));
  EXPECT_EQ("A;E;C;", val);
  EXPECT_TRUE(pg.parse("a 1 + 2 c", val));
  EXPECT_EQ("A;1 + 2 ;C;", val);
}

// =============================================================================
// Precedence Edge Case Tests
// =============================================================================

TEST(PrecedenceEdgeTest, Precedence_with_unary) {
  parser pg(R"(
    EXPR           <- INFIX_EXPR(UNARY, OPE)
    INFIX_EXPR(A, O) <- A (O A)* {
      precedence
        L + -
        L * /
    }
    UNARY          <- '-'? ATOM
    ATOM           <- < [0-9]+ > / '(' EXPR ')'
    OPE            <- < [-+*/] >
    %whitespace    <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("1 + 2 * 3"));
  EXPECT_TRUE(pg.parse("-1 + 2"));
  EXPECT_TRUE(pg.parse("(1 + 2) * 3"));
}

// =============================================================================
// Macro Edge Cases
// =============================================================================

// =============================================================================
// Backreference Tests
// =============================================================================

TEST(BackreferenceTest, Backreference_test) {
  parser parser(R"(
        START  <- _ LQUOTE < (!RQUOTE .)* > RQUOTE _
        LQUOTE <- 'R"' $delm< [a-zA-Z]* > '('
        RQUOTE <- ')' $delm '"'
        ~_     <- [ \t\r\n]*
    )");

  std::string token;
  parser["START"] = [&](const SemanticValues &vs) { token = vs.token(); };

  {
    token.clear();
    auto ret = parser.parse(R"delm(
            R"("hello world")"
        )delm");

    EXPECT_TRUE(ret);
    EXPECT_EQ("\"hello world\"", token);
  }

  {
    token.clear();
    auto ret = parser.parse(R"delm(
            R"foo("(hello world)")foo"
        )delm");

    EXPECT_TRUE(ret);
    EXPECT_EQ("\"(hello world)\"", token);
  }

  {
    token.clear();
    auto ret = parser.parse(R"delm(
            R"foo("(hello world)foo")foo"
        )delm");

    EXPECT_FALSE(ret);
    EXPECT_EQ("\"(hello world", token);
  }

  {
    token.clear();
    auto ret = parser.parse(R"delm(
            R"foo("(hello world)")bar"
        )delm");

    EXPECT_FALSE(ret);
    EXPECT_TRUE(token.empty());
  }
}

TEST(BackreferenceTest, Undefined_backreference_test) {
  parser parser("S <- $bref");
  EXPECT_FALSE(parser);
}

TEST(BackreferenceTest, Invalid_backreference_test) {
  parser parser(R"(
        START  <- _ LQUOTE (!RQUOTE .)* RQUOTE _
        LQUOTE <- 'R"' $delm< [a-zA-Z]* > '('
        RQUOTE <- ')' $delm2 '"'
        ~_     <- [ \t\r\n]*
    )");

  EXPECT_FALSE(parser);
  EXPECT_FALSE(parser.parse(R"delm(
            R"foo("(hello world)")foo"
        )delm"));
}

TEST(BackreferenceTest, Nested_capture_test) {
  parser parser(R"(
        ROOT      <- CONTENT
        CONTENT   <- (ELEMENT / TEXT)*
        ELEMENT   <- $(STAG CONTENT ETAG)
        STAG      <- '<' $tag< TAG_NAME > '>'
        ETAG      <- '</' $tag '>'
        TAG_NAME  <- 'b' / 'u'
        TEXT      <- TEXT_DATA
        TEXT_DATA <- ![<] .
    )");

  EXPECT_TRUE(!!parser);
  EXPECT_TRUE(parser.parse("This is <b>a <u>test</u> text</b>."));
  EXPECT_FALSE(parser.parse("This is <b>a <u>test</b> text</u>."));
  EXPECT_FALSE(parser.parse("This is <b>a <u>test text</b>."));
  EXPECT_FALSE(parser.parse("This is a <u>test</u> text</b>."));
}

TEST(BackreferenceTest, Backreference_with_Prioritized_Choice_test) {
  parser parser(R"(
        TREE           <- WRONG_BRANCH / CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

  EXPECT_TRUE(!!parser);
  EXPECT_FALSE(parser.parse("branchthatiscorrect"));
}

TEST(BackreferenceTest, Backreference_with_Zero_or_More_test) {
  parser parser(R"(
        TREE           <- WRONG_BRANCH* CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

  EXPECT_TRUE(!!parser);
  EXPECT_TRUE(parser.parse("branchthatiswrongbranchthatiscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiswrongbranchthatIscorrect"));
  EXPECT_FALSE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
  EXPECT_TRUE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiswron_branchthatiscorrect"));
}

TEST(BackreferenceTest, Backreference_with_One_or_More_test) {
  parser parser(R"(
        TREE           <- WRONG_BRANCH+ CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

  EXPECT_TRUE(!!parser);
  EXPECT_TRUE(parser.parse("branchthatiswrongbranchthatiscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiswrongbranchthatIscorrect"));
  EXPECT_FALSE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
  EXPECT_TRUE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiswron_branchthatiscorrect"));
}

TEST(BackreferenceTest, Backreference_with_Option_test) {
  parser parser(R"(
        TREE           <- WRONG_BRANCH? CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

  EXPECT_TRUE(!!parser);
  EXPECT_TRUE(parser.parse("branchthatiswrongbranchthatiscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiswrongbranchthatIscorrect"));
  EXPECT_FALSE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
  EXPECT_FALSE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiscorrect"));
  EXPECT_FALSE(parser.parse("branchthatiswron_branchthatiscorrect"));
}

// =============================================================================
// Backreference Edge Case Tests
// =============================================================================

TEST(BackrefEdgeTest, Backreference_in_sequence) {
  parser pg(R"(
    S <- $tag<[a-z]+> '=' $tag
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("abc=abc"));
  EXPECT_FALSE(pg.parse("abc=def"));
}

TEST(BackrefEdgeTest, Backreference_with_choices) {
  parser pg(R"(
    S    <- OPEN CONTENT CLOSE
    OPEN <- $delim<'(' / '[' / '{'>
    CLOSE <- $delim
    CONTENT <- [a-z]+
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("(hello("));
  EXPECT_TRUE(pg.parse("[hello["));
  EXPECT_FALSE(pg.parse("(hello["));
}

// =============================================================================
// Precedence Climbing Edge Cases

// =============================================================================
// Dictionary Tests
// =============================================================================

TEST(DicTest, Dictionary) {
  parser parser(R"(
        START <- 'This month is ' MONTH '.'
        MONTH <- 'Jan' | 'January' | 'Feb' | 'February'
	)");

  EXPECT_TRUE(parser.parse("This month is Jan."));
  EXPECT_TRUE(parser.parse("This month is January."));
  EXPECT_FALSE(parser.parse("This month is Jannuary."));
  EXPECT_FALSE(parser.parse("This month is ."));
}

TEST(DicTest, Dictionary_invalid) {
  parser parser(R"(
        START <- 'This month is ' MONTH '.'
        MONTH <- 'Jan' | 'January' | [a-z]+ | 'Feb' | 'February'
	)");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(DicTest, Dictionary_index) {
  parser parser(R"(
        START <- 'This month is ' MONTH '.'
        MONTH <- 'Jan' | 'January' | 'Feb' | 'February'
	)");

  parser["MONTH"] = [](const SemanticValues &vs) {
    EXPECT_EQ("Feb", vs.token());
    EXPECT_EQ(2, vs.choice());
  };

  EXPECT_TRUE(parser.parse("This month is Feb."));
}

// =============================================================================
// Dictionary Edge Case Tests
// =============================================================================

TEST(DictionaryTest, Dictionary_longest_match) {
  parser pg(R"(
    S <- MONTH
    MONTH <- 'Jan' | 'January' | 'Jun' | 'June' | 'Jul' | 'July'
  )");
  EXPECT_TRUE(pg);

  // Dictionary tries longest match
  EXPECT_TRUE(pg.parse("January"));
  EXPECT_TRUE(pg.parse("Jan"));
  EXPECT_TRUE(pg.parse("June"));
}

TEST(DictionaryTest, Dictionary_choice_index) {
  parser pg(R"(
    S <- COLOR
    COLOR <- 'red' | 'green' | 'blue'
  )");
  EXPECT_TRUE(pg);

  // Dictionary reports choice index for matched item
  pg["COLOR"] = [](const SemanticValues &vs) {
    EXPECT_EQ("blue", vs.token());
    EXPECT_EQ(2, vs.choice()); // 'blue' is index 2
  };

  EXPECT_TRUE(pg.parse("blue"));
}

// =============================================================================
// Backreference Edge Cases

// =============================================================================
// Repetition Tests
// =============================================================================

TEST(RepetitionTest, Repetition_0) {
  parser parser(R"(
        START <- '(' DIGIT{3} ') ' DIGIT{3} '-' DIGIT{4}
        DIGIT <- [0-9]
    )");
  EXPECT_TRUE(parser.parse("(123) 456-7890"));
  EXPECT_FALSE(parser.parse("(12a) 456-7890"));
  EXPECT_FALSE(parser.parse("(123) 45-7890"));
  EXPECT_FALSE(parser.parse("(123) 45-7a90"));
}

TEST(RepetitionTest, Repetition_2_4) {
  parser parser(R"(
        START <- DIGIT{2,4}
        DIGIT <- [0-9]
    )");
  EXPECT_FALSE(parser.parse("1"));
  EXPECT_TRUE(parser.parse("12"));
  EXPECT_TRUE(parser.parse("123"));
  EXPECT_TRUE(parser.parse("1234"));
  EXPECT_FALSE(parser.parse("12345"));
}

TEST(RepetitionTest, Repetition_2_1) {
  parser parser(R"(
        START <- DIGIT{2,1} # invalid range
        DIGIT <- [0-9]
    )");
  EXPECT_FALSE(parser.parse("1"));
  EXPECT_TRUE(parser.parse("12"));
  EXPECT_FALSE(parser.parse("123"));
}

TEST(RepetitionTest, Repetition_2) {
  parser parser(R"(
        START <- DIGIT{2,}
        DIGIT <- [0-9]
    )");
  EXPECT_FALSE(parser.parse("1"));
  EXPECT_TRUE(parser.parse("12"));
  EXPECT_TRUE(parser.parse("123"));
  EXPECT_TRUE(parser.parse("1234"));
}

TEST(RepetitionTest, Repetition__2) {
  parser parser(R"(
        START <- DIGIT{,2}
        DIGIT <- [0-9]
    )");
  EXPECT_TRUE(parser.parse("1"));
  EXPECT_TRUE(parser.parse("12"));
  EXPECT_FALSE(parser.parse("123"));
  EXPECT_FALSE(parser.parse("1234"));
}

// =============================================================================
// Repetition Edge Case Tests
// =============================================================================

TEST(RepetitionEdgeTest, Zero_or_more_empty_match_in_sequence) {
  parser pg(R"(
    S <- [a-z]* [0-9]+
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("abc123"));
  EXPECT_TRUE(pg.parse("123"));  // zero letters, some digits
  EXPECT_FALSE(pg.parse("abc")); // letters but no digits
}

TEST(RepetitionEdgeTest, One_or_more_minimum) {
  parser pg(R"(S <- [a-z]+)");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("a"));
  EXPECT_FALSE(pg.parse(""));
}

TEST(RepetitionEdgeTest, Nested_repetitions) {
  parser pg(R"(
    S <- LINE+
    LINE <- WORD+ '\n'?
    WORD <- < [a-z]+ > ' '?
  )");
  EXPECT_TRUE(pg);

  std::vector<std::string> words;
  pg["WORD"] = [&](const SemanticValues &vs) {
    words.push_back(std::string(vs.token()));
  };

  EXPECT_TRUE(pg.parse("hello world\nfoo bar"));
  EXPECT_EQ(4, words.size());
}

// =============================================================================
// Error Message Quality Tests

// =============================================================================
// User-Defined Rule Tests
// =============================================================================

TEST(UserRuleTest, User_defined_rule_test) {
  auto g = parser(R"(
        ROOT <- _ 'Hello' _ NAME '!' _
    )",
                  {{"NAME", usr([](const char *s, size_t n, SemanticValues &,
                                   std::any &) -> size_t {
                      static std::vector<std::string> names = {"PEG", "BNF"};
                      for (const auto &name : names) {
                        if (name.size() <= n &&
                            !name.compare(0, name.size(), s, name.size())) {
                          return name.size();
                        }
                      }
                      return static_cast<size_t>(-1);
                    })},
                   {"~_", zom(cls(" \t\r\n"))}});

  EXPECT_TRUE(g.parse(" Hello BNF! "));
}
TEST(UserRuleTest, User_rule_with_state) {
  int call_count = 0;
  auto g = parser(R"(
    ROOT <- CUSTOM+
  )",
                  {{"CUSTOM", usr([&](const char *s, size_t n, SemanticValues &,
                                      std::any &) -> size_t {
                      call_count++;
                      if (n > 0 && (s[0] >= 'a' && s[0] <= 'z')) { return 1; }
                      return static_cast<size_t>(-1);
                    })}});

  EXPECT_TRUE(g.parse("abc"));
  // Called 3 times for 'a','b','c' + 1 failure at end to terminate '+'
  EXPECT_EQ(4, call_count);
}

TEST(UserRuleTest, User_rule_failure) {
  auto g = parser(R"(
    ROOT <- CUSTOM 'end'
  )",
                  {{"CUSTOM", usr([](const char *, size_t, SemanticValues &,
                                     std::any &) -> size_t {
                      return static_cast<size_t>(-1); // always fail
                    })}});

  EXPECT_FALSE(g.parse("anything"));
}

TEST(UserRuleTest, User_rule_with_semantic_value) {
  auto g =
      parser(R"(
    ROOT <- NUM
  )",
             {{"NUM", usr([](const char *s, size_t n, SemanticValues & /*vs*/,
                             std::any &) -> size_t {
                 size_t i = 0;
                 while (i < n && s[i] >= '0' && s[i] <= '9')
                   i++;
                 if (i == 0) return static_cast<size_t>(-1);
                 return i;
               })}});

  EXPECT_TRUE(g.parse("123"));
  EXPECT_FALSE(g.parse("abc"));
}

TEST(UserRuleTest, User_rule_combined_with_peg_rules) {
  auto g = parser(R"(
    ROOT <- GREETING CUSTOM '!'
    GREETING <- 'Hello' _
    ~_ <- [ \t]*
  )",
                  {{"CUSTOM", usr([](const char *s, size_t n, SemanticValues &,
                                     std::any &) -> size_t {
                      // Match uppercase words
                      size_t i = 0;
                      while (i < n && s[i] >= 'A' && s[i] <= 'Z')
                        i++;
                      return i > 0 ? i : static_cast<size_t>(-1);
                    })}});

  EXPECT_TRUE(g.parse("Hello WORLD!"));
  EXPECT_FALSE(g.parse("Hello world!"));
}

// =============================================================================
// Dictionary Precedence Tests
// =============================================================================

TEST(DictionaryPrecedenceTest, Pipe_binds_tighter_than_slash) {
  // `|` binds tighter than `/`: 'a' | 'b' / 'a' 'c' means
  // ('a' | 'b') / ('a' 'c'), so "ac" fails once 'a' wins the first alternative.
  parser pg(R"(S <- 'a' | 'b' / 'a' 'c')");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("a"));
  EXPECT_TRUE(pg.parse("b"));
  EXPECT_FALSE(pg.parse("ac"));
  EXPECT_FALSE(pg.parse("c"));
}

TEST(DictionaryPrecedenceTest, Pipe_binds_tighter_than_sequence) {
  // `|` binds tighter than sequence: 'a' 'b' | 'c' means 'a' ('b' | 'c').
  parser pg(R"(S <- 'a' 'b' | 'c')");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("ab"));
  EXPECT_TRUE(pg.parse("ac"));
  EXPECT_FALSE(pg.parse("c"));
  EXPECT_FALSE(pg.parse("a"));
}

TEST(DictionaryPrecedenceTest, Pure_dictionary) {
  // A run of `|` literals is a dictionary.
  parser pg(R"(S <- 'a' | 'bb' | 'ccc')");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("a"));
  EXPECT_TRUE(pg.parse("bb"));
  EXPECT_TRUE(pg.parse("ccc"));
  EXPECT_FALSE(pg.parse("b"));
}

TEST(DictionaryPrecedenceTest, Dictionary_case_insensitive) {
  // Case-insensitive dictionary members.
  parser pg(R"(S <- 'cat'i | 'dog'i)");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("cat"));
  EXPECT_TRUE(pg.parse("CAT"));
  EXPECT_TRUE(pg.parse("Dog"));
  EXPECT_FALSE(pg.parse("fish"));
}

TEST(DictionaryPrecedenceTest, Mixed_pipe_slash_chain) {
  // Mixing `|` and `/` across a choice chain.
  parser pg(R"(S <- 'a' | 'b' / 'c' | 'd')");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("a"));
  EXPECT_TRUE(pg.parse("b"));
  EXPECT_TRUE(pg.parse("c"));
  EXPECT_TRUE(pg.parse("d"));
  EXPECT_FALSE(pg.parse("e"));
}

TEST(DictionaryPrecedenceTest, Dictionary_ast) {
  // A parenthesized dictionary is one alternative of the outer choice: both
  // 'a' and 'b' give S/0.
  parser pg(R"(
    S <- ('a' | 'b') C / D
    C <- < 'c' >
    D <- < 'd' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("ac", ast));
  EXPECT_EQ(R"(+ S/0
  - C (c)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("bc", ast));
  EXPECT_EQ(R"(+ S/0
  - C (c)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("d", ast));
  EXPECT_EQ(R"(+ S/1
  - D (d)
)",
            ast_to_s(ast));
}
