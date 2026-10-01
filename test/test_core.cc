#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

#if !defined(PEGLIB_NO_UNICODE_CHARS)
TEST(GeneralTest, Simple_syntax_test_with_unicode) {
  parser parser(u8" ROOT ← _ "
                " _ <- ' ' ");

  bool ret = parser;
  EXPECT_TRUE(ret);
  EXPECT_TRUE(parser.parse(" "));
}
#endif

TEST(GeneralTest, Simple_syntax_test) {
  parser parser(R"(
        ROOT <- _
        _ <- ' '
    )");

  bool ret = parser;
  EXPECT_TRUE(ret);
}

TEST(GeneralTest, Enable_ast_without_a_grammar) {
  parser parser("S <- A"); // A is undefined
  ASSERT_FALSE(parser);

  parser.enable_ast(); // does nothing
}

TEST(GeneralTest, Optimize_ast_without_a_grammar) {
  parser with("S <- A  A <- 'x'");
  with.enable_ast();
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(with.parse("x", ast));

  parser without("S <- A"); // A is undefined
  ASSERT_FALSE(without);
  EXPECT_EQ(ast_to_s(with.optimize_ast(ast)),
            ast_to_s(without.optimize_ast(ast)));
}

TEST(GeneralTest, Empty_syntax_test) {
  parser parser("");
  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, Start_rule_with_ignore_operator_test) {
  parser parser(R"(
        ~ROOT <- _
        _ <- ' '
    )");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, Invalid_UTF8_text_test) {
  std::string s = "a <- '";
  s += static_cast<char>(0xe8); // Make invalid utf8 text...

  parser parser(s.data());

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, Backslash_escape_sequence_test) {
  parser parser(R"(
        ROOT <- _
        _ <- '\\'
    )");

  bool ret = parser;
  EXPECT_TRUE(ret);
}

TEST(GeneralTest, Invalid_escape_sequence_test) {
  parser parser(R"(
        ROOT <- _
        _ <- '\'
    )");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, Action_taking_non_const_Semantic_Values_parameter) {
  parser parser(R"(
        ROOT <- TEXT
        TEXT <- [a-zA-Z]+
    )");

  parser["ROOT"] = [&](SemanticValues &vs) {
    auto s = std::string(std::any_cast<std::string_view>(vs[0]));
    s[0] = 'H'; // mutate
    return s;   // move
  };

  parser["TEXT"] = [&](SemanticValues &vs) { return vs.token(); };

  std::string val;
  auto ret = parser.parse("hello", val);
  EXPECT_TRUE(ret);
  EXPECT_EQ("Hello", val);
}

TEST(GeneralTest, String_capture_test) {
  parser parser(R"(
        ROOT      <-  _ ('[' TAG_NAME ']' _)*
        TAG_NAME  <-  (!']' .)+
        _         <-  [ \t]*
    )");

  std::vector<std::string_view> tags;

  parser["TAG_NAME"] = [&](const SemanticValues &vs) {
    tags.push_back(vs.sv());
  };

  auto ret = parser.parse(" [tag1] [tag:2] [tag-3] ");

  EXPECT_TRUE(ret);
  EXPECT_EQ(3, tags.size());
  EXPECT_EQ("tag1", tags[0]);
  EXPECT_EQ("tag:2", tags[1]);
  EXPECT_EQ("tag-3", tags[2]);
}

using namespace peg;

TEST(GeneralTest, String_capture_test2) {
  std::vector<std::string_view> tags;

  Definition ROOT, TAG, TAG_NAME, WS;
  ROOT <= seq(WS, zom(TAG));
  TAG <= seq(chr('['), TAG_NAME, chr(']'), WS);
  TAG_NAME <= oom(seq(npd(chr(']')), dot())),
      [&](const SemanticValues &vs) { tags.push_back(vs.sv()); };
  WS <= zom(cls(" \t"));

  auto r = ROOT.parse(" [tag1] [tag:2] [tag-3] ");

  EXPECT_TRUE(r.ret);
  EXPECT_EQ(3, tags.size());
  EXPECT_EQ("tag1", tags[0]);
  EXPECT_EQ("tag:2", tags[1]);
  EXPECT_EQ("tag-3", tags[2]);
}

TEST(GeneralTest, String_capture_test3) {
  parser pg(R"(
        ROOT  <- _ TOKEN*
        TOKEN <- '[' < (!']' .)+ > ']' _
        _     <- [ \t\r\n]*
    )");

  std::vector<std::string_view> tags;

  pg["TOKEN"] = [&](const SemanticValues &vs) { tags.push_back(vs.token()); };

  auto ret = pg.parse(" [tag1] [tag:2] [tag-3] ");

  EXPECT_TRUE(ret);
  EXPECT_EQ(3, tags.size());
  EXPECT_EQ("tag1", tags[0]);
  EXPECT_EQ("tag:2", tags[1]);
  EXPECT_EQ("tag-3", tags[2]);
}

TEST(GeneralTest, Cyclic_grammar_test) {
  Definition PARENT;
  Definition CHILD;

  PARENT <= seq(CHILD);
  CHILD <= seq(PARENT);
}

TEST(GeneralTest, Visit_test) {
  Definition ROOT, TAG, TAG_NAME, WS;

  ROOT <= seq(WS, zom(TAG));
  TAG <= seq(chr('['), TAG_NAME, chr(']'), WS);
  TAG_NAME <= oom(seq(npd(chr(']')), dot()));
  WS <= zom(cls(" \t"));

  AssignIDToDefinition defIds;
  ROOT.accept(defIds);

  EXPECT_EQ(4, defIds.ids.size());
}

TEST(GeneralTest, Token_check_test) {
  parser parser(R"(
        EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _
        TERM_OPERATOR    <-  < [-+] > _
        FACTOR_OPERATOR  <-  < [/*] > _
        NUMBER           <-  < [0-9]+ > _
        _                <-  [ \t\r\n]*
    )");

  EXPECT_FALSE(parser["EXPRESSION"].is_token());
  EXPECT_FALSE(parser["FACTOR"].is_token());
  EXPECT_TRUE(parser["FACTOR_OPERATOR"].is_token());
  EXPECT_TRUE(parser["NUMBER"].is_token());
  EXPECT_TRUE(parser["_"].is_token());
}

TEST(GeneralTest, Lambda_action_test) {
  parser parser(R"(
       START <- (CHAR)*
       CHAR  <- .
    )");

  std::string ss;
  parser["CHAR"] = [&](const SemanticValues &vs) { ss += *vs.sv().data(); };

  bool ret = parser.parse("hello");
  EXPECT_TRUE(ret);
  EXPECT_EQ("hello", ss);
}

TEST(GeneralTest, enter_leave_handlers_test) {
  parser parser(R"(
        START  <- LTOKEN '=' RTOKEN
        LTOKEN <- TOKEN
        RTOKEN <- TOKEN
        TOKEN  <- [A-Za-z]+
    )");

  parser["LTOKEN"].enter = [&](const Context & /*c*/, const char *, size_t,
                               std::any &dt) {
    auto &require_upper_case = *std::any_cast<bool *>(dt);
    require_upper_case = false;
  };
  parser["LTOKEN"].leave = [&](const Context & /*c*/, const char *, size_t,
                               size_t, std::any &, std::any &dt) {
    auto &require_upper_case = *std::any_cast<bool *>(dt);
    require_upper_case = true;
  };

  auto message = "should be upper case string...";

  parser["TOKEN"].predicate = [&](const SemanticValues &vs, const std::any &dt,
                                  std::string &msg) {
    auto &require_upper_case = *std::any_cast<bool *>(dt);
    if (require_upper_case) {
      const auto &s = vs.sv();
      if (!std::all_of(s.begin(), s.end(), ::isupper)) {
        msg = message;
        return false;
      }
    }
    return true;
  };

  bool require_upper_case = false;
  std::any dt = &require_upper_case;
  EXPECT_FALSE(parser.parse("hello=world", dt));
  EXPECT_FALSE(parser.parse("HELLO=world", dt));
  EXPECT_TRUE(parser.parse("hello=WORLD", dt));
  EXPECT_TRUE(parser.parse("HELLO=WORLD", dt));

  parser.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    EXPECT_EQ(1, ln);
    EXPECT_EQ(7, col);
    EXPECT_EQ(message, msg);
  });
  parser.parse("hello=world", dt);
}

TEST(GeneralTest, WHITESPACE_test) {
  parser parser(R"(
        # Rules
        ROOT         <-  ITEM (',' ITEM)*
        ITEM         <-  WORD / PHRASE

        # Tokens
        WORD         <-  < [a-zA-Z0-9_]+ >
        PHRASE       <-  < '"' (!'"' .)* '"' >

        %whitespace  <-  [ \t\r\n]*
    )");

  auto ret = parser.parse(R"(  one, 	 "two, three",   four  )");

  EXPECT_TRUE(ret);
}

TEST(GeneralTest, WHITESPACE_test2) {
  parser parser(R"(
        # Rules
        ROOT         <-  ITEM (',' ITEM)*
        ITEM         <-  '[' < [a-zA-Z0-9_]+ > ']'

        %whitespace  <-  (SPACE / TAB)*
        SPACE        <-  ' '
        TAB          <-  '\t'
    )");

  std::vector<std::string_view> items;
  parser["ITEM"] = [&](const SemanticValues &vs) {
    items.push_back(vs.token());
  };

  auto ret = parser.parse(R"([one], 	[two] ,[three] )");

  EXPECT_TRUE(ret);
  EXPECT_EQ(3, items.size());
  EXPECT_EQ("one", items[0]);
  EXPECT_EQ("two", items[1]);
  EXPECT_EQ("three", items[2]);
}

