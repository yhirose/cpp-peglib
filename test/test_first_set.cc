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
