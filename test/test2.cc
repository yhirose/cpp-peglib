#include <gtest/gtest.h>
#include <peglib.h>
#include <sstream>

using namespace peg;

TEST(TokenBoundaryTest, Token_boundary_1) {
  parser pg(R"(
        ROOT        <- TOP
        TOP         <- 'a' 'b' 'c'
        %whitespace <- [ \t\r\n]*
    )");

  EXPECT_TRUE(pg.parse(" a  b  c "));
}

TEST(TokenBoundaryTest, Token_boundary_2) {
  parser pg(R"(
        ROOT        <- TOP
        TOP         <- < 'a' 'b' 'c' >
        %whitespace <- [ \t\r\n]*
    )");

  EXPECT_FALSE(pg.parse(" a  b  c "));
}

TEST(TokenBoundaryTest, Token_boundary_3) {
  parser pg(R"(
        ROOT        <- TOP
        TOP         <- < 'a' B 'c' >
        B           <- 'b'
        %whitespace <- [ \t\r\n]*
    )");

  EXPECT_FALSE(pg.parse(" a  b  c "));
}

TEST(TokenBoundaryTest, Token_boundary_4) {
  parser pg(R"(
        ROOT        <- TOP
        TOP         <- < A 'b' 'c' >
        A           <- 'a'
        %whitespace <- [ \t\r\n]*
    )");

  EXPECT_FALSE(pg.parse(" a  b  c "));
}

TEST(TokenBoundaryTest, Token_boundary_5) {
  parser pg(R"(
        ROOT        <- TOP
        TOP         <- A < 'b' C >
        A           <- 'a'
        C           <- 'c'
        %whitespace <- [ \t\r\n]*
    )");

  EXPECT_FALSE(pg.parse(" a  b  c "));
}

TEST(TokenBoundaryTest, Token_boundary_6) {
  parser pg(R"(
        ROOT        <- TOP
        TOP         <- < A > B C
        A           <- 'a'
        B           <- 'b'
        C           <- 'c'
        %whitespace <- [ \t\r\n]*
    )");

  EXPECT_TRUE(pg.parse(" a  b  c "));
}

TEST(TokenBoundaryTest, Token_boundary_7) {
  parser pg(R"(
        ROOT        <- TOP
        TOP         <- < A B C >
        A           <- 'a'
        B           <- 'b'
        C           <- 'c'
        %whitespace <- [ \t\r\n]*
    )");

  EXPECT_FALSE(pg.parse(" a  b  c "));
}

