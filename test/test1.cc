#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

#if !defined(PEGLIB_NO_UNICODE_CHARS)
TEST(GeneralTest, Simple_syntax_test_with_unicode) {
  parser parser(u8" ROOT ← _ "
                " _ <- ' ' ");

  bool ret = parser;
  EXPECT_TRUE(ret);
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