TEST(GeneralTest, WHITESPACE_test3) {
  parser parser(R"(
        StrQuot      <- < '"' < (StrEscape / StrChars)* > '"' >
        StrEscape    <- '\\' any
        StrChars     <- (!'"' !'\\' any)+
        any          <- .
        %whitespace  <- [ \t]*
    )");

  parser["StrQuot"] = [](const SemanticValues &vs) {
    EXPECT_EQ(R"(  aaa \" bbb  )", vs.token());
  };

  auto ret = parser.parse(R"( "  aaa \" bbb  " )");
  EXPECT_TRUE(ret);
}

TEST(NoWhitespaceTest, Preserve_whitespace_in_rule) {
  parser parser(R"(
        S           <- 'x' Str 'y'
        Str         <- '"' < (!'"' .)* > '"'  { no_whitespace }
        %whitespace <- [ \t]*
    )");
  ASSERT_TRUE(!!parser);

  std::string tok;
  parser["Str"] = [&](const SemanticValues &vs) { tok = vs.token_to_string(); };

  EXPECT_TRUE(parser.parse(R"(x "  a b  " y)"));
  EXPECT_EQ("  a b  ", tok);
}

TEST(NoWhitespaceTest, Predicate_after_literal) {
  auto make = [](const char *kw_rule) {
    return std::string(R"(
        START   <- (KEYWORD / ID)*
        )") +
           kw_rule + R"(
        IDCHAR  <- [A-Za-z0-9_]
        ID      <- !KEYWORD < [A-Za-z_] IDCHAR* >
        %whitespace <- [ \t\r\n]*
    )";
  };

  // Without no_whitespace, "create" eats its trailing whitespace before
  // !IDCHAR tests the input, so the keyword is never recognized.
  {
    parser parser(make(R"(KEYWORD <- "create" !IDCHAR)").c_str());
    ASSERT_TRUE(!!parser);
    auto count = 0;
    parser["KEYWORD"] = [&](const SemanticValues &) { count++; };
    EXPECT_TRUE(parser.parse("I will create a deleter"));
    EXPECT_EQ(0, count);
  }
  {
    parser parser(
        make(R"(KEYWORD <- "create" !IDCHAR  { no_whitespace })").c_str());
    ASSERT_TRUE(!!parser);
    auto count = 0;
    parser["KEYWORD"] = [&](const SemanticValues &) { count++; };
    EXPECT_TRUE(parser.parse("I will create a deleter"));
    EXPECT_EQ(1, count);
  }
}

TEST(NoWhitespaceTest, Trailing_whitespace_is_skipped_after_rule) {
  parser parser(R"(
        S           <- KW KW
        KW          <- 'a' 'b'  { no_whitespace }
        %whitespace <- [ \t]*
    )");
  ASSERT_TRUE(!!parser);
  EXPECT_TRUE(parser.parse("ab ab"));   // whitespace after the rule is skipped
  EXPECT_FALSE(parser.parse("a b ab")); // but not inside the rule
}

TEST(NoWhitespaceTest, Combined_with_other_instructions) {
  parser parser(R"(
        S           <- KW
        KW          <- 'a' 'b'  { no_whitespace; no_ast_opt }
        %whitespace <- [ \t]*
    )");
  ASSERT_TRUE(!!parser);
  EXPECT_TRUE(parser.parse("ab"));
  EXPECT_FALSE(parser.parse("a b"));
}

TEST(NoWhitespaceTest, With_packrat) {
  parser parser(R"(
        S           <- 'x' Str 'y' / 'x' Str 'z'
        Str         <- '"' < (!'"' .)* > '"'  { no_whitespace }
        %whitespace <- [ \t]*
    )");
  ASSERT_TRUE(!!parser);
  parser.enable_packrat_parsing();
  EXPECT_TRUE(parser.parse(R"(x " a " z)"));
  EXPECT_FALSE(parser.parse(R"(x " a " w)"));
}

TEST(GeneralTest, WHITESPACE_test4) {
  parser parser(R"(
        ROOT         <-  HELLO OPE WORLD
        HELLO        <-  'hello'
        OPE          <-  < [-+] >
        WORLD        <-  'world' / 'WORLD'
        %whitespace  <-  [ \t\r\n]*
    )");

  parser["HELLO"] = [](const SemanticValues &vs) {
    EXPECT_EQ("hello", vs.token());
  };

  parser["OPE"] = [](const SemanticValues &vs) { EXPECT_EQ("+", vs.token()); };

  parser["WORLD"] = [](const SemanticValues &vs) {
    EXPECT_EQ("world", vs.token());
  };

  auto ret = parser.parse("  hello + world  ");
  EXPECT_TRUE(ret);
}

TEST(GeneralTest, Word_expression_test) {
  parser parser(R"(
        ROOT         <-  'hello' ','? 'world'
        %whitespace  <-  [ \t\r\n]*
        %word        <-  [a-z]+
    )");

  EXPECT_FALSE(parser.parse("helloworld"));
  EXPECT_TRUE(parser.parse("hello world"));
  EXPECT_TRUE(parser.parse("hello,world"));
  EXPECT_TRUE(parser.parse("hello, world"));
  EXPECT_TRUE(parser.parse("hello , world"));
}

TEST(GeneralTest, Word_expression_test_PrioritizedChoice) {
  parser parser(R"(
    Identifier  ← < !Keyword [a-z][a-z]* >
    Keyword     ← 'def' / 'to'
    %whitespace ← [ \t\r\n]*
    %word       ← [a-z]+
  )");

  EXPECT_TRUE(parser.parse("toa"));
}

TEST(GeneralTest, Word_expression_test_Dictionary) {
  parser parser(R"(
    Identifier  ← < !Keyword [a-z][a-z]* >
    Keyword     ← 'def' | 'to'
    %whitespace ← [ \t\r\n]*
    %word       ← [a-z]+
  )");

  EXPECT_TRUE(parser.parse("toa"));
}

TEST(GeneralTest, Word_expression_case_ignore_test_Dictionary) {
  parser parser(R"(
    Identifier  ← < !Keyword [a-z][a-z]* >
    Keyword     ← 'def'i | 'to'i
    %whitespace ← [ \t\r\n]*
    %word       ← [a-z]+
  )");

  EXPECT_TRUE(parser.parse("toa"));
}

TEST(GeneralTest, Word_expression_syntax_error_test_Dictionary) {
  parser parser(R"(
    Identifier  ← < !Keyword [a-z][a-z]* >
    Keyword     ← 'def' | 'to'i
    %whitespace ← [ \t\r\n]*
    %word       ← [a-z]+
  )");

  EXPECT_FALSE(parser);
}

TEST(GeneralTest, Skip_token_test) {
  parser parser("  ROOT  <-  _ ITEM (',' _ ITEM _)* "
                "  ITEM  <-  ([a-z0-9])+  "
                "  ~_    <-  [ \t]*    ");

  parser["ROOT"] = [&](const SemanticValues &vs) { EXPECT_EQ(2, vs.size()); };

  auto ret = parser.parse(" item1, item2 ");

  EXPECT_TRUE(ret);
}

TEST(GeneralTest, Skip_token_test2) {
  parser parser(R"(
        ROOT        <-  ITEM (',' ITEM)*
        ITEM        <-  < ([a-z0-9])+ >
        %whitespace <-  [ \t]*
    )");

  parser["ROOT"] = [&](const SemanticValues &vs) { EXPECT_EQ(2, vs.size()); };

  auto ret = parser.parse(" item1, item2 ");

  EXPECT_TRUE(ret);
}

TEST(GeneralTest, Custom_AST_test) {
  struct CustomType {
    bool dummy = false;
  };
  using CustomAst = AstBase<CustomType>;

  parser parser(R"(
        ROOT <- _ TEXT*
        TEXT <- [a-zA-Z]+ _
        _ <- [ \t\r\n]*
    )");

  parser.enable_ast<CustomAst>();
  std::shared_ptr<CustomAst> ast;
  bool ret = parser.parse("a b c", ast);
  EXPECT_TRUE(ret);
  EXPECT_EQ(4, ast->nodes.size());
}

TEST(GeneralTest, Backtracking_test) {
  parser parser(R"(
       START <- PAT1 / PAT2
       PAT1  <- HELLO ' One'
       PAT2  <- HELLO ' Two'
       HELLO <- 'Hello'
    )");

  size_t count = 0;
  parser["HELLO"] = [&](const SemanticValues & /*vs*/) { count++; };

  parser.enable_packrat_parsing();

  bool ret = parser.parse("Hello Two");
  EXPECT_TRUE(ret);
  EXPECT_EQ(1, count); // Skip second time
}

TEST(GeneralTest, Backtracking_with_AST) {
  parser parser(R"(
        S <- A? B (A B)* A
        A <- 'a'
        B <- 'b'
    )");

  parser.enable_ast();
  std::shared_ptr<Ast> ast;
  bool ret = parser.parse("ba", ast);
  EXPECT_TRUE(ret);
  EXPECT_EQ(2, ast->nodes.size());
}

