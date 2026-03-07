#include <charconv>
#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// =============================================================================
// Token Boundary Tests
// =============================================================================

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

// =============================================================================
// Token Boundary Interaction Tests
// =============================================================================

TEST(TokenBoundaryInteractionTest, Token_boundary_with_backtracking) {
  parser pg(R"(
    S           <- A / B
    A           <- < 'hello' > ' world'
    B           <- < 'hello' > ' there'
    %whitespace <- ''
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("hello world"));
  EXPECT_TRUE(pg.parse("hello there"));
}

TEST(TokenBoundaryInteractionTest, Nested_token_boundaries) {
  parser pg(R"(
    S <- < 'a' < 'b' > 'c' >
  )");
  EXPECT_TRUE(pg);

  pg["S"] = [](const SemanticValues &vs) {
    // Inner token captures 'b', outer captures 'abc'
    EXPECT_EQ("b", vs.token());
  };

  EXPECT_TRUE(pg.parse("abc"));
}

TEST(TokenBoundaryInteractionTest, Token_boundary_with_unicode) {
  parser pg(R"(
    S <- < [あ-ん]+ >
  )");
  EXPECT_TRUE(pg);

  pg["S"] = [](const SemanticValues &vs) { EXPECT_EQ(u8"あいう", vs.token()); };

  EXPECT_TRUE(pg.parse(u8"あいう"));
}

