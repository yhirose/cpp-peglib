#include "catch.hh"
#include <peglib.h>
#include <sstream>

using namespace peg;

TEST_CASE("Token boundary 1", "[token boundary]")
{
    parser pg(R"(
        ROOT        <- TOP
        TOP         <- 'a' 'b' 'c'
        %whitespace <- [ \t\r\n]*
    )");

    REQUIRE(pg.parse(" a  b  c "));
}

TEST_CASE("Token boundary 2", "[token boundary]")
{
    parser pg(R"(
        ROOT        <- TOP
        TOP         <- < 'a' 'b' 'c' >
        %whitespace <- [ \t\r\n]*
    )");

    REQUIRE(!pg.parse(" a  b  c "));
}

TEST_CASE("Token boundary 3", "[token boundary]")
{
    parser pg(R"(
        ROOT        <- TOP
        TOP         <- < 'a' B 'c' >
        B           <- 'b'
        %whitespace <- [ \t\r\n]*
    )");

    REQUIRE(!pg.parse(" a  b  c "));
}

TEST_CASE("Token boundary 4", "[token boundary]")
{
    parser pg(R"(
        ROOT        <- TOP
        TOP         <- < A 'b' 'c' >
        A           <- 'a'
        %whitespace <- [ \t\r\n]*
    )");

    REQUIRE(!pg.parse(" a  b  c "));
}

TEST_CASE("Token boundary 5", "[token boundary]")
{
    parser pg(R"(
        ROOT        <- TOP
        TOP         <- A < 'b' C >
        A           <- 'a'
        C           <- 'c'
        %whitespace <- [ \t\r\n]*
    )");

    REQUIRE(!pg.parse(" a  b  c "));
}

TEST_CASE("Token boundary 6", "[token boundary]")
{
    parser pg(R"(
        ROOT        <- TOP
        TOP         <- < A > B C
        A           <- 'a'
        B           <- 'b'
        C           <- 'c'
        %whitespace <- [ \t\r\n]*
    )");

    REQUIRE(pg.parse(" a  b  c "));
}

TEST_CASE("Token boundary 7", "[token boundary]")
{
    parser pg(R"(
        ROOT        <- TOP
        TOP         <- < A B C >
        A           <- 'a'
        B           <- 'b'
        C           <- 'c'
        %whitespace <- [ \t\r\n]*
    )");

    REQUIRE(!pg.parse(" a  b  c "));
}

TEST_CASE("Infinite loop 1", "[infinite loop]")
{
    parser pg(R"(
        ROOT  <- WH TOKEN* WH
        TOKEN <- [a-z0-9]*
        WH    <- [ \t]*
    )");

  REQUIRE(!pg);
}

TEST_CASE("Infinite loop 2", "[infinite loop]") {
  parser pg(R"(
        ROOT  <- WH TOKEN+ WH
        TOKEN <- [a-z0-9]*
        WH    <- [ \t]*
    )");

  REQUIRE(!pg);
}

TEST_CASE("Infinite loop 3", "[infinite loop]") {
  parser pg(R"(
        ROOT  <- WH TOKEN* WH
        TOKEN <- !'word1'
        WH    <- [ \t]*
    )");

  REQUIRE(!pg);
}

TEST_CASE("Infinite loop 4", "[infinite loop]") {
  parser pg(R"(
        ROOT  <- WH TOKEN* WH
        TOKEN <- &'word1'
        WH    <- [ \t]*
    )");

  REQUIRE(!pg);
}

TEST_CASE("Infinite loop 5", "[infinite loop]") {
  parser pg(R"(
        Numbers <- Number*
        Number <- [0-9]+ / Spacing
        Spacing <- ' ' / '\t' / '\n' / EOF # EOF is empty
        EOF <- !.
    )");

  REQUIRE(!pg);
}

TEST_CASE("Infinite loop 6", "[infinite loop]") {
  parser pg(R"(
        S <- ''*
    )");

  REQUIRE(!pg);
}

TEST_CASE("Infinite loop 7", "[infinite loop]") {
  parser pg(R"(
        S <- A*
        A <- ''
    )");

  REQUIRE(!pg);
}

TEST_CASE("Infinite loop 8", "[infinite loop]") {
    parser pg(R"(
        ROOT <- ('A' /)*
    )");

    REQUIRE(!pg);
}

TEST_CASE("Infinite 9", "[infinite loop]") {
  parser pg(R"(
START      <- __? SECTION*

SECTION    <- HEADER __ ENTRIES __?

HEADER     <- '[' _ CATEGORY (':' _  ATTRIBUTES)? ']'^header

CATEGORY   <- < [-_a-zA-Z0-9\u0080-\uFFFF ]+ > _
ATTRIBUTES <- ATTRIBUTE (',' _ ATTRIBUTE)*
ATTRIBUTE  <- < [-_a-zA-Z0-9\u0080-\uFFFF]+ > _

ENTRIES    <- (ENTRY (__ ENTRY)*)?

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

header <- (!__ .)* { message "invalid section header, missing ']'." }

# The `(!(__ / HEADER) )+` should be `(!(__ / HEADER) .)+`
entry  <- (!(__ / HEADER) )+ { message "invalid token '%t', expecting another phrase." }
  )");

    REQUIRE(!pg);
}