TEST(InfiniteLoopTest, Infinite_loop_1) {
  parser pg(R"(
        ROOT  <- WH TOKEN* WH
        TOKEN <- [a-z0-9]*
        WH    <- [ \t]*
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_2) {
  parser pg(R"(
        ROOT  <- WH TOKEN+ WH
        TOKEN <- [a-z0-9]*
        WH    <- [ \t]*
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_3) {
  parser pg(R"(
        ROOT  <- WH TOKEN* WH
        TOKEN <- !'word1'
        WH    <- [ \t]*
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_4) {
  parser pg(R"(
        ROOT  <- WH TOKEN* WH
        TOKEN <- &'word1'
        WH    <- [ \t]*
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_5) {
  parser pg(R"(
        Numbers <- Number*
        Number <- [0-9]+ / Spacing
        Spacing <- ' ' / '\t' / '\n' / EOF # EOF is empty
        EOF <- !.
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_6) {
  parser pg(R"(
        S <- ''*
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_7) {
  parser pg(R"(
        S <- A*
        A <- ''
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_8) {
  parser pg(R"(
        ROOT <- ('A' /)*
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Infinite_loop_9) {
  parser pg(R"(
        ROOT <- %recover(('A' /)*)
    )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, Not_infinite_1) {
  parser pg(R"(
        Numbers <- Number* EOF
        Number <- [0-9]+ / Spacing
        Spacing <- ' ' / '\t' / '\n'
        EOF <- !.
    )");

  EXPECT_TRUE(!!pg);
}

TEST(InfiniteLoopTest, Not_infinite_2) {
  parser pg(R"(
        ROOT      <-  _ ('[' TAG_NAME ']' _)*
        # In a sequence operator, if there is at least one non-empty element, we can treat it as non-empty
        TAG_NAME  <-  (!']' .)+
        _         <-  [ \t]*
    )");

  EXPECT_TRUE(!!pg);
}

TEST(InfiniteLoopTest, Not_infinite_3) {
  parser pg(R"(
        EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _ # Recursive...
        TERM_OPERATOR    <-  < [-+] > _
        FACTOR_OPERATOR  <-  < [*/] > _
        NUMBER           <-  < [0-9]+ > _
        _                <-  [ \t\r\n]*
    )");

  EXPECT_TRUE(!!pg);
}

TEST(InfiniteLoopTest, whitespace) {
  parser pg(R"(
    S <- 'hello'
    %whitespace <- ('')*
  )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, word) {
  parser pg(R"(
    S <- 'hello'
    %whitespace <- ' '*
    %word <- ('')*
  )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, in_sequence) {
  parser pg(R"(
    S <- A*
    A <- 'a' B
    B <- 'a' ''*
  )");

  EXPECT_FALSE(pg);
}

TEST(InfiniteLoopTest, in_prioritized_choice) {
  parser pg(R"(
    S <- A*
    A <- 'a' / 'b' B
    B <- 'a' ''*
  )");

  EXPECT_FALSE(pg);
}

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

TEST(LeftRecursiveTest, Left_recursive_test) {
  parser parser(R"(
        A <- A 'a'
        B <- A 'a'
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursiveTest, Left_recursive_with_option_test) {
  parser parser(R"(
        A  <- 'a' / 'b'? B 'c'
        B  <- A
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursiveTest, Left_recursive_with_zom_test) {
  parser parser(R"(
        A <- 'a'* A*
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursiveTest, Left_recursive_with_a_ZOM_content_rule) {
  parser parser(R"(
        A <- B
        B <- _ A
        _ <- ' '* # Zero or more
    )");

  EXPECT_FALSE(parser);
}

TEST(LeftRecursiveTest, Left_recursive_with_empty_string_test) {
  parser parser(" A <- '' A");

  EXPECT_FALSE(parser);
}

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

TEST(PredicateTest, Semantic_predicate_test) {
  parser parser("NUMBER  <-  [0-9]+");

  parser["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  parser["NUMBER"].predicate = [](const SemanticValues &vs,
                                  const std::any & /*dt*/, std::string &msg) {
    if (vs.token_to_number<long>() != 100) {
      msg = "value error!!";
      return false;
    }
    return true;
  };

  long val;
  EXPECT_TRUE(parser.parse("100", val));
  EXPECT_EQ(100, val);

  parser.set_logger([](size_t line, size_t col, const std::string &msg) {
    EXPECT_EQ(1, line);
    EXPECT_EQ(1, col);
    EXPECT_EQ("value error!!", msg);
  });
  EXPECT_FALSE(parser.parse("200", val));
}

#ifdef CPPPEGLIB_SYMBOL_TABLE_SUPPORT
TEST(SymbolTableTest, symbol_instruction_test) {
  parser parser(R"(S            <- (Decl / Ref)*
Decl         <- 'decl' symbol
Ref          <- 'ref' is_symbol
Name         <- < [a-zA-Z]+ >
%whitespace  <- [ \t\r\n]*

symbol       <- Name { declare_symbol var_table }
is_symbol    <- Name { check_symbol var_table }
)");

  {
    const auto source = R"(decl aaa
ref aaa
ref bbb
)";
    parser.set_logger([](size_t line, size_t col, const std::string &msg) {
      EXPECT_EQ(3, line);
      EXPECT_EQ(5, col);
      EXPECT_EQ("'bbb' doesn't exist.", msg);
    });
    EXPECT_FALSE(parser.parse(source));
  }

  {
    const auto source = R"(decl aaa
ref aaa
decl aaa
)";
    parser.set_logger([](size_t line, size_t col, const std::string &msg) {
      EXPECT_EQ(3, line);
      EXPECT_EQ(6, col);
      EXPECT_EQ("'aaa' already exists.", msg);
    });
    EXPECT_FALSE(parser.parse(source));
  }
}

TEST(SymbolTableTest, symbol_instruction_backtrack_test) {
  parser parser(R"(S            <- (DeclBT / Decl / Ref)*
DeclBT       <- 'decl' symbol 'backtrack' # match fails, so symbol should not be set
Decl         <- 'decl' symbol
Ref          <- 'ref' is_symbol
Name         <- < [a-zA-Z]+ >
%whitespace  <- [ \t\r\n]*

# 'var_table' is a table name.
symbol       <- Name { declare_symbol var_table } # Declare symbol instruction
is_symbol    <- Name { check_symbol var_table }   # Check symbol instruction
)");

  const auto source = R"(decl foo
ref foo
)";
  EXPECT_TRUE(parser.parse(source));
}

TEST(SymbolTableTest, symbol_instruction_backtrack_test2) {
  parser parser(R"(S            <- DeclBT* Decl Ref
DeclBT       <- 'decl' symbol 'backtrack' # match fails, so symbol should not be set
Decl         <- 'decl' symbol
Ref          <- 'ref' is_symbol
Name         <- < [a-zA-Z]+ >
%whitespace  <- [ \t\r\n]*

# 'var_table' is a table name.
symbol       <- Name { declare_symbol var_table } # Declare symbol instruction
is_symbol    <- Name { check_symbol var_table }   # Check symbol instruction
)");

  const auto source = R"(decl foo
ref foo
)";
  EXPECT_TRUE(parser.parse(source));
}

TEST(SymbolTableTest, typedef_test) {
  parser parser(R"(
S            <- (Decl / TypeDef)*
Decl         <- 'decl' type
TypeDef      <- 'typedef' type_ref type ';'
type         <- Name { declare_symbol type_table }
type_ref     <- Name { check_symbol type_table }

Name         <- < [a-zA-Z0-9_]+ >
%whitespace  <- [ \t\r\n]*
)");

  {
    const auto source = R"(decl long
typedef long __off64_t;
typedef __off64_t __loff_t;
)";
    EXPECT_TRUE(parser.parse(source));
  }

  {
    const auto source = R"(decl long
typedef long __off64_t;
typedef __off64_T __loff_t;
)";
    parser.set_logger([](size_t line, size_t col, const std::string &msg) {
      EXPECT_EQ(3, line);
      EXPECT_EQ(9, col);
      EXPECT_EQ("'__off64_T' doesn't exist.", msg);
    });
    EXPECT_FALSE(parser.parse(source));
  }

  {
    const auto source = R"(decl long
typedef long __off64_t;
typedef __off64_t __loff_t;
typedef __off64_t __loff_t;
)";
    parser.set_logger([](size_t line, size_t col, const std::string &msg) {
      EXPECT_EQ(4, line);
      EXPECT_EQ(19, col);
      EXPECT_EQ("'__loff_t' already exists.", msg);
    });
    EXPECT_FALSE(parser.parse(source));
  }
}

TEST(SymbolTableTest, predicate_test) {
  parser parser(R"(
S            <- (Decl / Ref)*
Decl         <- 'decl' symbol
Ref          <- 'ref' is_symbol
Name         <- < [a-zA-Z]+ >
%whitespace  <- [ \t\r\n]*

# These must be tokens.
symbol       <- < Name >
is_symbol    <- < Name >
)");

  std::set<std::string> dic;

  parser[R"(symbol)"].predicate =
      [&](const SemanticValues &vs, const std::any & /*dt*/, std::string &msg) {
        auto tok = vs.token_to_string();
        if (dic.find(tok) != dic.end()) {
          msg = "'" + tok + "' already exists.";
          return false;
        }
        dic.insert(tok);
        return true;
      };

  parser[R"(is_symbol)"].predicate =
      [&](const SemanticValues &vs, const std::any & /*dt*/, std::string &msg) {
        auto tok = vs.token_to_string();
        if (dic.find(tok) == dic.end()) {
          msg = "'" + tok + "' doesn't exist.";
          return false;
        }
        return true;
      };

  parser.enable_ast();

  {
    const auto source = R"(decl aaa
ref aaa
ref bbb
)";
    parser.set_logger([](size_t line, size_t col, const std::string &msg) {
      EXPECT_EQ(3, line);
      EXPECT_EQ(5, col);
      EXPECT_EQ("'bbb' doesn't exist.", msg);
    });
    std::shared_ptr<Ast> ast;
    dic.clear();
    EXPECT_FALSE(parser.parse(source, ast));
  }

  {
    const auto source = R"(decl aaa
ref aaa
decl aaa
)";
    parser.set_logger([](size_t line, size_t col, const std::string &msg) {
      std::cerr << line << ":" << col << ": " << msg << "\n";
      EXPECT_EQ(3, line);
      EXPECT_EQ(6, col);
      EXPECT_EQ("'aaa' already exists.", msg);
    });
    std::shared_ptr<Ast> ast;
    dic.clear();
    EXPECT_FALSE(parser.parse(source, ast));
  }
}
#endif

TEST(UnicodeTest, Japanese_character) {
  peg::parser parser(u8R"(
        文 <- 修飾語? 主語 述語 '。'
        主語 <- 名詞 助詞
        述語 <- 動詞 助詞
        修飾語 <- 形容詞
        名詞 <- 'サーバー' / 'クライアント'
        形容詞 <- '古い' / '新しい'
        動詞 <- '落ち' / '復旧し'
        助詞 <- 'が' / 'を' / 'た' / 'ます' / 'に'
    )");

  bool ret = parser;
  EXPECT_TRUE(ret);

  EXPECT_TRUE(parser.parse(u8R"(サーバーを復旧します。)"));
}

TEST(UnicodeTest, dot_with_a_code) {
  peg::parser parser(" S <- 'a' . 'b' ");
  EXPECT_TRUE(parser.parse(u8R"(aあb)"));
}

TEST(UnicodeTest, dot_with_a_char) {
  peg::parser parser(" S <- 'a' . 'b' ");
  EXPECT_TRUE(parser.parse(u8R"(aåb)"));
}

TEST(UnicodeTest, character_class) {
  peg::parser parser(R"(
        S <- 'a' [い-おAさC-Eた-とは] 'b'
    )");

  bool ret = parser;
  EXPECT_TRUE(ret);

  EXPECT_FALSE(parser.parse(u8R"(aあb)"));
  EXPECT_TRUE(parser.parse(u8R"(aいb)"));
  EXPECT_TRUE(parser.parse(u8R"(aうb)"));
  EXPECT_TRUE(parser.parse(u8R"(aおb)"));
  EXPECT_FALSE(parser.parse(u8R"(aかb)"));
  EXPECT_TRUE(parser.parse(u8R"(aAb)"));
  EXPECT_FALSE(parser.parse(u8R"(aBb)"));
  EXPECT_TRUE(parser.parse(u8R"(aEb)"));
  EXPECT_FALSE(parser.parse(u8R"(aFb)"));
  EXPECT_FALSE(parser.parse(u8R"(aそb)"));
  EXPECT_TRUE(parser.parse(u8R"(aたb)"));
  EXPECT_TRUE(parser.parse(u8R"(aちb)"));
  EXPECT_TRUE(parser.parse(u8R"(aとb)"));
  EXPECT_FALSE(parser.parse(u8R"(aなb)"));
  EXPECT_TRUE(parser.parse(u8R"(aはb)"));
  EXPECT_FALSE(parser.parse(u8R"(a?b)"));
}

#if 0 // TODO: Unicode Grapheme support
TEST(UnicodeTest, dot_with_a_grapheme)
{
    peg::parser parser(" S <- 'a' . 'b' ");
    EXPECT_TRUE(parser.parse(u8R"(aसिb)"));
}
#endif

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

TEST(LineInformationTest, Line_information_test) {
  parser parser(R"(
        S    <- _ (WORD _)+
        WORD <- [A-Za-z]+
        ~_   <- [ \t\r\n]+
    )");

  std::vector<std::pair<size_t, size_t>> locations;
  parser["WORD"] = [&](const peg::SemanticValues &vs) {
    locations.push_back(vs.line_info());
  };

  bool ret = parser;
  EXPECT_TRUE(ret);

  ret = parser.parse(" Mon Tue Wed \nThu  Fri  Sat\nSun\n");
  EXPECT_TRUE(ret);

  {
    auto val = std::make_pair<size_t, size_t>(1, 2);
    EXPECT_TRUE(val == locations[0]);
  }
  {
    auto val = std::make_pair<size_t, size_t>(1, 6);
    EXPECT_TRUE(val == locations[1]);
  }
  {
    auto val = std::make_pair<size_t, size_t>(1, 10);
    EXPECT_TRUE(val == locations[2]);
  }
  {
    auto val = std::make_pair<size_t, size_t>(2, 1);
    EXPECT_TRUE(val == locations[3]);
  }
  {
    auto val = std::make_pair<size_t, size_t>(2, 6);
    EXPECT_TRUE(val == locations[4]);
  }
  {
    auto val = std::make_pair<size_t, size_t>(2, 11);
    EXPECT_TRUE(val == locations[5]);
  }
  {
    auto val = std::make_pair<size_t, size_t>(3, 1);
    EXPECT_TRUE(val == locations[6]);
  }
}

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

TEST(ErrorTest, Default_error_handling_1) {
  parser pg(R"(
    S <- '@' A B
    A <- < [a-z]+ >
    B <- 'hello' | 'world'
    %whitespace <- [ ]*
    %word       <- [a-z]
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(1:8: syntax error, unexpected 'typo', expecting <B>.)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  EXPECT_FALSE(pg.parse(" @ aaa typo "));
  EXPECT_EQ(i, errors.size());
}

TEST(ErrorTest, Default_error_handling_2) {
  parser pg(R"(
    S <- '@' A B
    A <- < [a-z]+ >
    B <- 'hello' / 'world'
    %whitespace <- ' '*
    %word       <- [a-z]
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(1:8: syntax error, unexpected 'typo', expecting 'hello', 'world'.)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  EXPECT_FALSE(pg.parse(" @ aaa typo "));
  EXPECT_EQ(i, errors.size());
}

TEST(ErrorTest, Default_error_handling_fiblang) {
  parser pg(R"(
    # Syntax
    START             ← STATEMENTS
    STATEMENTS        ← (DEFINITION / EXPRESSION)*
    DEFINITION        ← 'def' ↑ Identifier '(' Identifier ')' EXPRESSION
    EXPRESSION        ← TERNARY
    TERNARY           ← CONDITION ('?' EXPRESSION (':' / %recover(col)) EXPRESSION)?
    CONDITION         ← INFIX (ConditionOperator INFIX)?
    INFIX             ← CALL (InfixOperator CALL)*
    CALL              ← PRIMARY ('(' EXPRESSION ')')?
    PRIMARY           ← FOR / Identifier / '(' ↑ EXPRESSION ')' / Number
    FOR               ← 'for' ↑ Identifier 'from' Number 'to' Number EXPRESSION

    # Token
    ConditionOperator ← '<'
    InfixOperator     ← '+' / '-'
    Identifier        ← !Keyword < [a-zA-Z][a-zA-Z0-9_]* >
    Number            ← < [0-9]+ >
    Keyword           ← 'def' / 'for' / 'from' / 'to'

    %whitespace       ← [ \t\r\n]*
    %word             ← [a-zA-Z]

    col ← '' { error_message "missing colon." }
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(4:7: syntax error, unexpected 'frm', expecting 'from'.)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  EXPECT_FALSE(pg.parse(R"(def fib(x)
  x < 2 ? 1 : fib(x - 2) + fib(x - 1)

for n frm 1 to 30
  puts(fib(n))
  )"));
  EXPECT_EQ(i, errors.size());
}

TEST(ErrorTest, Error_recovery_1) {
  parser pg(R"(
START      <- __? SECTION*

SECTION    <- HEADER __ ENTRIES __?

HEADER     <- '[' _ CATEGORY (':' _  ATTRIBUTES)? ']'^header

CATEGORY   <- < [-_a-zA-Z0-9\u0080-\uFFFF ]+ > _
ATTRIBUTES <- ATTRIBUTE (',' _ ATTRIBUTE)*
ATTRIBUTE  <- < [-_a-zA-Z0-9\u0080-\uFFFF]+ > _

ENTRIES    <- (ENTRY (__ ENTRY)*)? { no_ast_opt }

ENTRY      <- ONE_WAY PHRASE ('|' _ PHRASE)* !'='
            / PHRASE ('|' _ PHRASE)+ !'='
            / %recover(entry)

ONE_WAY    <- PHRASE '=' _
PHRASE     <- WORD (' ' WORD)* _
WORD       <- < (![ \t\r\n=|[\]#] .)+ >

~__        <- _ (comment? nl _)+
~_         <- [ \t]*

comment    <- ('#' (!nl .)*)
nl         <- '\r'? '\n'

header <- (!__ .)* { error_message "invalid section header, missing ']'." }
entry  <- (!(__ / HEADER) .)+ { error_message "invalid entry." }
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(3:1: invalid entry.)",
      R"(7:1: invalid entry.)",
      R"(10:11: invalid section header, missing ']'.)",
      R"(18:1: invalid entry.)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_FALSE(pg.parse(R"([Section 1]
111 = 222 | 333
aaa || bbb
ccc = ddd

[Section 2]
eee
fff | ggg

[Section 3
hhh | iii

[Section 日本語]
ppp | qqq

[Section 4]
jjj | kkk
lll = mmm | nnn = ooo

[Section 5]
rrr | sss

  )",
                        ast));

  EXPECT_EQ(i, errors.size());
  EXPECT_FALSE(ast);
}

TEST(ErrorTest, Error_recovery_2) {
  parser pg(R"(
    START <- ENTRY ((',' ENTRY) / %recover((!(',' / Space) .)+))* (_ / %recover(.*))
    ENTRY <- '[' ITEM (',' ITEM)* ']'
    ITEM  <- WORD / NUM / %recover((!(',' / ']') .)+)
    NUM   <- [0-9]+ ![a-z]
    WORD  <- '"' [a-z]+ '"'

    ~_    <- Space*
    Space <- [ \n]
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(1:6: syntax error, unexpected ']', expecting ','.)",
      R"(1:18: syntax error, unexpected 'z', expecting <NUM>.)",
      R"(1:24: syntax error, unexpected ',', expecting '"'.)",
      R"(1:31: syntax error, unexpected 'ccc', expecting '"', <NUM>.)",
      R"(1:38: syntax error, unexpected 'ddd', expecting '"', <NUM>.)",
      R"(1:55: syntax error, unexpected ']', expecting '"'.)",
      R"(1:58: syntax error, unexpected '\n', expecting '"', <NUM>.)",
      R"(2:3: syntax error, expecting ']'.)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_FALSE(
      pg.parse(R"([000]],[111],[222z,"aaa,"bbb",ccc"],[ddd",444,555,"eee],[
  )",
               ast));
  EXPECT_EQ(i, errors.size());

  EXPECT_FALSE(ast);
}

TEST(ErrorTest, Error_recovery_3) {
  parser pg(R"~(
# Grammar
START      <- __? SECTION*

SECTION    <- HEADER __ ENTRIES __?

HEADER     <- '['^missing_bracket _ CATEGORY (':' _  ATTRIBUTES)? ']'^missing_bracket ___

CATEGORY   <- < (&[-_a-zA-Z0-9\u0080-\uFFFF ] (![\u0080-\uFFFF])^vernacular_char .)+ > _
ATTRIBUTES <- ATTRIBUTE (',' _ ATTRIBUTE)*
ATTRIBUTE  <- < [-_a-zA-Z0-9]+ > _

ENTRIES    <- (ENTRY (__ ENTRY)*)? { no_ast_opt }

ENTRY      <- ONE_WAY PHRASE^expect_phrase (or _ PHRASE^expect_phrase)* ___
            / PHRASE (or^missing_or _ PHRASE^expect_phrase) (or _ PHRASE^expect_phrase)* ___ { no_ast_opt }

ONE_WAY    <- PHRASE assign _
PHRASE     <- WORD (' ' WORD)* _ { no_ast_opt }
WORD       <- < (![ \t\r\n=|[\]#] (![*?] / %recover(wildcard)) .)+ >

~assign    <- '=' ____
~or        <- '|' (!'|')^duplicate_or ____

~_         <- [ \t]*
~__        <- _ (comment? nl _)+
~___       <- (!operators)^invalid_ope
~____      <- (!operators)^invalid_ope_comb

operators  <- [|=]+
comment    <- ('#' (!nl .)*)
nl         <- '\r'? '\n'

# Recovery
duplicate_or     <- skip_puncs { error_message "Duplicate OR operator (|)" }
missing_or       <- '' { error_message "Missing OR operator (|)" }
missing_bracket  <- skip_puncs { error_message "Missing opening/closing square bracket" }
expect_phrase    <- skip { error_message "Expect phrase" }
invalid_ope_comb <- skip_puncs { error_message "Use of invalid operator combination" }
invalid_ope      <- skip { error_message "Use of invalid operator" }
wildcard         <- '' { error_message "Wildcard characters (%c) should not be used" }
vernacular_char  <- '' { error_message "Section name %c must be in English" }

skip             <- (!(__) .)*
skip_puncs       <- [|=]* _
  )~");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(3:7: Wildcard characters (*) should not be used)",
      R"(4:6: Wildcard characters (?) should not be used)",
      R"(5:6: Duplicate OR operator (|))",
      R"(9:4: Missing OR operator (|))",
      R"(11:16: Expect phrase)",
      R"(13:11: Missing opening/closing square bracket)",
      R"(16:10: Section name 日 must be in English)",
      R"(16:11: Section name 本 must be in English)",
      R"(16:12: Section name 語 must be in English)",
      R"(16:13: Section name で must be in English)",
      R"(16:14: Section name す must be in English)",
      R"(21:17: Use of invalid operator)",
      R"(24:10: Use of invalid operator combination)",
      R"(26:10: Missing OR operator (|))",
      R"(28:3: Missing opening/closing square bracket)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_FALSE(pg.parse(R"([Section 1]
111 = 222 | 333
AAA BB* | CCC
AAA B?B | CCC
aaa || bbb
ccc = ddd

[Section 2]
eee
fff | ggg
fff | ggg 111 |

[Section 3
hhh | iii

[Section 日本語です]
ppp | qqq

[Section 4]
jjj | kkk
lll = mmm | nnn = ooo

[Section 5]
ppp qqq |= rrr

Section 6]
sss | ttt
  )",
                        ast));

  EXPECT_EQ(i, errors.size());
  EXPECT_FALSE(ast);
}

TEST(ErrorTest, Error_recovery_Java) {
  parser pg(R"(
Prog       ← PUBLIC CLASS NAME LCUR PUBLIC STATIC VOID MAIN LPAR STRING LBRA RBRA NAME RPAR BlockStmt RCUR
BlockStmt  ← LCUR (Stmt)* RCUR^rcblk
Stmt       ← IfStmt / WhileStmt / PrintStmt / DecStmt / AssignStmt / BlockStmt
IfStmt     ← IF LPAR Exp RPAR Stmt (ELSE Stmt)?
WhileStmt  ← WHILE LPAR Exp RPAR Stmt
DecStmt    ← INT NAME (ASSIGN Exp)? SEMI
AssignStmt ← NAME ASSIGN Exp SEMI^semia
PrintStmt  ← PRINTLN LPAR Exp RPAR SEMI
Exp        ← RelExp (EQ RelExp)*
RelExp     ← AddExp (LT AddExp)*
AddExp     ← MulExp ((PLUS / MINUS) MulExp)*
MulExp     ← AtomExp ((TIMES / DIV) AtomExp)*
AtomExp    ← LPAR Exp RPAR / NUMBER / NAME

NUMBER     ← < [0-9]+ >
NAME       ← < [a-zA-Z_][a-zA-Z_0-9]* >

~LPAR       ← '('
~RPAR       ← ')'
~LCUR       ← '{'
~RCUR       ← '}'
~LBRA       ← '['
~RBRA       ← ']'
~SEMI       ← ';'

~EQ         ← '=='
~LT         ← '<'
~ASSIGN     ← '='

~IF         ← 'if'
~ELSE       ← 'else'
~WHILE      ← 'while'

PLUS       ← '+'
MINUS      ← '-'
TIMES      ← '*'
DIV        ← '/'

CLASS      ← 'class'
PUBLIC     ← 'public'
STATIC     ← 'static'

VOID       ← 'void'
INT        ← 'int'

MAIN       ← 'main'
STRING     ← 'String'
PRINTLN    ← 'System.out.println'

%whitespace ← [ \t\n]*
%word       ← NAME

# Throw operator labels
rcblk      ← SkipToRCUR { error_message "missing end of block." }
semia      ← '' { error_message "missing simicolon in assignment." }

# Recovery expressions
SkipToRCUR ← (!RCUR (LCUR SkipToRCUR / .))* RCUR
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(8:5: missing simicolon in assignment.)",
      R"(8:6: missing end of block.)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_FALSE(pg.parse(R"(public class Example {
  public static void main(String[] args) {
    int n = 5;
    int f = 1;
    while(0 < n) {
      f = f * n;
      n = n - 1
    };
    System.out.println(f);
  }
}
  )",
                        ast));

  EXPECT_FALSE(ast);
}

TEST(ErrorTest, Error_message_with_rule_instruction) {
  parser pg(R"(
START       <- (LINE (ln LINE)*)? ln?

LINE        <- STR '=' CODE

CODE        <- HEX / DEC { error_message 'code format error...' }
HEX         <- < [a-f0-9]+ 'h' >
DEC         <- < [0-9]+ >

STR         <- < [a-z0-9]+ >

~ln         <- [\r\n]+
%whitespace <- [ \t]*
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(2:5: code format error...)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());
  });

  EXPECT_FALSE(pg.parse(R"(1 = ah
2 = b
3 = ch
  )"));
  EXPECT_EQ(i, errors.size());
}

TEST(StateTest, Indent) {
  parser pg(R"(Start <- Statements {}
Statements <- Statement*
Statement <- Samedent (S / I)

S <- 'S' EOS { no_ast_opt }
I <- 'I' EOL Block / 'I' EOS { no_ast_opt }

Block <- Statements {}

~Samedent <- ' '* {}

~EOS <- EOL / EOF
~EOL <- '\n'
~EOF <- !.
  )");

  EXPECT_TRUE(!!pg);

  size_t indent = 0;

  pg["Block"].enter = [&](const Context & /*c*/, const char * /*s*/,
                          size_t /*n*/, std::any & /*dt*/) { indent += 2; };

  pg["Block"].leave = [&](const Context & /*c*/, const char * /*s*/,
                          size_t /*n*/, size_t /*matchlen*/,
                          std::any & /*value*/,
                          std::any & /*dt*/) { indent -= 2; };

  pg["Samedent"].predicate = [&](const SemanticValues &vs,
                                 const std::any & /*dt*/, std::string &msg) {
    if (indent != vs.sv().size()) {
      msg = "different indent...";
      return false;
    }
    return true;
  };

  pg.enable_ast();

  const auto source = R"(I
  S
  I
    I
      S
      S
    S
  S
)";

  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse(source, ast));

  ast = pg.optimize_ast(ast);
  auto s = ast_to_s(ast);

  EXPECT_EQ(R"(+ Start/0[I]
  + Block/0[Statements]
    + Statement/0[S]
    + Statement/0[I]
      + Block/0[Statements]
        + Statement/0[I]
          + Block/0[Statements]
            + Statement/0[S]
            + Statement/0[S]
        + Statement/0[S]
    + Statement/0[S]
)", s);
}

TEST(StateTest, NestedBlocks) {
  parser pg(R"(
program <- (~NL / expr)*

~BLOCK_COMMENT  <- '/*' ('/'+[^*/]+ / BLOCK_COMMENT / '*'+[^*/]+ / [^*/] )* ('*/'^unterminated_comment)
~LINE_COMMENT   <- '//' [^\n]*
~NOISE          <- [ \f\r\t] / BLOCK_COMMENT

NL              <- NOISE* LINE_COMMENT? '\n'

# error recovery
unterminated_comment <- '' { error_message "unterminated block comment" }

expr <- 'hello'
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::pair<size_t, size_t>> locations;

  pg["BLOCK_COMMENT"].enter = [&](const Context &c, const char *s, size_t /*n*/,
                                  std::any & /*dt*/) {
    locations.push_back(c.line_info(s));
  };

  pg["BLOCK_COMMENT"].leave = [&](const Context & /*c*/, const char * /*s*/,
                                  size_t /*n*/, size_t /*matchlen*/,
                                  std::any & /*value*/,
                                  std::any & /*dt*/) { locations.pop_back(); };

  std::vector<std::string> errors{
      R"(7:1: unterminated block comment)",
  };

  size_t i = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg, const std::string &rule) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    EXPECT_EQ(errors[i++], ss.str());

    EXPECT_EQ("unterminated_comment", rule);
    EXPECT_EQ(4, locations.size());
    EXPECT_EQ(1, locations[0].first);
    EXPECT_EQ(1, locations[0].second);
    EXPECT_EQ(2, locations[1].first);
    EXPECT_EQ(2, locations[1].second);
    EXPECT_EQ(3, locations[2].first);
    EXPECT_EQ(3, locations[2].second);
    EXPECT_EQ(4, locations[3].first);
    EXPECT_EQ(4, locations[3].second);
  });

  EXPECT_FALSE(pg.parse(R"(/* line 1:1 is the first comment open
 /* line 2:2 is the second
  /* line 3:3 and so on
   /* line 4:4
    /* line 5:5
*/
)"));

  EXPECT_EQ(i, errors.size());
}