TEST(GeneralTest, Octal_Hex_Unicode_value_test) {
  parser parser(R"( ROOT <- '\132\x7a\u30f3' )");

  auto ret = parser.parse("Zzン");

  EXPECT_TRUE(ret);
}

TEST(GeneralTest, Ignore_case_literal_test) {
  parser parser(R"(
    ROOT         <-  HELLO WORLD
    HELLO        <-  'hello'i
    WORLD        <-  'world'i
    %whitespace  <-  [ \t\r\n]*
  )");

  parser["HELLO"] = [](const SemanticValues &vs) {
    EXPECT_EQ("Hello", vs.token());
  };

  parser["WORLD"] = [](const SemanticValues &vs) {
    EXPECT_EQ("World", vs.token());
  };

  auto ret = parser.parse("  Hello World  ");
  EXPECT_TRUE(ret);
}

TEST(GeneralTest, Ignore_case_character_class_test) {
  parser parser(R"(ROOT <-  [a-z]i+)");

  EXPECT_TRUE(parser.parse("abc"));
  EXPECT_TRUE(parser.parse("ABC"));
  EXPECT_TRUE(parser.parse("Abc"));
  EXPECT_TRUE(parser.parse("Abc"));
  EXPECT_FALSE(parser.parse("123"));
}

TEST(GeneralTest, Ignore_case_negate_character_class_test) {
  parser parser(R"(ROOT <-  [^a-z]i+)");

  EXPECT_TRUE(parser.parse("123"));
  EXPECT_FALSE(parser.parse("ABC"));
}

TEST(GeneralTest, mutable_lambda_test) {
  std::vector<std::string_view> vec;

  parser pg("ROOT <- 'mutable lambda test'");

  // This test makes sure if the following code can be compiled.
  pg["TOKEN"] = [=](const SemanticValues &vs) mutable {
    vec.push_back(vs.sv());
  };
}

TEST(GeneralTest, Simple_calculator_test) {
  parser parser(R"(
        Additive  <- Multiplicative '+' Additive / Multiplicative
        Multiplicative <- Primary '*' Multiplicative / Primary
        Primary   <- '(' Additive ')' / Number
        Number    <- [0-9]+
    )");

  parser["Additive"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: return std::any_cast<int>(vs[0]) + std::any_cast<int>(vs[1]);
    default: return std::any_cast<int>(vs[0]);
    }
  };

  parser["Multiplicative"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: return std::any_cast<int>(vs[0]) * std::any_cast<int>(vs[1]);
    default: return std::any_cast<int>(vs[0]);
    }
  };

  parser["Number"] = [](const SemanticValues &vs) {
    return vs.token_to_number<int>();
  };

  int val;
  parser.parse("(1+2)*3", val);

  EXPECT_EQ(9, val);
}

TEST(GeneralTest, Simple_calculator_with_recovery_test) {
  parser parser(R"(
        Additive    <- Multiplicative '+' Additive / Multiplicative
        Multiplicative   <- Primary '*' Multiplicative^cond / Primary
        Primary     <- '(' Additive ')' / Number
        Number      <- < [0-9]+ >
        %whitespace <- [ \t]*
        cond <- '' { error_message "missing multiplicative" }
    )");

  parser["Additive"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: return std::any_cast<int>(vs[0]) + std::any_cast<int>(vs[1]);
    default: return std::any_cast<int>(vs[0]);
    }
  };

  parser["Multiplicative"] = [](const SemanticValues &vs) {
    switch (vs.choice()) {
    case 0: return std::any_cast<int>(vs[0]) * std::any_cast<int>(vs[1]);
    default: return std::any_cast<int>(vs[0]);
    }
  };

  parser["Number"] = [](const SemanticValues &vs) {
    return vs.token_to_number<int>();
  };

  int val = 0;
  auto ret = parser.parse(" (1 + 2) * ", val);

  EXPECT_FALSE(ret);
  EXPECT_EQ(0, val);
}

TEST(GeneralTest, Calculator_test) {
  // Construct grammar
  Definition EXPRESSION, TERM, FACTOR, TERM_OPERATOR, FACTOR_OPERATOR, NUMBER;

  EXPRESSION <= seq(TERM, zom(seq(TERM_OPERATOR, TERM)));
  TERM <= seq(FACTOR, zom(seq(FACTOR_OPERATOR, FACTOR)));
  FACTOR <= cho(NUMBER, seq(chr('('), EXPRESSION, chr(')')));
  TERM_OPERATOR <= cls("+-");
  FACTOR_OPERATOR <= cls("*/");
  NUMBER <= oom(cls("0-9"));

  // Setup actions
  auto reduce = [](const SemanticValues &vs) -> long {
    long ret = std::any_cast<long>(vs[0]);
    for (auto i = 1u; i < vs.size(); i += 2) {
      auto num = std::any_cast<long>(vs[i + 1]);
      switch (std::any_cast<char>(vs[i])) {
      case '+': ret += num; break;
      case '-': ret -= num; break;
      case '*': ret *= num; break;
      case '/': ret /= num; break;
      }
    }
    return ret;
  };

  EXPRESSION = reduce;
  TERM = reduce;
  TERM_OPERATOR = [](const SemanticValues &vs) { return *vs.sv().data(); };
  FACTOR_OPERATOR = [](const SemanticValues &vs) { return *vs.sv().data(); };
  NUMBER = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };

  // Parse
  long val;
  auto r = EXPRESSION.parse_and_get_value("1+2*3*(4-5+6)/7-8", val);

  EXPECT_TRUE(r.ret);
  EXPECT_EQ(-3, val);
}

TEST(GeneralTest, Calculator_test2) {
  // Parse syntax
  auto syntax = R"(
        # Grammar for Calculator...
        EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' EXPRESSION ')'
        TERM_OPERATOR    <-  [-+]
        FACTOR_OPERATOR  <-  [/*]
        NUMBER           <-  [0-9]+
    )";

  auto cxt = ParserGenerator::parse(syntax, strlen(syntax), {}, nullptr, {});
  auto &g = *cxt.grammar;

  // Setup actions
  auto reduce = [](const SemanticValues &vs) -> long {
    long ret = std::any_cast<long>(vs[0]);
    for (auto i = 1u; i < vs.size(); i += 2) {
      auto num = std::any_cast<long>(vs[i + 1]);
      switch (std::any_cast<char>(vs[i])) {
      case '+': ret += num; break;
      case '-': ret -= num; break;
      case '*': ret *= num; break;
      case '/': ret /= num; break;
      }
    }
    return ret;
  };

  g["EXPRESSION"] = reduce;
  g["TERM"] = reduce;
  g["TERM_OPERATOR"] = [](const SemanticValues &vs) { return *vs.sv().data(); };
  g["FACTOR_OPERATOR"] = [](const SemanticValues &vs) {
    return *vs.sv().data();
  };
  g["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  // Parse
  long val;
  auto r = g[cxt.start].parse_and_get_value("1+2*3*(4-5+6)/7-8", val);

  EXPECT_TRUE(r.ret);
  EXPECT_EQ(-3, val);
}

TEST(GeneralTest, Calculator_test3) {
  // Parse syntax
  parser parser(R"(
        # Grammar for Calculator...
        EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' EXPRESSION ')'
        TERM_OPERATOR    <-  [-+]
        FACTOR_OPERATOR  <-  [/*]
        NUMBER           <-  [0-9]+
    )");

  auto reduce = [](const SemanticValues &vs) -> long {
    long ret = std::any_cast<long>(vs[0]);
    for (auto i = 1u; i < vs.size(); i += 2) {
      auto num = std::any_cast<long>(vs[i + 1]);
      switch (std::any_cast<char>(vs[i])) {
      case '+': ret += num; break;
      case '-': ret -= num; break;
      case '*': ret *= num; break;
      case '/': ret /= num; break;
      }
    }
    return ret;
  };

  // Setup actions
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

  // Parse
  long val;
  auto ret = parser.parse("1+2*3*(4-5+6)/7-8", val);

  EXPECT_TRUE(ret);
  EXPECT_EQ(-3, val);
}

TEST(GeneralTest, Calculator_test_with_AST) {
  parser parser(R"(
        EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _
        TERM_OPERATOR    <-  < [-+] > _
        FACTOR_OPERATOR  <-  < [/*] > _
        NUMBER           <-  < [0-9]+ > _
        ~_               <-  [ \t\r\n]*
    )");

  parser.enable_ast();

  std::function<long(const Ast &)> eval = [&](const Ast &ast) {
    if (ast.name == "NUMBER") {
      return ast.token_to_number<long>();
    } else {
      const auto &nodes = ast.nodes;
      auto result = eval(*nodes[0]);
      for (auto i = 1u; i < nodes.size(); i += 2) {
        auto num = eval(*nodes[i + 1]);
        auto ope = nodes[i]->token[0];
        switch (ope) {
        case '+': result += num; break;
        case '-': result -= num; break;
        case '*': result *= num; break;
        case '/': result /= num; break;
        }
      }
      return result;
    }
  };

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse("1+2*3*(4-5+6)/7-8", ast);
  ast = parser.optimize_ast(ast);
  auto val = eval(*ast);

  EXPECT_TRUE(ret);
  EXPECT_EQ(-3, val);
}

TEST(GeneralTest, Calculator_test_with_combinators_and_AST) {
  // Construct grammar
  AST_DEFINITIONS(EXPRESSION, TERM, FACTOR, TERM_OPERATOR, FACTOR_OPERATOR,
                  NUMBER);

  EXPRESSION <= seq(TERM, zom(seq(TERM_OPERATOR, TERM)));
  TERM <= seq(FACTOR, zom(seq(FACTOR_OPERATOR, FACTOR)));
  FACTOR <= cho(NUMBER, seq(chr('('), EXPRESSION, chr(')')));
  TERM_OPERATOR <= cls("+-");
  FACTOR_OPERATOR <= cls("*/");
  NUMBER <= oom(cls("0-9"));

  std::function<long(const Ast &)> eval = [&](const Ast &ast) {
    if (ast.name == "NUMBER") {
      return ast.token_to_number<long>();
    } else {
      const auto &nodes = ast.nodes;
      auto result = eval(*nodes[0]);
      for (auto i = 1u; i < nodes.size(); i += 2) {
        auto num = eval(*nodes[i + 1]);
        auto ope = nodes[i]->token[0];
        switch (ope) {
        case '+': result += num; break;
        case '-': result -= num; break;
        case '*': result *= num; break;
        case '/': result /= num; break;
        }
      }
      return result;
    }
  };

  std::shared_ptr<Ast> ast;
  auto r = EXPRESSION.parse_and_get_value("1+2*3*(4-5+6)/7-8", ast);
  ast = AstOptimizer(true).optimize(ast);
  auto val = eval(*ast);

  EXPECT_TRUE(r.ret);
  EXPECT_EQ(-3, val);
}

