#include <gtest/gtest.h>
#include <peglib.h>
#include <sstream>

using namespace peg;

// =============================================================================
// Infinite Loop Detection Tests
// =============================================================================

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

// =============================================================================
// Error Handling Tests
// =============================================================================

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
semia      ← '' { error_message "missing semicolon in assignment." }

# Recovery expressions
SkipToRCUR ← (!RCUR (LCUR SkipToRCUR / .))* RCUR
  )");

  EXPECT_TRUE(!!pg);

  std::vector<std::string> errors{
      R"(8:5: missing semicolon in assignment.)",
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

// =============================================================================
// Error Message Quality Tests
// =============================================================================

TEST(ErrorMsgTest, Error_with_expected_literals) {
  parser pg(R"(
    S <- 'hello' ('world' / 'there' / 'friend')
    %whitespace <- [ ]*
  )");
  EXPECT_TRUE(pg);

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });

  EXPECT_FALSE(pg.parse("hello xyz"));
  // Error should mention expected alternatives
  EXPECT_FALSE(error_msg.empty());
}

TEST(ErrorMsgTest, Error_at_correct_position) {
  parser pg(R"(
    S <- 'abc' 'def' 'ghi'
  )");
  EXPECT_TRUE(pg);

  size_t err_col = 0;
  pg.set_logger(
      [&](size_t, size_t col, const std::string &) { err_col = col; });

  EXPECT_FALSE(pg.parse("abcdefXXX"));
  EXPECT_EQ(7, err_col); // Error at position of 'XXX'
}

TEST(ErrorMsgTest, Error_with_dictionary) {
  parser pg(R"(
    S <- 'select' KEYWORD
    KEYWORD <- 'from' | 'where' | 'join'
    %whitespace <- [ ]*
  )");
  EXPECT_TRUE(pg);

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });

  EXPECT_FALSE(pg.parse("select xyz"));
  EXPECT_FALSE(error_msg.empty());
}

// =============================================================================
// Whitespace Edge Cases

// =============================================================================
// Cut Operator Tests
// =============================================================================

TEST(CutTest, Basic_backtrack_prevention) {
  parser pg(R"(
    S <- A / B
    A <- 'a' ↑ 'b'
    B <- 'a' 'c'
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("ab"));
  // After 'a' matches in A and cut fires, B should NOT be tried
  EXPECT_FALSE(pg.parse("ac"));
}

TEST(CutTest, Without_cut_allows_backtrack) {
  parser pg(R"(
    S <- A / B
    A <- 'a' 'b'
    B <- 'a' 'c'
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("ab"));
  // Without cut, backtrack to B works
  EXPECT_TRUE(pg.parse("ac"));
}

TEST(CutTest, Cut_in_sequence_with_multiple_alternatives) {
  parser pg(R"(
    S <- A / B / C
    A <- 'x' ↑ 'a'
    B <- 'x' 'b'
    C <- 'y' 'c'
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("xa"));
  // Cut in A prevents trying B
  EXPECT_FALSE(pg.parse("xb"));
  // C is unrelated branch, should still work
  EXPECT_TRUE(pg.parse("yc"));
}

TEST(CutTest, Cut_in_nested_rules) {
  parser pg(R"(
    S    <- STMT*
    STMT <- IF / ASSIGN
    IF   <- 'if' ↑ EXPR 'then' EXPR
    ASSIGN <- NAME '=' EXPR
    EXPR <- NAME / NUM
    NAME <- < [a-z]+ >
    NUM  <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("if x then 1"));
  EXPECT_TRUE(pg.parse("x = 1"));
  // 'if' matched, cut fires, then expects EXPR which fails
  EXPECT_FALSE(pg.parse("if"));
}

TEST(CutTest, Cut_does_not_affect_outer_choice) {
  // Cut only affects the immediate choice level
  parser pg(R"(
    S <- INNER / 'fallback'
    INNER <- A / B
    A <- 'a' ↑ 'x'
    B <- 'a' 'y'
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("ax"));
  // Cut in INNER prevents B, but INNER fails, so outer 'fallback' can be tried
  EXPECT_TRUE(pg.parse("fallback"));
}

TEST(CutTest, Cut_with_error_message) {
  parser pg(R"(
    S <- 'begin' ↑ BODY 'end'
    BODY <- < [a-z]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });

  EXPECT_TRUE(pg.parse("begin hello end"));
  EXPECT_FALSE(pg.parse("begin"));
  EXPECT_FALSE(error_msg.empty());
}

TEST(CutTest, Cut_with_repetition) {
  parser pg(R"(
    S    <- ITEM+
    ITEM <- 'a' ↑ 'b' / 'c' 'd'
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("ab ab"));
  EXPECT_TRUE(pg.parse("cd"));
  // First ITEM matches 'ab', second starts 'a' → cut → fails on 'c'
  EXPECT_FALSE(pg.parse("ab ac"));
}

// =============================================================================
// Recovery Operator Tests
// =============================================================================

// =============================================================================
// Recovery Operator Tests
// =============================================================================

TEST(RecoveryTest, Basic_recovery_with_named_error) {
  parser pg(R"(
    S    <- 'a' ITEM^err 'c'
    ITEM <- 'b'
    err  <- '' { error_message "expected 'b'" }
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("abc"));

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });
  EXPECT_FALSE(pg.parse("axc"));
  EXPECT_EQ("expected 'b'", error_msg);
}

TEST(RecoveryTest, Recovery_in_list_pattern) {
  parser pg(R"(
    S     <- ITEM (',' ITEM)*
    ITEM  <- NUM / %recover((!(',' / !.) .)+)
    NUM   <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("1, 2, 3"));

  size_t error_count = 0;
  pg.set_logger([&](size_t, size_t, const std::string &) { error_count++; });
  // 'abc' triggers recovery, then continues parsing
  EXPECT_FALSE(pg.parse("1, abc, 3"));
  EXPECT_GT(error_count, 0);
}

TEST(RecoveryTest, Multiple_recovery_points) {
  parser pg(R"(
    PROGRAM <- STMT*
    STMT    <- ASSIGN / %recover((!'\n' .)* '\n')
    ASSIGN  <- NAME '=' NUM ';'
    NAME    <- < [a-z]+ >
    NUM     <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  size_t error_count = 0;
  pg.set_logger([&](size_t, size_t, const std::string &) { error_count++; });
  EXPECT_FALSE(pg.parse("x = 1;\nbad line\ny = 2;\n"));
  EXPECT_GT(error_count, 0);
}

TEST(RecoveryTest, Recovery_with_AST) {
  parser pg(R"(
    S    <- ITEM+
    ITEM <- NUM / %recover([^0-9 \t\r\n]+)
    NUM  <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  pg.enable_ast();

  size_t error_count = 0;
  pg.set_logger([&](size_t, size_t, const std::string &) { error_count++; });

  std::shared_ptr<Ast> ast;
  EXPECT_FALSE(pg.parse("123 abc 456", ast));
  EXPECT_GT(error_count, 0);
}

// =============================================================================
// Enter/Leave Handler Tests