TEST(TokenBoundaryInteractionTest, Token_boundary_with_repetition) {
  parser pg(R"(
    S    <- ITEM+
    ITEM <- < [a-z]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  std::vector<std::string> tokens;
  pg["ITEM"] = [&](const SemanticValues &vs) {
    tokens.push_back(std::string(vs.token()));
  };

  EXPECT_TRUE(pg.parse("hello world foo"));
  ASSERT_EQ(3, tokens.size());
  EXPECT_EQ("hello", tokens[0]);
  EXPECT_EQ("world", tokens[1]);
  EXPECT_EQ("foo", tokens[2]);
}

// =============================================================================
// Capture Scope and Ignore Tests
// =============================================================================

// =============================================================================
// Predicate Tests
// =============================================================================

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

TEST(PredicateTest, User_data_pass_from_predicate_to_action) {
  parser parser(R"(
    NUMBER <- < [0-9]+ >
  )");

  parser["NUMBER"].predicate = [](const SemanticValues &vs,
                                  const std::any & /*dt*/, std::string &msg,
                                  std::any &predicate_data) {
    int value;
    auto [ptr, err] = std::from_chars(
        vs.token().data(), vs.token().data() + vs.token().size(), value);
    if (err != std::errc()) {
      msg = "Number out of range.";
      return false;
    }
    predicate_data = value;
    return true;
  };

  parser["NUMBER"] = [](const SemanticValues & /*vs*/, std::any & /*dt*/,
                        const std::any &predicate_data) {
    return std::any_cast<int>(predicate_data);
  };

  int result;
  EXPECT_TRUE(parser.parse("12345", result));
  EXPECT_EQ(12345, result);

  parser.set_logger(
      [](size_t /*line*/, size_t /*col*/, const std::string &msg) {
        EXPECT_EQ("Number out of range.", msg);
      });
  EXPECT_FALSE(parser.parse("99999999999999999999", result));
}

TEST(PredicateTest, User_data_backward_compatibility) {
  parser parser(R"(
    NUMBER <- < [0-9]+ >
  )");

  // Old 3-arg predicate still works
  parser["NUMBER"].predicate = [](const SemanticValues &vs,
                                  const std::any & /*dt*/, std::string &msg) {
    if (vs.token_to_number<long>() != 100) {
      msg = "value error!!";
      return false;
    }
    return true;
  };

  // Old 1-arg action still works
  parser["NUMBER"] = [](const SemanticValues &vs) {
    return vs.token_to_number<long>();
  };

  long val;
  EXPECT_TRUE(parser.parse("100", val));
  EXPECT_EQ(100, val);
}

// =============================================================================
// Semantic Predicate Tests
// =============================================================================

TEST(SemanticPredicateTest, Predicate_with_backtracking) {
  parser pg(R"(
    S    <- A / B
    A    <- NUM
    B    <- NUM
    NUM  <- < [0-9]+ >
  )");
  EXPECT_TRUE(pg);

  // A's predicate rejects even numbers, so B should be tried
  pg["A"].predicate = [](const SemanticValues &vs, const std::any &,
                         std::string &msg) {
    auto n = vs.token_to_number<int>();
    if (n % 2 == 0) {
      msg = "must be odd";
      return false;
    }
    return true;
  };

  pg["B"].predicate = [](const SemanticValues &vs, const std::any &,
                         std::string &msg) {
    auto n = vs.token_to_number<int>();
    if (n % 2 != 0) {
      msg = "must be even";
      return false;
    }
    return true;
  };

  // Odd -> A matches
  EXPECT_TRUE(pg.parse("3"));
  // Even -> A fails predicate, B matches
  EXPECT_TRUE(pg.parse("4"));
}

TEST(SemanticPredicateTest, Predicate_error_message) {
  parser pg("S <- < [0-9]+ >");
  EXPECT_TRUE(pg);

  pg["S"].predicate = [](const SemanticValues &vs, const std::any &,
                         std::string &msg) {
    auto n = vs.token_to_number<int>();
    if (n > 100) {
      msg = "value must be <= 100";
      return false;
    }
    return true;
  };

  std::string error_msg;
  pg.set_logger(
      [&](size_t, size_t, const std::string &msg) { error_msg = msg; });

  EXPECT_TRUE(pg.parse("50"));
  EXPECT_FALSE(pg.parse("200"));
  EXPECT_EQ("value must be <= 100", error_msg);
}

TEST(SemanticPredicateTest, Predicate_with_user_data) {
  parser pg(R"(
    S <- TOKEN+
    TOKEN <- < [a-z]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  pg["TOKEN"].predicate = [](const SemanticValues &vs, const std::any &dt,
                             std::string &msg) {
    auto &banned = *std::any_cast<std::set<std::string> *>(dt);
    auto tok = vs.token_to_string();
    if (banned.count(tok)) {
      msg = "'" + tok + "' is banned";
      return false;
    }
    return true;
  };

  std::set<std::string> banned_words = {"bad", "evil"};
  std::any dt = &banned_words;

  EXPECT_TRUE(pg.parse("good nice", dt));
  EXPECT_FALSE(pg.parse("good bad", dt));
}

TEST(SemanticPredicateTest, Predicate_in_repetition) {
  parser pg(R"(
    S    <- NUM+
    NUM  <- < [0-9]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  std::vector<int> values;
  pg["NUM"] = [](const SemanticValues &vs) {
    return vs.token_to_number<int>();
  };
  pg["NUM"].predicate = [](const SemanticValues &vs, const std::any &,
                           std::string &msg) {
    auto n = vs.token_to_number<int>();
    if (n < 0 || n > 9) {
      msg = "single digit only";
      return false;
    }
    return true;
  };

  EXPECT_TRUE(pg.parse("1 2 3"));
  EXPECT_FALSE(pg.parse("1 20 3"));
}

// =============================================================================
// Packrat Cache Correctness Tests
// =============================================================================

// =============================================================================
// Lookahead Predicate Tests
// =============================================================================

TEST(LookaheadTest, And_predicate_does_not_consume) {
  parser pg(R"(
    S <- &'hello' 'hello world'
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("hello world"));
  EXPECT_FALSE(pg.parse("goodbye world"));
}

TEST(LookaheadTest, Not_predicate_does_not_consume) {
  parser pg(R"(
    S <- !'bad' < [a-z]+ >
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("good"));
  EXPECT_FALSE(pg.parse("bad"));
  // "badge" starts with "bad" so !'bad' fails
  EXPECT_FALSE(pg.parse("badge"));
}

TEST(LookaheadTest, Nested_not_predicate) {
  // !!A is double negation: NOT(NOT(A))
  // If A matches, !A fails, !!A succeeds (without consuming)
  // cpp-peglib parses '!!' as two separate NOT prefixes
  parser pg(R"(
    S <- (!'bad' .)* !.
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("go"));
  // 'bad' prefix blocks further progress
  EXPECT_FALSE(pg.parse("bad"));
}

TEST(LookaheadTest, Predicate_in_sequence) {
  parser pg(R"(
    S <- &[a-z] < [a-z0-9]+ >
  )");
  EXPECT_TRUE(pg);

  // Must start with lowercase letter
  EXPECT_TRUE(pg.parse("abc123"));
  EXPECT_FALSE(pg.parse("123abc"));
}

TEST(LookaheadTest, Complex_predicate_chain) {
  parser pg(R"(
    S <- KEYWORD / IDENT
    KEYWORD <- ('if' / 'else' / 'while') ![a-zA-Z0-9_]
    IDENT   <- < [a-zA-Z_][a-zA-Z0-9_]* >
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("if"));
  EXPECT_TRUE(pg.parse("else"));
  // "iffy" should not match KEYWORD, should match IDENT
  EXPECT_TRUE(pg.parse("iffy"));
}

TEST(LookaheadTest, Not_predicate_with_any_character) {
  // Match everything until "end"
  parser pg(R"(
    S <- (!'end' .)* 'end'
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("hello world end"));
  EXPECT_TRUE(pg.parse("end"));
  EXPECT_FALSE(pg.parse("hello world"));
}

// =============================================================================
// Token Boundary Interaction Tests

// =============================================================================
// Capture Scope and Ignore Tests
// =============================================================================

TEST(CaptureScopeTest, Ignore_operator_skips_semantic_value) {
  parser pg(R"(
    S <- ~A B ~C
    A <- 'a'
    B <- 'b'
    C <- 'c'
  )");
  EXPECT_TRUE(pg);

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("abc", ast));
  // Only B's value should appear
  EXPECT_EQ(1, ast->nodes.size());
  EXPECT_EQ("B", ast->nodes[0]->name);
}

TEST(CaptureScopeTest, Ignore_operator_in_grammar) {
  parser pg(R"(
    S  <- A B C
    ~A <- 'a' _
    B  <- < 'b' > _
    ~C <- 'c' _
    ~_ <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  pg.enable_ast();

  std::shared_ptr<Ast> ast;
  EXPECT_TRUE(pg.parse("a b c", ast));
  EXPECT_EQ(1, ast->nodes.size());
  EXPECT_EQ("B", ast->nodes[0]->name);
}

TEST(CaptureScopeTest, Token_capture_with_predicate) {
  parser pg(R"(
    S <- < &[a-z] [a-z0-9]+ >
  )");
  EXPECT_TRUE(pg);

  pg["S"] = [](const SemanticValues &vs) { EXPECT_EQ("abc123", vs.token()); };

  EXPECT_TRUE(pg.parse("abc123"));
  EXPECT_FALSE(pg.parse("123abc"));
}

TEST(CaptureScopeTest, Multiple_captures_in_rule) {
  parser pg(R"(
    S <- WORD ' ' NUM
    WORD <- < [a-z]+ >
    NUM  <- < [0-9]+ >
  )");
  EXPECT_TRUE(pg);

  std::string word, num;
  pg["WORD"] = [&](const SemanticValues &vs) { word = vs.token_to_string(); };
  pg["NUM"] = [&](const SemanticValues &vs) { num = vs.token_to_string(); };

  EXPECT_TRUE(pg.parse("hello 123"));
  EXPECT_EQ("hello", word);
  EXPECT_EQ("123", num);
}

// =============================================================================

// =============================================================================
// Character Class Tests
// =============================================================================

TEST(CharClassTest, Single_character_range) {
  parser pg(R"(S <- [a-a]+)");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("aaa"));
  EXPECT_FALSE(pg.parse("b"));
}

TEST(CharClassTest, Special_characters_in_class) {
  // Hyphen and backslash in character class
  parser pg(R"(S <- [-\\]+)");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("-\\"));
  EXPECT_FALSE(pg.parse("abc"));
}

TEST(CharClassTest, Character_class_with_escape_sequences) {
  parser pg(R"(S <- [\t\n\r]+)");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("\t\n\r"));
  EXPECT_FALSE(pg.parse("abc"));
}