TEST(GeneralTest, Ignore_semantic_value_test) {
  parser parser(R"(
       START <-  ~HELLO WORLD
       HELLO <- 'Hello' _
       WORLD <- 'World' _
       _     <- [ \t\r\n]*
    )");

  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse("Hello World", ast);

  EXPECT_TRUE(ret);
  EXPECT_EQ(1, ast->nodes.size());
  EXPECT_EQ("WORLD", ast->nodes[0]->name);
}

TEST(GeneralTest, Ignore_semantic_value_of_or_predicate_test) {
  parser parser(R"(
       START       <- _ !DUMMY HELLO_WORLD '.'
       HELLO_WORLD <- HELLO 'World' _
       HELLO       <- 'Hello' _
       DUMMY       <- 'dummy' _
       ~_          <- [ \t\r\n]*
   )");

  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse("Hello World.", ast);

  EXPECT_TRUE(ret);
  EXPECT_EQ(1, ast->nodes.size());
  EXPECT_EQ("HELLO_WORLD", ast->nodes[0]->name);
}

TEST(GeneralTest, Ignore_semantic_value_of_and_predicate_test) {
  parser parser(R"(
       START       <- _ &HELLO HELLO_WORLD '.'
       HELLO_WORLD <- HELLO 'World' _
       HELLO       <- 'Hello' _
       ~_          <- [ \t\r\n]*
    )");

  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse("Hello World.", ast);

  EXPECT_TRUE(ret);
  EXPECT_EQ(1, ast->nodes.size());
  EXPECT_EQ("HELLO_WORLD", ast->nodes[0]->name);
}