TEST_CASE("Not infinite 1", "[infinite loop]") {
  parser pg(R"(
        Numbers <- Number* EOF
        Number <- [0-9]+ / Spacing
        Spacing <- ' ' / '\t' / '\n'
        EOF <- !.
    )");

  REQUIRE(!!pg); // OK
}

TEST_CASE("Not infinite 2", "[infinite loop]") {
  parser pg(R"(
        ROOT      <-  _ ('[' TAG_NAME ']' _)*
        # In a sequence operator, if there is at least one non-empty element, we can treat it as non-empty
        TAG_NAME  <-  (!']' .)+
        _         <-  [ \t]*
    )");

  REQUIRE(!!pg); // OK
}

TEST_CASE("Not infinite 3", "[infinite loop]") {
  parser pg(R"(
        EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _ # Recursive...
        TERM_OPERATOR    <-  < [-+] > _
        FACTOR_OPERATOR  <-  < [/*] > _
        NUMBER           <-  < [0-9]+ > _
        _                <-  [ \t\r\n]*
    )");

  REQUIRE(!!pg); // OK
}

TEST_CASE("Precedence climbing", "[precedence]") {
  parser parser(R"(
        START            <-  _ EXPRESSION
        EXPRESSION       <-  ATOM (OPERATOR ATOM)* {
                               precedence
                                 L + -
                                 L * /
                             }
        ATOM             <-  NUMBER / T('(') EXPRESSION T(')')
        OPERATOR         <-  T([-+/*])
        NUMBER           <-  T('-'? [0-9]+)
		~_               <-  [ \t]*
		T(S)             <-  < S > _
	)");

  REQUIRE(!!parser); // OK

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
  parser["NUMBER"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };

  bool ret = parser;
  REQUIRE(ret == true);

  {
    auto expr = " 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ";
    long val = 0;
    ret = parser.parse(expr, val);

    REQUIRE(ret == true);
    REQUIRE(val == -3);
  }

  {
    auto expr = "-1+-2--3"; // -1 + -2 - -3 = 0
    long val = 0;
    ret = parser.parse(expr, val);

    REQUIRE(ret == true);
    REQUIRE(val == 0);
  }
}

TEST_CASE("Precedence climbing with literal operator", "[precedence]") {
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

  REQUIRE(!!parser); // OK

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
  parser["OPERATOR"] = [](const SemanticValues &vs) { return vs.token_to_string(); };
  parser["NUMBER"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };

  bool ret = parser;
  REQUIRE(ret == true);

  {
    auto expr = " 1 #plus#  2 #multiply# 3 #multiply# (4 - 5 #plus# 6) / 7 - 8 ";
    long val = 0;
    ret = parser.parse(expr, val);

    REQUIRE(ret == true);
    REQUIRE(val == -3);
  }

  {
    auto expr = "-1#plus#-2--3"; // -1 + -2 - -3 = 0
    long val = 0;
    ret = parser.parse(expr, val);

    REQUIRE(ret == true);
    REQUIRE(val == 0);
  }
}

TEST_CASE("Precedence climbing with macro", "[precedence]") {
  // Create a PEG parser
  parser parser(R"(
        EXPRESSION             <-  INFIX_EXPRESSION(ATOM, OPERATOR)
        INFIX_EXPRESSION(A, O) <-  A (O A)* {
                                     precedence
                                       L + -
                                       L * /
                                   }
        ATOM                   <-  NUMBER / '(' EXPRESSION ')'
        OPERATOR               <-  < [-+/*] >
        NUMBER                 <-  < '-'? [0-9]+ >
        %whitespace            <-  [ \t]*
	)");

  parser.enable_packrat_parsing();

  bool ret = parser;
  REQUIRE(ret == true);

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
  parser["NUMBER"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };

  {
    auto expr = " 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ";
    long val = 0;
    ret = parser.parse(expr, val);

    REQUIRE(ret == true);
    REQUIRE(val == -3);
  }

  {
    auto expr = "-1+-2--3"; // -1 + -2 - -3 = 0
    long val = 0;
    ret = parser.parse(expr, val);

    REQUIRE(ret == true);
    REQUIRE(val == 0);
  }
}

TEST_CASE("Precedence climbing error1", "[precedence]") {
  parser parser(R"(
        START            <-  _ EXPRESSION
        EXPRESSION       <-  ATOM (OPERATOR ATOM1)* {
                               precedence
                                 L + -
                                 L * /
                             }
        ATOM             <-  NUMBER / T('(') EXPRESSION T(')')
        ATOM1            <-  NUMBER / T('(') EXPRESSION T(')')
        OPERATOR         <-  T([-+/*])
        NUMBER           <-  T('-'? [0-9]+)
		~_               <-  [ \t]*
		T(S)             <-  < S > _
	)");

  bool ret = parser;
  REQUIRE(ret == false);
}

TEST_CASE("Precedence climbing error2", "[precedence]") {
  parser parser(R"(
        START            <-  _ EXPRESSION
        EXPRESSION       <-  ATOM OPERATOR ATOM {
                               precedence
                                 L + -
                                 L * /
                             }
        ATOM             <-  NUMBER / T('(') EXPRESSION T(')')
        OPERATOR         <-  T([-+/*])
        NUMBER           <-  T('-'? [0-9]+)
		~_               <-  [ \t]*
		T(S)             <-  < S > _
	)");

  bool ret = parser;
  REQUIRE(ret == false);
}

TEST_CASE("Precedence climbing error3", "[precedence]") {
  parser parser(R"(
        EXPRESSION               <-  PRECEDENCE_PARSING(ATOM, OPERATOR)
        PRECEDENCE_PARSING(A, O) <-  A (O A)+ {
                                       precedence
                                         L + -
                                         L * /
                                     }
        ATOM                     <-  NUMBER / '(' EXPRESSION ')'
        OPERATOR                 <-  < [-+/*] >
        NUMBER                   <-  < '-'? [0-9]+ >
        %whitespace              <-  [ \t]*
	)");

  bool ret = parser;
  REQUIRE(ret == false);
}

TEST_CASE("Packrat parser test with %whitespace%", "[packrat]") {
  peg::parser parser(R"(
        ROOT         <-  'a'
        %whitespace  <-  SPACE*
        SPACE        <-  ' '
    )");

  parser.enable_packrat_parsing();

  auto ret = parser.parse("a");
  REQUIRE(ret == true);
}

TEST_CASE("Packrat parser test with macro", "[packrat]") {
  parser parser(R"(
        EXPRESSION       <-  _ LIST(TERM, TERM_OPERATOR)
        TERM             <-  LIST(FACTOR, FACTOR_OPERATOR)
        FACTOR           <-  NUMBER / T('(') EXPRESSION T(')')
        TERM_OPERATOR    <-  T([-+])
        FACTOR_OPERATOR  <-  T([/*])
        NUMBER           <-  T([0-9]+)
		~_               <-  [ \t]*
		LIST(I, D)       <-  I (D I)*
		T(S)             <-  < S > _
	)");

  parser.enable_packrat_parsing();

  auto ret = parser.parse(" 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ");
  REQUIRE(ret == true);
}

TEST_CASE("Packrat parser test with precedence expression parser",
          "[packrat]") {
  peg::parser parser(R"(
    Expression  <- Atom (Operator Atom)* { precedence L + - L * / }
    Atom        <- _? Number _?
    Number      <- [0-9]+
    Operator    <- '+' / '-' / '*' / '/'
    _           <- ' '+
  )");

  bool ret = parser;
  REQUIRE(ret == true);

  parser.enable_packrat_parsing();

  ret = parser.parse(" 1 + 2 * 3 ");
  REQUIRE(ret == true);
}

TEST_CASE("Backreference test", "[backreference]") {
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

    REQUIRE(ret == true);
    REQUIRE(token == "\"hello world\"");
  }

  {
    token.clear();
    auto ret = parser.parse(R"delm(
            R"foo("(hello world)")foo"
        )delm");

    REQUIRE(ret == true);
    REQUIRE(token == "\"(hello world)\"");
  }

  {
    token.clear();
    auto ret = parser.parse(R"delm(
            R"foo("(hello world)foo")foo"
        )delm");

    REQUIRE(ret == false);
    REQUIRE(token == "\"(hello world");
  }

  {
    token.clear();
    auto ret = parser.parse(R"delm(
            R"foo("(hello world)")bar"
        )delm");

    REQUIRE(ret == false);
    REQUIRE(token.empty());
  }
}

TEST_CASE("Invalid backreference test", "[backreference]") {
  parser parser(R"(
        START  <- _ LQUOTE (!RQUOTE .)* RQUOTE _
        LQUOTE <- 'R"' $delm< [a-zA-Z]* > '('
        RQUOTE <- ')' $delm2 '"'
        ~_     <- [ \t\r\n]*
    )");

  REQUIRE_THROWS_AS(parser.parse(R"delm(
            R"foo("(hello world)")foo"
        )delm"),
                    std::runtime_error);
}

TEST_CASE("Nested capture test", "[backreference]") {
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

  REQUIRE(parser.parse("This is <b>a <u>test</u> text</b>."));
  REQUIRE(!parser.parse("This is <b>a <u>test</b> text</u>."));
  REQUIRE(!parser.parse("This is <b>a <u>test text</b>."));
  REQUIRE(!parser.parse("This is a <u>test</u> text</b>."));
}

TEST_CASE("Backreference with Prioritized Choice test", "[backreference]") {
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

  REQUIRE_THROWS_AS(parser.parse("branchthatiscorrect"), std::runtime_error);
}

TEST_CASE("Backreference with Zero or More test", "[backreference]") {
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

  REQUIRE(parser.parse("branchthatiswrongbranchthatiscorrect"));
  REQUIRE(!parser.parse("branchthatiswrongbranchthatIscorrect"));
  REQUIRE(
      !parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
  REQUIRE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
  REQUIRE_THROWS_AS(parser.parse("branchthatiscorrect"), std::runtime_error);
  REQUIRE_THROWS_AS(parser.parse("branchthatiswron_branchthatiscorrect"),
                    std::runtime_error);
}

TEST_CASE("Backreference with One or More test", "[backreference]") {
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

  REQUIRE(parser.parse("branchthatiswrongbranchthatiscorrect"));
  REQUIRE(!parser.parse("branchthatiswrongbranchthatIscorrect"));
  REQUIRE(
      !parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
  REQUIRE(
      parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
  REQUIRE(!parser.parse("branchthatiscorrect"));
  REQUIRE(!parser.parse("branchthatiswron_branchthatiscorrect"));
}

TEST_CASE("Backreference with Option test", "[backreference]") {
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

  REQUIRE(parser.parse("branchthatiswrongbranchthatiscorrect"));
  REQUIRE(!parser.parse("branchthatiswrongbranchthatIscorrect"));
  REQUIRE(
      !parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
  REQUIRE(
      !parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
  REQUIRE_THROWS_AS(parser.parse("branchthatiscorrect"), std::runtime_error);
  REQUIRE_THROWS_AS(parser.parse("branchthatiswron_branchthatiscorrect"),
                    std::runtime_error);
}

TEST_CASE("Repetition {0}", "[repetition]") {
  parser parser(R"(
        START <- '(' DIGIT{3} ') ' DIGIT{3} '-' DIGIT{4}
        DIGIT <- [0-9]
    )");
  REQUIRE(parser.parse("(123) 456-7890"));
  REQUIRE(!parser.parse("(12a) 456-7890"));
  REQUIRE(!parser.parse("(123) 45-7890"));
  REQUIRE(!parser.parse("(123) 45-7a90"));
}

TEST_CASE("Repetition {2,4}", "[repetition]") {
  parser parser(R"(
        START <- DIGIT{2,4}
        DIGIT <- [0-9]
    )");
  REQUIRE(!parser.parse("1"));
  REQUIRE(parser.parse("12"));
  REQUIRE(parser.parse("123"));
  REQUIRE(parser.parse("1234"));
  REQUIRE(!parser.parse("12345"));
}

TEST_CASE("Repetition {2,1}", "[repetition]") {
  parser parser(R"(
        START <- DIGIT{2,1} # invalid range
        DIGIT <- [0-9]
    )");
  REQUIRE(!parser.parse("1"));
  REQUIRE(parser.parse("12"));
  REQUIRE(!parser.parse("123"));
}

TEST_CASE("Repetition {2,}", "[repetition]") {
  parser parser(R"(
        START <- DIGIT{2,}
        DIGIT <- [0-9]
    )");
  REQUIRE(!parser.parse("1"));
  REQUIRE(parser.parse("12"));
  REQUIRE(parser.parse("123"));
  REQUIRE(parser.parse("1234"));
}

TEST_CASE("Repetition {,2}", "[repetition]") {
  parser parser(R"(
        START <- DIGIT{,2}
        DIGIT <- [0-9]
    )");
  REQUIRE(parser.parse("1"));
  REQUIRE(parser.parse("12"));
  REQUIRE(!parser.parse("123"));
  REQUIRE(!parser.parse("1234"));
}

TEST_CASE("Left recursive test", "[left recursive]") {
  parser parser(R"(
        A <- A 'a'
        B <- A 'a'
    )");

  REQUIRE(!parser);
}

TEST_CASE("Left recursive with option test", "[left recursive]") {
  parser parser(R"(
        A  <- 'a' / 'b'? B 'c'
        B  <- A
    )");

  REQUIRE(!parser);
}

TEST_CASE("Left recursive with zom test", "[left recursive]") {
  parser parser(R"(
        A <- 'a'* A*
    )");

  REQUIRE(!parser);
}

TEST_CASE("Left recursive with a ZOM content rule", "[left recursive]") {
  parser parser(R"(
        A <- B
        B <- _ A
        _ <- ' '* # Zero or more
    )");

  REQUIRE(!parser);
}

TEST_CASE("Left recursive with empty string test", "[left recursive]") {
  parser parser(" A <- '' A");

  REQUIRE(!parser);
}

TEST_CASE("User defined rule test", "[user rule]") {
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

  REQUIRE(g.parse(" Hello BNF! ") == true);
}

TEST_CASE("Semantic predicate test", "[predicate]") {
  parser parser("NUMBER  <-  [0-9]+");

  parser["NUMBER"] = [](const SemanticValues &vs) {
    auto val = vs.token_to_number<long>();
    if (val != 100) { throw parse_error("value error!!"); }
    return val;
  };

  long val;
  REQUIRE(parser.parse("100", val));
  REQUIRE(val == 100);

  parser.log = [](size_t line, size_t col, const std::string &msg) {
    REQUIRE(line == 1);
    REQUIRE(col == 1);
    REQUIRE(msg == "value error!!");
  };
  REQUIRE(!parser.parse("200", val));
}

TEST_CASE("Japanese character", "[unicode]") {
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
  REQUIRE(ret == true);

  REQUIRE(parser.parse(u8R"(サーバーを復旧します。)"));
}

TEST_CASE("dot with a code", "[unicode]") {
  peg::parser parser(" S <- 'a' . 'b' ");
  REQUIRE(parser.parse(u8R"(aあb)"));
}

TEST_CASE("dot with a char", "[unicode]") {
  peg::parser parser(" S <- 'a' . 'b' ");
  REQUIRE(parser.parse(u8R"(aåb)"));
}

TEST_CASE("character class", "[unicode]") {
  peg::parser parser(R"(
        S <- 'a' [い-おAさC-Eた-とは] 'b'
    )");

  bool ret = parser;
  REQUIRE(ret == true);

  REQUIRE(!parser.parse(u8R"(aあb)"));
  REQUIRE(parser.parse(u8R"(aいb)"));
  REQUIRE(parser.parse(u8R"(aうb)"));
  REQUIRE(parser.parse(u8R"(aおb)"));
  REQUIRE(!parser.parse(u8R"(aかb)"));
  REQUIRE(parser.parse(u8R"(aAb)"));
  REQUIRE(!parser.parse(u8R"(aBb)"));
  REQUIRE(parser.parse(u8R"(aEb)"));
  REQUIRE(!parser.parse(u8R"(aFb)"));
  REQUIRE(!parser.parse(u8R"(aそb)"));
  REQUIRE(parser.parse(u8R"(aたb)"));
  REQUIRE(parser.parse(u8R"(aちb)"));
  REQUIRE(parser.parse(u8R"(aとb)"));
  REQUIRE(!parser.parse(u8R"(aなb)"));
  REQUIRE(parser.parse(u8R"(aはb)"));
  REQUIRE(!parser.parse(u8R"(a?b)"));
}

#if 0 // TODO: Unicode Grapheme support
TEST_CASE("dot with a grapheme", "[unicode]")
{
    peg::parser parser(" S <- 'a' . 'b' ");
    REQUIRE(parser.parse(u8R"(aसिb)"));
}
#endif

TEST_CASE("Macro simple test", "[macro]") {
  parser parser(R"(
		S     <- HELLO WORLD
		HELLO <- T('hello')
		WORLD <- T('world')
		T(a)  <- a [ \t]*
	)");

  REQUIRE(parser.parse("hello \tworld "));
}

TEST_CASE("Macro two parameters", "[macro]") {
  parser parser(R"(
		S           <- HELLO_WORLD
		HELLO_WORLD <- T('hello', 'world')
		T(a, b)     <- a [ \t]* b [ \t]*
	)");

  REQUIRE(parser.parse("hello \tworld "));
}

TEST_CASE("Macro syntax error", "[macro]") {
  parser parser(R"(
		S     <- T('hello')
		T (a) <- a [ \t]*
	)");

  bool ret = parser;
  REQUIRE(ret == false);
}

TEST_CASE("Macro missing argument", "[macro]") {
  parser parser(R"(
		S       <- T ('hello')
		T(a, b) <- a [ \t]* b
	)");

  bool ret = parser;
  REQUIRE(ret == false);
}

TEST_CASE("Macro reference syntax error", "[macro]") {
  parser parser(R"(
		S    <- T ('hello')
		T(a) <- a [ \t]*
	)");

  bool ret = parser;
  REQUIRE(ret == false);
}

TEST_CASE("Macro invalid macro reference error", "[macro]") {
  parser parser(R"(
		S <- T('hello')
		T <- 'world'
	)");

  bool ret = parser;
  REQUIRE(ret == false);
}

TEST_CASE("Macro calculator", "[macro]") {
  // Create a PEG parser
  parser parser(R"(
        # Grammar for simple calculator...
        EXPRESSION       <-  _ LIST(TERM, TERM_OPERATOR)
        TERM             <-  LIST(FACTOR, FACTOR_OPERATOR)
        FACTOR           <-  NUMBER / T('(') EXPRESSION T(')')
        TERM_OPERATOR    <-  T([-+])
        FACTOR_OPERATOR  <-  T([/*])
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
  parser["NUMBER"] = [](const SemanticValues &vs) { return vs.token_to_number<long>(); };

  bool ret = parser;
  REQUIRE(ret == true);

  auto expr = " 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ";
  long val = 0;
  ret = parser.parse(expr, val);

  REQUIRE(ret == true);
  REQUIRE(val == -3);
}

TEST_CASE("Macro expression arguments", "[macro]") {
  parser parser(R"(
		S             <- M('hello' / 'Hello', 'world' / 'World')
		M(arg0, arg1) <- arg0 [ \t]+ arg1
	)");

  REQUIRE(parser.parse("Hello world"));
}

TEST_CASE("Macro recursive", "[macro]") {
  parser parser(R"(
		S    <- M('abc')
		M(s) <- !s / s ' ' M(s / '123') / s
	)");

  REQUIRE(parser.parse(""));
  REQUIRE(parser.parse("abc"));
  REQUIRE(parser.parse("abc abc"));
  REQUIRE(parser.parse("abc 123 abc"));
}

TEST_CASE("Macro recursive2", "[macro]") {
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
    REQUIRE(parser.parse("abc abc-123"));
  }
}

TEST_CASE("Macro exclusive modifiers", "[macro]") {
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

  REQUIRE(parser.parse("public"));
  REQUIRE(parser.parse("static"));
  REQUIRE(parser.parse("final"));
  REQUIRE(parser.parse("public static final"));
  REQUIRE(!parser.parse("public public"));
  REQUIRE(!parser.parse("public static public"));
}

TEST_CASE("Macro token check test", "[macro]") {
  parser parser(R"(
        # Grammar for simple calculator...
        EXPRESSION       <-  _ LIST(TERM, TERM_OPERATOR)
        TERM             <-  LIST(FACTOR, FACTOR_OPERATOR)
        FACTOR           <-  NUMBER / T('(') EXPRESSION T(')')
        TERM_OPERATOR    <-  T([-+])
        FACTOR_OPERATOR  <-  T([/*])
        NUMBER           <-  T([0-9]+)
		~_               <-  [ \t]*
		LIST(I, D)       <-  I (D I)*
		T(S)             <-  < S > _
	)");

  REQUIRE(parser["EXPRESSION"].is_token() == false);
  REQUIRE(parser["TERM"].is_token() == false);
  REQUIRE(parser["FACTOR"].is_token() == false);
  REQUIRE(parser["FACTOR_OPERATOR"].is_token() == true);
  REQUIRE(parser["NUMBER"].is_token() == true);
  REQUIRE(parser["_"].is_token() == true);
  REQUIRE(parser["LIST"].is_token() == false);
  REQUIRE(parser["T"].is_token() == true);
}

TEST_CASE("Macro passes an arg to another macro", "[macro]") {
  parser parser(R"(
        A    <- B(C)
        B(D) <- D
        C    <- 'c'
	)");

  REQUIRE(parser.parse("c"));
}

TEST_CASE("Unreferenced rule", "[macro]") {
  parser parser(R"(
        A    <- B(C)
        B(D) <- D
        C    <- 'c'
        D    <- 'd'
	)");

  bool ret = parser;
  REQUIRE(ret == true); // This is OK, because it's a warning, not an erro...
}

TEST_CASE("Nested macro call", "[macro]") {
  parser parser(R"(
        A    <- B(T)
        B(X) <- C(X)
        C(Y) <- Y
        T    <- 'val'
	)");

  REQUIRE(parser.parse("val"));
}

TEST_CASE("Nested macro call2", "[macro]") {
  parser parser(R"(
        START           <- A('TestVal1', 'TestVal2')+
        A(Aarg1, Aarg2) <- B(Aarg1) '#End'
        B(Barg1)        <- '#' Barg1
	)");

  REQUIRE(parser.parse("#TestVal1#End"));
}

TEST_CASE("Line information test", "[line information]") {
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
  REQUIRE(ret == true);

  ret = parser.parse(" Mon Tue Wed \nThu  Fri  Sat\nSun\n");
  REQUIRE(ret == true);

  REQUIRE(locations[0] == std::make_pair<size_t, size_t>(1, 2));
  REQUIRE(locations[1] == std::make_pair<size_t, size_t>(1, 6));
  REQUIRE(locations[2] == std::make_pair<size_t, size_t>(1, 10));
  REQUIRE(locations[3] == std::make_pair<size_t, size_t>(2, 1));
  REQUIRE(locations[4] == std::make_pair<size_t, size_t>(2, 6));
  REQUIRE(locations[5] == std::make_pair<size_t, size_t>(2, 11));
  REQUIRE(locations[6] == std::make_pair<size_t, size_t>(3, 1));
}

TEST_CASE("Dictionary", "[dic]") {
  parser parser(R"(
        START <- 'This month is ' MONTH '.'
        MONTH <- 'Jan' | 'January' | 'Feb' | 'February'
	)");

  REQUIRE(parser.parse("This month is Jan."));
  REQUIRE(parser.parse("This month is January."));
  REQUIRE_FALSE(parser.parse("This month is Jannuary."));
  REQUIRE_FALSE(parser.parse("This month is ."));
}

TEST_CASE("Dictionary invalid", "[dic]") {
  parser parser(R"(
        START <- 'This month is ' MONTH '.'
        MONTH <- 'Jan' | 'January' | [a-z]+ | 'Feb' | 'February'
	)");

  bool ret = parser;
  REQUIRE_FALSE(ret);
}

TEST_CASE("Error recovery 1", "[error]") {
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

header <- (!__ .)* { message "invalid section header, missing ']'." }
entry  <- (!(__ / HEADER) .)+ { message "invalid entry." }
  )");

  REQUIRE(!!pg); // OK

  std::vector<std::string> errors{
    R"(3:1: invalid entry.)",
    R"(7:1: invalid entry.)",
    R"(10:11: invalid section header, missing ']'.)",
    R"(18:1: invalid entry.)",
  };

  size_t i = 0;
  pg.log = [&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    REQUIRE(ss.str() == errors[i++]);
  };

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  REQUIRE_FALSE(pg.parse(R"([Section 1]
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

  )", ast));

  ast = pg.optimize_ast(ast);

  REQUIRE(ast_to_s(ast) ==
R"(+ START
  + SECTION
    - HEADER/0[CATEGORY] (Section 1)
    + ENTRIES
      + ENTRY/0
        - ONE_WAY/0[WORD] (111)
        - PHRASE/0[WORD] (222)
        - PHRASE/0[WORD] (333)
      + ENTRY/2
      + ENTRY/0
        - ONE_WAY/0[WORD] (ccc)
        - PHRASE/0[WORD] (ddd)
  + SECTION
    - HEADER/0[CATEGORY] (Section 2)
    + ENTRIES
      + ENTRY/2
      + ENTRY/1
        - PHRASE/0[WORD] (fff)
        - PHRASE/0[WORD] (ggg)
  + SECTION
    - HEADER/0[CATEGORY] (Section 3)
    + ENTRIES
      + ENTRY/1
        - PHRASE/0[WORD] (hhh)
        - PHRASE/0[WORD] (iii)
  + SECTION
    - HEADER/0[CATEGORY] (Section 日本語)
    + ENTRIES
      + ENTRY/1
        - PHRASE/0[WORD] (ppp)
        - PHRASE/0[WORD] (qqq)
  + SECTION
    - HEADER/0[CATEGORY] (Section 4)
    + ENTRIES
      + ENTRY/1
        - PHRASE/0[WORD] (jjj)
        - PHRASE/0[WORD] (kkk)
      + ENTRY/2
  + SECTION
    - HEADER/0[CATEGORY] (Section 5)
    + ENTRIES
      + ENTRY/1
        - PHRASE/0[WORD] (rrr)
        - PHRASE/0[WORD] (sss)
)");
}

TEST_CASE("Error recovery 2", "[error]") {
  parser pg(R"(
    START <- ENTRY ((',' ENTRY) / %recover((!(',' / Space) .)+))* (_ / %recover(.*))
    ENTRY <- '[' ITEM (',' ITEM)* ']'
    ITEM  <- WORD / NUM / %recover((!(',' / ']') .)+)
    NUM   <- [0-9]+ ![a-z]
    WORD  <- '"' [a-z]+ '"'

    ~_    <- Space+
    Space <- [ \n]
  )");

  REQUIRE(!!pg); // OK

  std::vector<std::string> errors{
    R"(1:6: syntax error, unexpected ']'.)",
    R"(1:18: syntax error, unexpected 'z', expecting <NUM>.)",
    R"(1:24: syntax error, unexpected ',', expecting <WORD>.)",
    R"(1:31: syntax error, unexpected 'ccc', expecting <NUM>.)",
    R"(1:38: syntax error, unexpected 'ddd', expecting <NUM>.)",
    R"(1:55: syntax error, unexpected ']', expecting <WORD>.)",
    R"(1:58: syntax error, unexpected '\n', expecting <NUM>.)",
    R"(2:3: syntax error.)",
  };

  size_t i = 0;
  pg.log = [&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    REQUIRE(ss.str() == errors[i++]);
  };

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  REQUIRE_FALSE(pg.parse(R"([000]],[111],[222z,"aaa,"bbb",ccc"],[ddd",444,555,"eee],[
  )", ast));

  ast = pg.optimize_ast(ast);

  REQUIRE(ast_to_s(ast) ==
R"(+ START
  - ENTRY/0[NUM] (000)
  - ENTRY/0[NUM] (111)
  + ENTRY
    + ITEM/2
    + ITEM/2
    - ITEM/0[WORD] ("bbb")
    + ITEM/2
  + ENTRY
    + ITEM/2
    - ITEM/1[NUM] (444)
    - ITEM/1[NUM] (555)
    + ITEM/2
)");
}

TEST_CASE("Error recovery 3", "[error]") {
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
duplicate_or     <- skip_puncs { message "Duplicate OR operator (|)" }
missing_or       <- '' { message "Missing OR operator (|)" }
missing_bracket  <- skip_puncs { message "Missing opening/closing square bracket" }
expect_phrase    <- skip { message "Expect phrase" }
invalid_ope_comb <- skip_puncs { message "Use of invalid operator combination" }
invalid_ope      <- skip { message "Use of invalid operator" }
wildcard         <- '' { message "Wildcard characters (%c) should not be used" }
vernacular_char  <- '' { message "Section name %c must be in English" }

skip             <- (!(__) .)*
skip_puncs       <- [|=]* _
  )~");

  REQUIRE(!!pg); // OK

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
  };

  size_t i = 0;
  pg.log = [&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    REQUIRE(ss.str() == errors[i++]);
  };

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  REQUIRE_FALSE(pg.parse(R"([Section 1]
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
  )", ast));

  ast = pg.optimize_ast(ast);

  REQUIRE(ast_to_s(ast) ==
R"(+ START
  + SECTION
    - HEADER/0[CATEGORY] (Section 1)
    + ENTRIES
      + ENTRY/0
        + ONE_WAY/0[PHRASE]
          - WORD (111)
        + PHRASE
          - WORD (222)
        + PHRASE
          - WORD (333)
      + ENTRY/1
        + PHRASE
          - WORD (AAA)
          - WORD (BB*)
        + PHRASE
          - WORD (CCC)
      + ENTRY/1
        + PHRASE
          - WORD (AAA)
          - WORD (B?B)
        + PHRASE
          - WORD (CCC)
      + ENTRY/1
        + PHRASE
          - WORD (aaa)
        + PHRASE
          - WORD (bbb)
      + ENTRY/0
        + ONE_WAY/0[PHRASE]
          - WORD (ccc)
        + PHRASE
          - WORD (ddd)
  + SECTION
    - HEADER/0[CATEGORY] (Section 2)
    + ENTRIES
      + ENTRY/1
        + PHRASE
          - WORD (eee)
      + ENTRY/1
        + PHRASE
          - WORD (fff)
        + PHRASE
          - WORD (ggg)
      + ENTRY/1
        + PHRASE
          - WORD (fff)
        + PHRASE
          - WORD (ggg)
          - WORD (111)
  + SECTION
    - HEADER/0[CATEGORY] (Section 3)
    + ENTRIES
      + ENTRY/1
        + PHRASE
          - WORD (hhh)
        + PHRASE
          - WORD (iii)
  + SECTION
    - HEADER/0[CATEGORY] (Section 日本語です)
    + ENTRIES
      + ENTRY/1
        + PHRASE
          - WORD (ppp)
        + PHRASE
          - WORD (qqq)
  + SECTION
    - HEADER/0[CATEGORY] (Section 4)
    + ENTRIES
      + ENTRY/1
        + PHRASE
          - WORD (jjj)
        + PHRASE
          - WORD (kkk)
      + ENTRY/0
        + ONE_WAY/0[PHRASE]
          - WORD (lll)
        + PHRASE
          - WORD (mmm)
        + PHRASE
          - WORD (nnn)
  + SECTION
    - HEADER/0[CATEGORY] (Section 5)
    + ENTRIES
      + ENTRY/1
        + PHRASE
          - WORD (ppp)
          - WORD (qqq)
        + PHRASE
          - WORD (rrr)
      + ENTRY/1
        + PHRASE
          - WORD (Section)
          - WORD (6)
      + ENTRY/1
        + PHRASE
          - WORD (sss)
        + PHRASE
          - WORD (ttt)
)");
}

TEST_CASE("Error recovery Java", "[error]") {
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
rcblk      ← SkipToRCUR { message "missing end of block." }
semia      ← '' { message "missing simicolon in assignment." }

# Recovery expressions
SkipToRCUR ← (!RCUR (LCUR SkipToRCUR / .))* RCUR
  )");

  REQUIRE(!!pg); // OK

  std::vector<std::string> errors{
    R"(8:5: missing simicolon in assignment.)",
    R"(8:6: missing end of block.)",
  };

  size_t i = 0;
  pg.log = [&](size_t ln, size_t col, const std::string &msg) {
    std::stringstream ss;
    ss << ln << ":" << col << ": " << msg;
    REQUIRE(ss.str() == errors[i++]);
  };

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  REQUIRE_FALSE(pg.parse(R"(public class Example {
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
  )", ast));

  ast = pg.optimize_ast(ast);

  REQUIRE(ast_to_s(ast) ==
R"(+ Prog
  - PUBLIC (public)
  - CLASS (class)
  - NAME (Example)
  - PUBLIC (public)
  - STATIC (static)
  - VOID (void)
  - MAIN (main)
  - STRING (String)
  - NAME (args)
  + BlockStmt
    + Stmt/3[DecStmt]
      - INT (int)
      - NAME (n)
      - Exp/0[NUMBER] (5)
    + Stmt/3[DecStmt]
      - INT (int)
      - NAME (f)
      - Exp/0[NUMBER] (1)
    + Stmt/1[WhileStmt]
      + Exp/0[RelExp]
        - AddExp/0[NUMBER] (0)
        - AddExp/0[NAME] (n)
      + Stmt/5[BlockStmt]
        + Stmt/4[AssignStmt]
          - NAME (f)
          + Exp/0[MulExp]
            - AtomExp/2[NAME] (f)
            - TIMES (*)
            - AtomExp/2[NAME] (n)
        + Stmt/4[AssignStmt]
          - NAME (n)
          + Exp/0[AddExp]
            - MulExp/0[NAME] (n)
            - MINUS (-)
            - MulExp/0[NUMBER] (1)
)");
}