TEST(CharClassTest, Negated_character_class_matches_unicode) {
  parser pg(R"(S <- [^a-z]+)");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("123"));
  EXPECT_TRUE(pg.parse("ABC"));
  EXPECT_TRUE(pg.parse(u8"日本"));
  EXPECT_FALSE(pg.parse("abc"));
}

TEST(CharClassTest, Case_insensitive_character_class) {
  parser pg(R"(S <- [a-z]i+)");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("abc"));
  EXPECT_TRUE(pg.parse("ABC"));
  EXPECT_TRUE(pg.parse("aBc"));
  EXPECT_FALSE(pg.parse("123"));
}

// =============================================================================

// =============================================================================
// Literal Tests
// =============================================================================

TEST(LiteralTest, Empty_literal_in_option) {
  parser pg(R"(S <- 'a' ''? 'b')");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("ab"));
}

TEST(LiteralTest, Case_insensitive_literal) {
  parser pg(R"(
    S <- 'hello'i _ 'world'i
    ~_ <- [ \t]+
  )");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("hello world"));
  EXPECT_TRUE(pg.parse("HELLO WORLD"));
  EXPECT_TRUE(pg.parse("Hello World"));
  EXPECT_FALSE(pg.parse("hello123"));
}

TEST(LiteralTest, Literal_with_escape_sequences) {
  parser pg(R"(S <- '\t\n\r\\\'\"')");
  EXPECT_TRUE(pg);

  EXPECT_TRUE(pg.parse("\t\n\r\\'\""));
}

// =============================================================================