TEST(GeneralTest, Literal_token_on_AST_test1) {
  parser parser(R"(
        STRING_LITERAL  <- '"' (('\\"' / '\\t' / '\\n') / (!["] .))* '"'
    )");
  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse(R"("a\tb")", ast);

  EXPECT_TRUE(ret);
  EXPECT_TRUE(ast->is_token);
  EXPECT_EQ(R"("a\tb")", ast->token);
  EXPECT_TRUE(ast->nodes.empty());
}

TEST(GeneralTest, Literal_token_on_AST_test2) {
  parser parser(R"(
        STRING_LITERAL  <-  '"' (ESC / CHAR)* '"'
        ESC             <-  ('\\"' / '\\t' / '\\n')
        CHAR            <-  (!["] .)
    )");
  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse(R"("a\tb")", ast);

  EXPECT_TRUE(ret);
  EXPECT_FALSE(ast->is_token);
  EXPECT_TRUE(ast->token.empty());
  EXPECT_EQ(3, ast->nodes.size());
}

TEST(GeneralTest, Literal_token_on_AST_test3) {
  parser parser(R"(
        STRING_LITERAL  <-  < '"' (ESC / CHAR)* '"' >
        ESC             <-  ('\\"' / '\\t' / '\\n')
        CHAR            <-  (!["] .)
    )");
  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse(R"("a\tb")", ast);

  EXPECT_TRUE(ret);
  EXPECT_TRUE(ast->is_token);
  EXPECT_EQ(R"("a\tb")", ast->token);
  EXPECT_TRUE(ast->nodes.empty());
}

TEST(GeneralTest, Literal_token_on_AST_test4) {
  parser parser(R"(
        STRING_LITERAL  <-  < '"' < (ESC / CHAR)* > '"' >
        ESC             <-  ('\\"' / '\\t' / '\\n')
        CHAR            <-  (!["] .)
    )");
  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse(R"("a\tb")", ast);

  EXPECT_TRUE(ret);
  EXPECT_TRUE(ast->is_token);
  EXPECT_EQ(R"(a\tb)", ast->token);
  EXPECT_TRUE(ast->nodes.empty());
}

TEST(GeneralTest, Missing_missing_definitions_test) {
  parser parser(R"(
        A <- B C
    )");

  EXPECT_FALSE(parser);
}

TEST(GeneralTest, Definition_duplicates_test) {
  parser parser(R"(
        A <- ''
        A <- ''
    )");

  EXPECT_FALSE(parser);
}

TEST(GeneralTest, Semantic_values_test) {
  parser parser(R"(
        term <- ( a b c x )? a b c
        a <- 'a'
        b <- 'b'
        c <- 'c'
        x <- 'x'
    )");

  for (const auto &item : parser.get_grammar()) {
    const auto &rule = item.first;
    parser[rule.data()] = [rule](const SemanticValues &vs, std::any &) {
      if (rule == "term") {
        EXPECT_EQ("a at 0", std::any_cast<std::string>(vs[0]));
        EXPECT_EQ("b at 1", std::any_cast<std::string>(vs[1]));
        EXPECT_EQ("c at 2", std::any_cast<std::string>(vs[2]));
        return std::string();
      } else {
        return rule + " at " + std::to_string(vs.sv().data() - vs.ss);
      }
    };
  }

  EXPECT_TRUE(parser.parse("abc"));
}

TEST(GeneralTest, Ordered_choice_count) {
  parser parser(R"(
        S <- 'a' / 'b'
    )");

  parser["S"] = [](const SemanticValues &vs) {
    EXPECT_EQ(1, vs.choice());
    EXPECT_EQ(2, vs.choice_count());
  };

  parser.parse("b");
}

TEST(GeneralTest, Ordered_choice_count_2) {
  parser parser(R"(
        S <- ('a' / 'b')*
    )");

  parser["S"] = [](const SemanticValues &vs) {
    EXPECT_EQ(0, vs.choice());
    EXPECT_EQ(0, vs.choice_count());
  };

  parser.parse("b");
}

TEST(GeneralTest, Semantic_value_tag) {
  parser parser(R"(
        S <- A? B* C?
        A <- 'a'
        B <- 'b'
        C <- 'c'
    )");

  {
    using namespace udl;
    parser["S"] = [](const SemanticValues &vs) {
      EXPECT_EQ(1, vs.size());
      EXPECT_EQ(1, vs.tags.size());
      EXPECT_EQ("C"_, vs.tags[0]);
    };
    auto ret = parser.parse("c");
    EXPECT_TRUE(ret);
  }

  {
    using namespace udl;
    parser["S"] = [](const SemanticValues &vs) {
      EXPECT_EQ(2, vs.size());
      EXPECT_EQ(2, vs.tags.size());
      EXPECT_EQ("B"_, vs.tags[0]);
      EXPECT_EQ("B"_, vs.tags[1]);
    };
    auto ret = parser.parse("bb");
    EXPECT_TRUE(ret);
  }

  {
    using namespace udl;
    parser["S"] = [](const SemanticValues &vs) {
      EXPECT_EQ(2, vs.size());
      EXPECT_EQ(2, vs.tags.size());
      EXPECT_EQ("A"_, vs.tags[0]);
      EXPECT_EQ("C"_, vs.tags[1]);
    };
    auto ret = parser.parse("ac");
    EXPECT_TRUE(ret);
  }
}

TEST(GeneralTest, Negated_Class_test) {
  parser parser(R"(
        ROOT <- [^a-z_]+
    )");

  bool ret = parser;
  EXPECT_TRUE(ret);

  EXPECT_TRUE(parser.parse("ABC123"));
  EXPECT_FALSE(parser.parse("ABcZ"));
  EXPECT_FALSE(parser.parse("ABCZ_"));
  EXPECT_FALSE(parser.parse(""));
}

TEST(GeneralTest, token_to_number_float_test) {
  parser parser(R"(
    S <- '1.1'
  )");
  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse("1.1", ast);

  EXPECT_TRUE(ret);
  EXPECT_TRUE(ast->is_token);
  EXPECT_EQ("1.1", ast->token);
  EXPECT_EQ(1.1f, ast->token_to_number<float>());
  EXPECT_TRUE(ast->nodes.empty());
}

TEST(GeneralTest, ParentReferencesShouldNotBeExpired) {
  auto parser = peg::parser(R"(
		ROOT            <- OPTIMIZES_AWAY
		OPTIMIZES_AWAY  <- ITEM+
		ITEM            <- 'a'
	)");
  parser.enable_ast<peg::Ast>();

  std::shared_ptr<peg::Ast> ast;
  parser.parse("aaa", ast);
  ast = parser.optimize_ast(ast);

  EXPECT_FALSE(ast->nodes[0]->parent.expired());
}

TEST(GeneralTest, EndOfInputTest) {
  auto parser = peg::parser(R"(
    S <- '[[' (!']]' .)* ']]' !.
	)");

  parser.disable_eoi_check();

  auto ret = parser.parse("[[]]]");
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, DefaultEndOfInputTest) {
  auto parser = peg::parser(R"(
    S <- '[[' (!']]' .)* ']]'
	)");

  auto ret = parser.parse("[[]]]");
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, DisableEndOfInputCheckTest) {
  auto parser = peg::parser(R"(
    S <- '[[' (!']]' .)* ']]'
	)");

  parser.disable_eoi_check();

  auto ret = parser.parse("[[]]]");
  EXPECT_TRUE(ret);
}

TEST(GeneralTest, InvalidCutOperator) {
  auto parser = peg::parser(R"(
    S <- 'a' ↑ 'b'
	)");

  auto ret = parser.parse("ab");
  EXPECT_TRUE(ret);

  ret = parser.parse("ac");
  EXPECT_FALSE(ret);

  ret = parser.parse("b");
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, HeuristicErrorTokenTest) {
  auto parser = peg::parser(R"(
    program      <- enum+
    enum         <- 'enum' enum_kind^untyped_enum
    enum_kind    <- 'sequence' / 'bitmask'

    %whitespace  <- [ \r\t\n]*
    %word        <- [a-zA-Z0-9_]

    untyped_enum <- '' { message "invalid/missing enum type, expected one of 'sequence' or 'bitmask', got '%t'"}
	)");

  parser.set_logger([&](size_t ln, size_t col, const std::string &msg) {
    EXPECT_EQ(1, ln);
    EXPECT_EQ(6, col);
    EXPECT_EQ("invalid/missing enum type, expected one of 'sequence' or "
              "'bitmask', got 'sequencer'",
              msg);
  });

  auto ret = parser.parse("enum sequencer");
  EXPECT_FALSE(ret);
}

TEST(GeneralTest, LiteralContentInAST) {
  parser parser(R"(
PROGRAM                <-  STATEMENTS

STATEMENTS             <-  (STATEMENT ';'?)*
STATEMENT              <-  ASSIGNMENT / RETURN / EXPRESSION_STATEMENT

ASSIGNMENT             <-  'let' IDENTIFIER '=' EXPRESSION
RETURN                 <-  'return' EXPRESSION
EXPRESSION_STATEMENT   <-  EXPRESSION

EXPRESSION             <-  INFIX_EXPR(PREFIX_EXPR, INFIX_OPE)
INFIX_EXPR(ATOM, OPE)  <-  ATOM (OPE ATOM)* {
                             precedence
                               L == !=
                               L < >
                               L + -
                               L * /
                           }

IF                     <-  'if' '(' EXPRESSION ')' BLOCK ('else' BLOCK)?

FUNCTION               <-  'fn' '(' PARAMETERS ')' BLOCK
PARAMETERS             <-  LIST(IDENTIFIER, ',')

BLOCK                  <-  '{' STATEMENTS '}'

CALL                   <-  PRIMARY (ARGUMENTS / INDEX)*
ARGUMENTS              <-  '(' LIST(EXPRESSION, ',') ')'
INDEX                  <-   '[' EXPRESSION ']'

PREFIX_EXPR            <-  PREFIX_OPE* CALL
PRIMARY                <-  IF / FUNCTION / ARRAY / HASH / INTEGER / BOOLEAN / NULL / IDENTIFIER / STRING / '(' EXPRESSION ')'

ARRAY                  <-  '[' LIST(EXPRESSION, ',') ']'

HASH                   <-  '{' LIST(HASH_PAIR, ',') '}'
HASH_PAIR              <-  EXPRESSION ':' EXPRESSION

IDENTIFIER             <-  < !KEYWORD [a-zA-Z]+ >
INTEGER                <-  < [0-9]+ >
STRING                 <-  < ["] < (!["] .)* > ["] >
BOOLEAN                <-  'true' / 'false'
NULL                   <-  'null'
PREFIX_OPE             <-  < [-!] >
INFIX_OPE              <-  < [-+/*<>] / '==' / '!=' >

KEYWORD                <-  ('null' | 'true' | 'false' | 'let' | 'return' | 'if' | 'else' | 'fn') ![a-zA-Z]

LIST(ITEM, DELM)       <-  (ITEM (~DELM ITEM)*)?

LINE_COMMENT           <-  '//' (!LINE_END .)* &LINE_END
LINE_END               <-  '\r\n' / '\r' / '\n' / !.

%whitespace            <-  ([ \t\r\n]+ / LINE_COMMENT)*
%word                  <-  [a-zA-Z]+
  )");
  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  auto ret = parser.parse(R"({1: 1, 2: 2, 3: 3})", ast);

  EXPECT_TRUE(ret);

  auto opt =
      AstOptimizer(true, {"EXPRESSION_STATEMENT", "PARAMETERS", "ARGUMENTS",
                          "INDEX", "RETURN", "BLOCK", "ARRAY", "HASH"});
  ast = opt.optimize(ast);

  EXPECT_EQ("EXPRESSION_STATEMENT", ast->name);

  auto node = ast->nodes[0];
  EXPECT_EQ("HASH", node->name);

  std::map<std::string, int64_t> expected = {
      {"1", 1},
      {"2", 2},
      {"3", 3},
  };

  for (auto node : node->nodes) {
    auto key = node->nodes[0];
    auto val = node->nodes[1];
    EXPECT_EQ("INTEGER", key->name);

    auto expectedValue = expected[key->token_to_string()];
    EXPECT_EQ("INTEGER", val->name);
    EXPECT_EQ(expectedValue, val->token_to_number<int64_t>());
  }
}

TEST(GeneralTest, NoAstOptPreservesPosition) {
  // When a rule has `no_ast_opt`, its source position (pos/len) must survive
  // when outer rules are collapsed away by AstOptimizer. Otherwise the
  // identity of the preserved rule is incomplete -- the name is kept but the
  // source range gets overwritten by the outermost collapsed wrapper, which
  // breaks error reporting and source-level rewriting on the preserved node.
  parser parser(R"(
    BLOCK       <-  '{' STATEMENTS '}'
    STATEMENTS  <-  STATEMENT (';' STATEMENT)*
    STATEMENT   <-  YIELD / EXPR
    YIELD       <-  'yield' EXPR        { no_ast_opt }
    EXPR        <-  < [a-z0-9]+ >
    %whitespace <-  [ \t\r\n]*
    %word       <-  [a-zA-Z]+
  )");
  parser.enable_ast();

  std::shared_ptr<Ast> ast;
  std::string src = "{ yield i }";
  auto ret = parser.parse(src, ast);
  EXPECT_TRUE(ret);

  ast = parser.optimize_ast(ast);

  // After collapse, the surviving node represents YIELD and must report
  // YIELD's source range (starting at 'yield'), not BLOCK's range
  // (which starts at '{').
  EXPECT_EQ("YIELD", ast->name);
  EXPECT_TRUE(ast->preserve_position);
  EXPECT_EQ(src.find("yield"), ast->position);
  EXPECT_LT(ast->length, src.length());
}

TEST(GeneralTest, CollapsedAstMatchesOptimizedAst) {
  // enable_ast(true, opt_mode) must build exactly the tree that
  // optimize_ast(ast, opt_mode) makes out of the full one, source ranges and
  // parent links included, and enable_ast(true, opt_mode, rules) the one
  // AstOptimizer(opt_mode, rules) makes. With TERM in `rules` in place of
  // YIELD, TERM's nodes are kept and YIELD's are not.
  const char *grammar = R"(
    PROGRAM     <-  STATEMENT (';' STATEMENT)*
    STATEMENT   <-  YIELD / ASSIGN / SUM
    YIELD       <-  'yield' EXPR               { no_ast_opt }
    ASSIGN      <-  NAME '=' EXPR
    SUM         <-  SUM '#' NUMBER / NUMBER
    EXPR        <-  TERM (OP TERM)* {
                      precedence
                        L + -
                        L * /
                    }
    TERM        <-  NUMBER / NAME / '(' EXPR ')' / LIST(EXPR, ',')
    LIST(I, D)  <-  '[' (I (D I)*)? ']'
    OP          <-  < [-+*/] >
    NUMBER      <-  < DIGIT+ >                 { ast_name: NUM }
    DIGIT       <-  [0-9]
    NAME        <-  < [a-z]+ >
    %whitespace <-  [ \t\r\n]*
  )";
  const char *src = "yield 1 + 2 * (x); a = [1, (2), b]; 7 # 8 # 9; 42";

  auto dump = [](const std::shared_ptr<Ast> &ast) {
    return ast_to_s<Ast>(ast, [](const Ast &node, int) {
      return "@" + std::to_string(node.position) + "+" +
             std::to_string(node.length) + " " + std::to_string(node.line) +
             ":" + std::to_string(node.column) + " " +
             std::to_string(node.preserve_position) + " " +
             std::to_string(node.tag) + "/" +
             std::to_string(node.original_tag) + "\n";
    });
  };

  std::function<size_t(const std::shared_ptr<Ast> &)> wrong_parents =
      [&](const std::shared_ptr<Ast> &ast) {
        size_t n = 0;
        for (const auto &child : ast->nodes) {
          if (child->parent.lock() != ast) { n++; }
          n += wrong_parents(child);
        }
        return n;
      };

  const std::vector<std::string> rules{"TERM"};
  for (auto packrat : {false, true}) {
    for (auto opt_mode : {true, false}) {
      for (auto own_rules : {false, true}) {
        parser full(grammar);
        parser collapsed(grammar);
        ASSERT_TRUE(full);
        if (packrat) {
          full.enable_packrat_parsing();
          collapsed.enable_packrat_parsing();
        }
        full.enable_ast();
        if (own_rules) {
          collapsed.enable_ast(true, opt_mode, rules);
        } else {
          collapsed.enable_ast(true, opt_mode);
        }

        std::shared_ptr<Ast> expected;
        std::shared_ptr<Ast> actual;
        ASSERT_TRUE(full.parse(src, expected));
        ASSERT_TRUE(collapsed.parse(src, actual));
        expected = own_rules ? AstOptimizer(opt_mode, rules).optimize(expected)
                             : full.optimize_ast(expected, opt_mode);

        EXPECT_EQ(dump(expected), dump(actual))
            << "packrat=" << packrat << " opt_mode=" << opt_mode
            << " own_rules=" << own_rules;
        EXPECT_EQ(0u, wrong_parents(actual));
        EXPECT_TRUE(actual->parent.expired());
      }
    }
  }
}

// Nodes get ids in the order they are first seen, so the same node seen twice
// (or seen in a callback and then found in the tree) shows up as the same id.
// The weak_ptrs keep addresses from being reused.
struct AstIds {
  std::map<const Ast *, size_t> ids;
  std::vector<std::weak_ptr<Ast>> keep;
  size_t id(const std::shared_ptr<Ast> &node) {
    auto it = ids.find(node.get());
    if (it != ids.end()) { return it->second; }
    keep.push_back(node);
    auto id = ids.size() + 1;
    ids[node.get()] = id;
    return id;
  }
};

static void dump_ast(const std::shared_ptr<Ast> &node, AstIds &ids,
                     std::string &out) {
  out += "#" + std::to_string(ids.id(node)) + " " + node->name + "|" +
         node->original_name + "|" + std::to_string(node->position) + "+" +
         std::to_string(node->length) + "|" + std::to_string(node->line) + ":" +
         std::to_string(node->column) + "|" +
         std::to_string(node->choice_count) + "/" +
         std::to_string(node->choice) + "|" +
         std::to_string(node->original_choice_count) + "/" +
         std::to_string(node->original_choice) + "|" +
         std::to_string(node->original_tag) + "|" +
         std::string(node->is_token ? node->token : "-") + "\n";
  for (const auto &child : node->nodes) {
    if (child->parent.lock() != node) { out += "WRONG PARENT\n"; }
    dump_ast(child, ids, out);
  }
}

TEST(GeneralTest, DeferredAstIsUnobservable) {
  // enable_ast(true) builds a node only once its rule match is kept (see
  // AstLogEntry). Predicates, leave handlers and user actions must see what
  // they see when every node is built right away (a tracer forces that), and
  // the tree must match optimize_ast's. The grammar backtracks over rules that
  // collapse in chains, token rules with inner rules, ignored rules, a
  // precedence rule and a macro, and has no left recursion (which would turn
  // deferring off).
  const char *grammar = R"(
    PROGRAM     <-  STATEMENT (';' STATEMENT)*
    STATEMENT   <-  YIELD / CALL / ASSIGN / VALUE
    YIELD       <-  'yield' VALUE              { no_ast_opt }
    CALL        <-  NAME '(' LIST(VALUE, ',') ')'
    ASSIGN      <-  NAME '=' VALUE ~BANG?
    VALUE       <-  WRAP / EXPR
    WRAP        <-  INNER EXCL
    INNER       <-  EXPR
    EXPR        <-  TERM (OP TERM)* {
                      precedence
                        L + -
                        L * /
                    }
    TERM        <-  NUMBER / NAME / '(' VALUE ')' / '[' LIST(VALUE, ',') ']'
    LIST(I, D)  <-  (I (D I)*)?
    OP          <-  < [-+*/] >
    NUMBER      <-  < DIGIT+ >                 { ast_name: NUM }
    DIGIT       <-  [0-9]
    NAME        <-  < [a-z]+ >
    BANG        <-  '??'
    ~EXCL       <-  '!'
    %whitespace <-  [ \t\r\n]*
  )";
  const char *src =
      "yield 1 + 2 * (x); f(1, [2, a + b!], (3)); a = [1, (2), b] ??; 42!";

  // What the callbacks saw, then the retained nodes and the tree as they are
  // after the parse.
  auto run = [&](bool eager, bool opt_mode, int observe) {
    parser pg(grammar);
    EXPECT_TRUE(pg);
    pg.enable_ast(true, opt_mode);
    if (eager) {
      pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
    }

    AstIds ids;
    std::string out;
    std::vector<std::shared_ptr<Ast>> retained;
    auto see = [&](const std::shared_ptr<Ast> &node) {
      dump_ast(node, ids, out);
      retained.push_back(node);
    };

    if (observe & 1) {
      for (auto name : {"STATEMENT", "INNER", "TERM", "OP", "NAME"}) {
        pg[name].predicate = [&, name](const SemanticValues &vs,
                                       const std::any &, std::string &) {
          out += std::string("P ") + name + "\n";
          for (const auto &v : vs) {
            see(std::any_cast<std::shared_ptr<Ast>>(v));
          }
          return true;
        };
      }
    }
    if (observe & 2) {
      for (auto name : {"VALUE", "WRAP", "EXPR", "NUMBER", "BANG"}) {
        pg[name].leave = [&, name](const Context &, const char *, size_t,
                                   size_t len, std::any &value, std::any &) {
          if (!success(len)) { return; }
          out += std::string("L ") + name + "\n";
          if (value.has_value()) {
            see(std::any_cast<std::shared_ptr<Ast>>(value));
          }
        };
      }
    }
    if (observe & 4) {
      pg["CALL"] = [&](const SemanticValues &vs) {
        out += "A CALL\n";
        auto node = std::make_shared<Ast>("", 1, 1, "CALL!",
                                          vs.transform<std::shared_ptr<Ast>>(),
                                          0, vs.sv().size());
        for (const auto &child : node->nodes) {
          see(child);
          child->parent = node;
        }
        return node;
      };
    }

    std::shared_ptr<Ast> ast;
    EXPECT_TRUE(pg.parse(src, ast));
    out += "AFTER\n";
    for (const auto &node : retained) {
      dump_ast(node, ids, out);
    }
    out += "TREE\n";
    dump_ast(ast, ids, out);
    return out;
  };

  for (auto opt_mode : {true, false}) {
    // The tree alone, against optimize_ast's.
    parser full(grammar);
    full.enable_ast();
    std::shared_ptr<Ast> expected;
    ASSERT_TRUE(full.parse(src, expected));
    expected = full.optimize_ast(expected, opt_mode);
    AstIds ids;
    std::string expected_tree = "AFTER\nTREE\n";
    dump_ast(expected, ids, expected_tree);
    EXPECT_EQ(expected_tree, run(false, opt_mode, 0))
        << "opt_mode=" << opt_mode;

    for (auto observe = 1; observe < 8; observe++) {
      EXPECT_EQ(run(true, opt_mode, observe), run(false, opt_mode, observe))
          << "opt_mode=" << opt_mode << " observe=" << observe;
    }
  }
}

TEST(GeneralTest, DeferredAstBuildsLongPrecedenceChain) {
  // A left-associative chain is parsed in a loop, but its tree is as deep as
  // the chain is long. Building it must not recurse that deep.
  parser pg("E <- A (OP A)* { precedence L + }\n A <- < [0-9] >\n OP <- '+'");
  ASSERT_TRUE(!!pg);
  pg.enable_ast(true);

  const size_t n = 100000;
  std::string s = "1";
  for (size_t i = 0; i < n; i++) {
    s += "+1";
  }
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse(s, ast));

  size_t folds = 0;
  for (auto node = ast; node->nodes.size() == 3; node = node->nodes[0]) {
    folds++;
  }
  EXPECT_EQ(n, folds);
}

static std::shared_ptr<Ast> ast_leaf() {
  return std::make_shared<Ast>("", 1, 1, "A", std::string_view("1"));
}

static std::shared_ptr<Ast> ast_node(std::vector<std::shared_ptr<Ast>> nodes) {
  return std::make_shared<Ast>("", 1, 1, "E", nodes);
}

TEST(GeneralTest, DeepAstIsReleased) {
  // Releasing a node must not recurse as deep as the tree below it.
  auto ast = ast_leaf();
  for (size_t i = 0; i < 1000000; i++) {
    ast = ast_node({ast});
  }
  ast.reset();
}

TEST(GeneralTest, ReleasedAstKeepsNodesHeldElsewhere) {
  // Only the nodes that die with a tree give up their children.
  auto kept = ast_node({ast_leaf(), ast_leaf()});
  auto shared = ast_node({kept, ast_leaf()});
  auto ast = ast_node({ast_node({shared}), ast_node({shared})});
  shared.reset();
  ast.reset();
  ASSERT_EQ(2u, kept->nodes.size());
  EXPECT_TRUE(kept->nodes[0] && kept->nodes[1]);
}

TEST(GeneralTest, RecognizerPathIsUnobservable) {
  // A rule match whose value nobody reads, or is always empty, builds no
  // value (see Holder::parse_core). Callbacks must see what they see when
  // every match builds its value (a tracer forces that), with and without
  // AST. The grammar has matches under `~`, `&` and `!`, inside token rules
  // and in whitespace, a predicate on a token rule, and precedence rules, one
  // of them in a macro used under `&`.
  const char *grammar = R"(
    PROGRAM     <-  &TERM STATEMENT (';' STATEMENT)* END?
    STATEMENT   <-  DECL / CALL / &CHECK &INFIX(TERM, OP) EXPR
    CHECK       <-  INFIX(TERM, OP)
    INFIX(A, O) <-  A (O A)* {
                      precedence
                        L + -
                        L * /
                    }
    DECL        <-  TYPE NAME ('=' EXPR)?
    TYPE        <-  < NAME >
    CALL        <-  NAME '(' LIST(EXPR, ',') ')'
    EXPR        <-  TERM (OP TERM)* {
                      precedence
                        L + -
                        L * /
                    }
    TERM        <-  NUMBER / NAME / '(' EXPR ')' / '[' LIST(EXPR, ',') ']'
                  / &'-' SIGNED
    SIGNED      <-  '-' NUMBER
    LIST(I, D)  <-  (I (D I)*)?
    OP          <-  < '+' / '-' / '*' / '/' >
    NUMBER      <-  < DIGIT+ FRACTION? >
    FRACTION    <-  '.' DIGIT+
    DIGIT       <-  [0-9]
    NAME        <-  !KEYWORD < ALPHA (ALPHA / DIGIT)* >
    KEYWORD     <-  ('if' / 'else') !ALPHA
    ALPHA       <-  [a-z]
    ~END        <-  '.' ALPHA*
    COMMENT     <-  '#' (!EOL .)* EOL
    EOL         <-  '\r\n' / '\n'
    %whitespace <-  [ \t\r\n]* COMMENT?
  )";
  const char *src =
      "int a = 1 + # c\n2 * (b - 3.5); f(1, [2, -4], iff); elsex / 6 .end";

  auto run = [&](bool eager, bool ast, int observe) {
    parser pg(grammar);
    EXPECT_TRUE(pg);
    if (ast) { pg.enable_ast(true); }
    if (eager) {
      pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
    }

    AstIds ids;
    std::string out;
    auto see = [&](const std::any &value) {
      if (!value.has_value()) {
        out += "  (empty)\n";
      } else {
        dump_ast(std::any_cast<std::shared_ptr<Ast>>(value), ids, out);
      }
    };
    auto see_scope = [&](const char *name, const SemanticValues &vs) {
      out += std::string(name) + " " + std::to_string(vs.size()) + " " +
             std::to_string(vs.tags.size()) + " " +
             std::to_string(vs.tokens.size()) + " " + vs.token_to_string() +
             " " + std::to_string(vs.choice_count()) + "/" +
             std::to_string(vs.choice()) + "\n";
      for (const auto &v : vs) {
        see(v);
      }
    };

    pg["TYPE"].predicate = [&](const SemanticValues &vs, const std::any &,
                               std::string &) {
      see_scope("TYPE", vs);
      return vs.token() == "int";
    };
    if (observe & 1) {
      for (auto name : {"STATEMENT", "OP", "NUMBER", "CALL"}) {
        pg[name].predicate = [&, name](const SemanticValues &vs,
                                       const std::any &, std::string &) {
          see_scope(name, vs);
          return true;
        };
      }
    }
    if (observe & 2) {
      for (auto name : {"TERM", "FRACTION", "COMMENT", "KEYWORD"}) {
        pg[name].leave = [&, name](const Context &, const char *, size_t,
                                   size_t len, std::any &value, std::any &) {
          if (!success(len)) { return; }
          out += std::string("L ") + name + "\n";
          see(value);
        };
      }
    }
    if (ast && (observe & 4)) {
      // Without an action, TERM's value is its first value.
      pg["TERM"].action = Action();
      pg["SIGNED"] = [&](const SemanticValues &vs) {
        see_scope("SIGNED", vs);
        return std::make_shared<Ast>("", 1, 1, "SIGNED!",
                                     vs.transform<std::shared_ptr<Ast>>(), 0,
                                     vs.sv().size());
      };
    }

    std::shared_ptr<Ast> tree;
    EXPECT_TRUE(ast ? pg.parse(src, tree) : pg.parse(src));
    if (tree) {
      out += "TREE\n";
      dump_ast(tree, ids, out);
    }
    return out;
  };

  for (auto ast : {true, false}) {
    for (auto observe = 0; observe < 8; observe++) {
      EXPECT_EQ(run(true, ast, observe), run(false, ast, observe))
          << "ast=" << ast << " observe=" << observe;
    }
  }

  // An AST action that would throw on a value that is not a node still runs
  // where its value is not read.
  for (auto eager : {true, false}) {
    parser pg(R"(
      S <- &A 'a'
      A <- B
      B <- 'a'
    )");
    pg.enable_ast(true);
    pg["B"] = [](const SemanticValues &) { return 1; };
    if (eager) {
      pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
    }
    std::shared_ptr<Ast> tree;
    EXPECT_THROW(pg.parse("a", tree), std::bad_any_cast) << "eager=" << eager;
  }

  // A rule without an action whose value is not read where it first matches
  // still builds it when the packrat cache or a left-recursive seed keeps it
  // for a later match that reads it.
  auto first_value = [](const char *grammar, const char *src, bool packrat,
                        bool eager) {
    parser pg(grammar);
    EXPECT_TRUE(pg);
    if (packrat) { pg.enable_packrat_parsing(); }
    if (pg.get_grammar().count("Q")) {
      pg["Q"].enter = [](auto &&...) {};
    }
    pg["D"] = [](const SemanticValues &vs) {
      return vs.token_to_number<int>();
    };
    std::string out;
    pg["S"] = [&](const SemanticValues &vs) {
      for (const auto &v : vs) {
        out += v.has_value() ? std::to_string(std::any_cast<int>(v)) : "-";
      }
    };
    if (eager) {
      pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
    }
    EXPECT_TRUE(pg.parse(src));
    return out;
  };
  const char *memoized = R"(
    S <- ~Q 'x' / Q
    Q <- C D
    C <- 'a'
    D <- [0-9]
  )";
  EXPECT_EQ(first_value(memoized, "a1", true, true),
            first_value(memoized, "a1", true, false));
  const char *left_recursive = R"(
    S <- &E E
    E <- E P D / Z D
    P <- '+'
    Z <- 'z'
    D <- [0-9]
  )";
  EXPECT_EQ(first_value(left_recursive, "z1+2", false, true),
            first_value(left_recursive, "z1+2", false, false));

  // A start rule sees the callbacks as they are even after another start rule
  // analyzed the rules they share.
  parser pg(R"(
    S <- &X 'a'
    X <- 'a'
  )");
  size_t leaves = 0;
  auto leave = [&](auto &&...) { leaves++; };
  pg["X"].leave = leave;
  EXPECT_TRUE(pg.parse("a"));
  pg["X"].leave = nullptr;
  EXPECT_TRUE(pg["X"].parse("a").ret);
  pg["X"].leave = leave;
  EXPECT_TRUE(pg.parse("a"));
  EXPECT_EQ(2u, leaves);
}

TEST(GeneralTest, CollapsedAstParentLinks) {
  std::function<size_t(const std::shared_ptr<Ast> &)> wrong_parents =
      [&](const std::shared_ptr<Ast> &ast) {
        size_t n = 0;
        for (const auto &child : ast->nodes) {
          if (child->parent.lock() != ast) { n++; }
          n += wrong_parents(child);
        }
        return n;
      };

  {
    // A second enable_ast() leaves the collapsing actions in place, so the
    // parent links must still be fixed up after each parse.
    parser pg(R"(
      T <- A '!' / B
      A <- X
      B <- X Z
      X <- K L
      K <- 'k'
      L <- 'l'
      Z <- 'z'
    )");
    pg.enable_packrat_parsing();
    pg.enable_ast(true);
    pg.enable_ast();

    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("klz", ast));
    EXPECT_EQ(0u, wrong_parents(ast));
  }

  {
    // Packrat hands the same zero-length E node to both X and Y.
    parser pg(R"(
      S  <- X Y
      X  <- A0 E
      Y  <- E B0 / E C0
      E  <- 'e'?
      A0 <- 'a'
      B0 <- 'b'
      C0 <- 'c'
    )");
    pg.enable_packrat_parsing();
    pg.enable_ast(true);

    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("ab", ast));
    EXPECT_EQ(0u, wrong_parents(ast));
  }
}

TEST(GeneralTest, ChoiceWithWhitespace) {
  auto parser = peg::parser(R"(
    type <- 'string' / 'int' / 'double'
    %whitespace <- ' '*
  )");

  parser["type"] = [](const SemanticValues &vs) {
    auto n = vs.choice();
    EXPECT_EQ(1, n);
  };

  auto ret = parser.parse("int");
  EXPECT_TRUE(ret);
}

TEST(GeneralTest, PassingContextAndOutputParameter) {
  parser parser(R"(
        START  <- TOKEN
        TOKEN  <- [0-9]+
    )");

  parser["TOKEN"] = [&](const peg::SemanticValues &vs, std::any & /*dt*/) {
    return vs.token_to_number<int>();
  };

  int output = 0;
  std::any dt = std::string{"context"};
  parser.parse<int>("42", dt, output);
  EXPECT_EQ(42, output);
}

TEST(GeneralTest, SpecifyStartRule) {
  auto grammar = R"(
    Start       <- A
    A           <- B (',' B)*
    B           <- '[one]' / '[two]'
    %whitespace <- [ \t\n]*
  )";

  {
    parser peg(grammar, "AAA");
    EXPECT_FALSE(peg);
  }

  {
    parser peg(grammar, "A");
    EXPECT_TRUE(peg.parse(" [one] , [two] "));
  }

  {
    parser peg(grammar);
    EXPECT_TRUE(peg.parse(" [one] , [two] "));

    peg.load_grammar(grammar, "A");
    EXPECT_TRUE(peg.parse(" [one] , [two] "));
  }

  {
    parser peg;

    peg.load_grammar(grammar);
    EXPECT_TRUE(peg.parse(" [one] , [two] "));

    peg.load_grammar(grammar, "A");
    EXPECT_TRUE(peg.parse(" [one] , [two] "));
  }
}

TEST(GeneralTest, InvalidRange) {
  parser parser("S <- [z-a]");

  bool ret = parser;
  EXPECT_FALSE(ret);
}

// =============================================================================
// AST Choice Tests
// =============================================================================

TEST(AstChoiceTest, Choice_suffix_top_level) {
  // A rule whose body is a choice carries the /N choice suffix.
  parser pg(R"(
    S <- A / B
    A <- < 'a' >
    B <- < 'b' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("a", ast));
  EXPECT_EQ(R"(+ S/0
  - A (a)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("b", ast));
  EXPECT_EQ(R"(+ S/1
  - B (b)
)",
            ast_to_s(ast));
}

TEST(AstChoiceTest, No_choice_suffix_nested_in_sequence) {
  // A choice nested inside a sequence does not leak a choice suffix onto the
  // rule node.
  parser pg(R"(
    S <- (A / B) C
    A <- < 'a' >
    B <- < 'b' >
    C <- < 'c' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("ac", ast));
  EXPECT_EQ(R"(+ S
  - A (a)
  - C (c)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("bc", ast));
  EXPECT_EQ(R"(+ S
  - B (b)
  - C (c)
)",
            ast_to_s(ast));
}

TEST(AstChoiceTest, No_choice_suffix_choice_at_sequence_end) {
  // A trailing choice inside a sequence still does not add a choice suffix.
  parser pg(R"(
    S <- A (B / C)
    A <- < 'a' >
    B <- < 'b' >
    C <- < 'c' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("ab", ast));
  EXPECT_EQ(R"(+ S
  - A (a)
  - B (b)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("ac", ast));
  EXPECT_EQ(R"(+ S
  - A (a)
  - C (c)
)",
            ast_to_s(ast));
}

TEST(AstChoiceTest, Choice_of_sequences) {
  // A top-level choice of sequences carries the choice suffix.
  parser pg(R"(
    S <- A B / C D
    A <- < 'a' >
    B <- < 'b' >
    C <- < 'c' >
    D <- < 'd' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("ab", ast));
  EXPECT_EQ(R"(+ S/0
  - A (a)
  - B (b)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("cd", ast));
  EXPECT_EQ(R"(+ S/1
  - C (c)
  - D (d)
)",
            ast_to_s(ast));
}

TEST(AstChoiceTest, Token_boundary_over_choice) {
  // A token boundary is unwrapped: an inner choice still carries the suffix.
  parser pg(R"(
    S <- < A / B >
    A <- < 'a' >
    B <- < 'b' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("a", ast));
  EXPECT_EQ(R"(- S/0 (a)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("b", ast));
  EXPECT_EQ(R"(- S/1 (b)
)",
            ast_to_s(ast));
}

TEST(AstChoiceTest, Dictionary_choice_suffix) {
  // A dictionary is choice-like and carries the choice suffix.
  parser pg(R"(S <- 'a' | 'b' | 'c')");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("a", ast));
  EXPECT_EQ(R"(- S/0 (a)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("b", ast));
  EXPECT_EQ(R"(- S/1 (b)
)",
            ast_to_s(ast));

  EXPECT_TRUE(pg.parse("c", ast));
  EXPECT_EQ(R"(- S/2 (c)
)",
            ast_to_s(ast));
}

// =============================================================================
// AST Instruction Tests
// =============================================================================

TEST(AstInstructionTest, No_ast_opt_instruction) {
  // A rule marked { no_ast_opt } is not collapsed by optimization.
  parser pg(R"(
    S <- Wrapped
    Wrapped <- Inner { no_ast_opt }
    Inner <- < [a-z]+ >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("abc", ast));
  ast = pg.optimize_ast(ast);
  EXPECT_EQ(R"(+ S[Wrapped]
  - Inner (abc)
)",
            ast_to_s(ast));
}

TEST(AstInstructionTest, Ast_name_instruction) {
  // { ast_name: X } overrides the node's emitted name.
  parser pg(R"(
    S <- Item
    Item <- < [a-z]+ > { ast_name: Word }
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("abc", ast));
  EXPECT_EQ(R"(+ S
  - Word (abc)
)",
            ast_to_s(ast));
}

// =============================================================================
// Ignore Tests
// =============================================================================

TEST(IgnoreTest, Ignore_drops_only_target_in_ast) {
  // ~ on a reference drops only that node's AST value, keeping siblings
  parser pg(R"(
    S <- ~A B
    A <- 'a'
    B <- < 'b' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("ab", ast));
  EXPECT_EQ(R"(+ S
  - B (b)
)",
            ast_to_s(ast));
}

TEST(IgnoreTest, Ignore_middle_reference_in_ast) {
  // ~ in the middle of a sequence keeps the non-ignored siblings
  parser pg(R"(
    S <- A ~B C
    A <- < 'a' >
    B <- 'b'
    C <- < 'c' >
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("abc", ast));
  EXPECT_EQ(R"(+ S
  - A (a)
  - C (c)
)",
            ast_to_s(ast));
}

TEST(IgnoreTest, Ignore_repeated_reference_in_ast) {
  // (~A)+ matches but contributes no AST children
  parser pg(R"(
    S <- (~A)+
    A <- 'a'
  )");
  ASSERT_TRUE(!!pg);
  pg.enable_ast();
  std::shared_ptr<Ast> ast;

  EXPECT_TRUE(pg.parse("aaa", ast));
  EXPECT_EQ(R"(+ S
)",
            ast_to_s(ast));
}

// =============================================================================
// Start Rule Tests
// =============================================================================

TEST(StartRuleTest, Word_directive_not_default_start) {
  // A reserved %word directive defined first must not become the default start
  // rule; the first ordinary rule (S) is the start.
  parser pg(R"(
    %word <- [a-z]+
    S <- 'cat'
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("cat"));
  EXPECT_FALSE(pg.parse("dog"));
}

TEST(StartRuleTest, Whitespace_directive_not_default_start) {
  // A reserved %whitespace directive defined first must not become the default
  // start rule.
  parser pg(R"(
    %whitespace <- [ ]*
    S <- 'cat'
  )");
  ASSERT_TRUE(!!pg);

  EXPECT_TRUE(pg.parse("cat"));
  EXPECT_FALSE(pg.parse("dog"));
}

// =============================================================================
// Semantic Value Tests
// =============================================================================

TEST(SemanticValueTest, Ignore_shifts_sv_index) {
  // A ~ignored child leaves no semantic value, so later children shift down in
  // the sv index.
  parser pg(R"(
    S <- ~A '+' B
    A <- < [0-9]+ >
    B <- < [0-9]+ >
  )");
  ASSERT_TRUE(!!pg);

  pg["A"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };
  pg["B"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };
  pg["S"] = [](const SemanticValues &vs) { return vs[0]; };

  long val = 0;
  EXPECT_TRUE(pg.parse("1+2", val));
  EXPECT_EQ(2, val);
  EXPECT_TRUE(pg.parse("10+20", val));
  EXPECT_EQ(20, val);
}

TEST(SemanticValueTest, Choice_index_value) {
  // vs.choice() reflects which alternative matched.
  parser pg(R"(S <- 'a' / 'b' / 'c')");
  ASSERT_TRUE(!!pg);

  pg["S"] = [](const SemanticValues &vs) {
    return static_cast<long>(vs.choice());
  };

  long val = -1;
  EXPECT_TRUE(pg.parse("a", val));
  EXPECT_EQ(0, val);
  EXPECT_TRUE(pg.parse("b", val));
  EXPECT_EQ(1, val);
  EXPECT_TRUE(pg.parse("c", val));
  EXPECT_EQ(2, val);
}

TEST(SemanticValueTest, Value_by_index) {
  // vs[i] is the value of the i-th child that has one.
  parser pg(R"(
    S <- A B
    A <- < [0-9] >
    B <- < [0-9] >
  )");
  ASSERT_TRUE(!!pg);

  pg["A"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };
  pg["B"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };
  pg["S"] = [](const SemanticValues &vs) { return vs[1]; };

  long val = 0;
  EXPECT_TRUE(pg.parse("12", val));
  EXPECT_EQ(2, val);
}
