#include <gtest/gtest.h>
#include <peglib.h>
#include <sstream>

using namespace peg;

// =============================================================================
// Unicode Tests
// =============================================================================

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

// =============================================================================
// Symbol Table Tests
// =============================================================================

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

// =============================================================================
// State Tests
// =============================================================================

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

  EXPECT_EQ(R"(+ Start[I]
  + Block[Statements]
    + Statement[S]
    + Statement[I]
      + Block[Statements]
        + Statement[I]
          + Block[Statements]
            + Statement[S]
            + Statement[S]
        + Statement[S]
    + Statement[S]
)",
            s);
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
  pg.set_logger([&](size_t ln, size_t col, const std::string &msg,
                    const std::string &rule) {
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

// =============================================================================
// Line Information Tests
// =============================================================================

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

// =============================================================================
// Line Info Tests (additional)
// =============================================================================

TEST(LineInfoTest, Multiline_line_info) {
  parser pg(R"(
    S    <- LINE+
    LINE <- < [a-z]+ > '\n'?
  )");
  EXPECT_TRUE(pg);

  std::vector<std::pair<size_t, size_t>> positions;
  pg["LINE"] = [&](const SemanticValues &vs) {
    positions.push_back(vs.line_info());
  };

  EXPECT_TRUE(pg.parse("abc\ndef\nghi"));
  ASSERT_EQ(3, positions.size());
  EXPECT_EQ(1, positions[0].first);
  EXPECT_EQ(1, positions[0].second);
  EXPECT_EQ(2, positions[1].first);
  EXPECT_EQ(1, positions[1].second);
  EXPECT_EQ(3, positions[2].first);
  EXPECT_EQ(1, positions[2].second);
}

TEST(LineInfoTest, Column_info_accuracy) {
  parser pg(R"(
    S     <- _ ITEM+
    ITEM  <- < [a-z]+ > _
    ~_    <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  std::vector<std::pair<size_t, size_t>> positions;
  pg["ITEM"] = [&](const SemanticValues &vs) {
    positions.push_back(vs.line_info());
  };

  EXPECT_TRUE(pg.parse("  abc  def"));
  ASSERT_EQ(2, positions.size());
  EXPECT_EQ(1, positions[0].first);
  EXPECT_EQ(3, positions[0].second); // "abc" starts at column 3
  EXPECT_EQ(1, positions[1].first);
  EXPECT_EQ(8, positions[1].second); // "def" starts at column 8
}

TEST(LineInfoTest, Error_position_accuracy) {
  parser pg(R"(
    S <- 'aaa' '\n' 'bbb' '\n' 'ccc'
  )");
  EXPECT_TRUE(pg);

  size_t err_line = 0, err_col = 0;
  pg.set_logger([&](size_t ln, size_t col, const std::string &) {
    err_line = ln;
    err_col = col;
  });

  EXPECT_FALSE(pg.parse("aaa\nbbb\nXXX"));
  EXPECT_EQ(3, err_line);
  EXPECT_EQ(1, err_col);
}

// =============================================================================
// Ambiguous Grammar Tests

// =============================================================================
// Context Tests
// =============================================================================

TEST(ContextTest, User_data_propagation) {
  parser pg(R"(
    S <- ITEM+
    ITEM <- < [a-z]+ >
    %whitespace <- [ \t]*
  )");
  EXPECT_TRUE(pg);

  pg["ITEM"] = [](const SemanticValues &vs, std::any &dt) {
    auto &count = *std::any_cast<int *>(dt);
    count++;
    return vs.token_to_string();
  };

  int count = 0;
  std::any dt = &count;
  EXPECT_TRUE(pg.parse("hello world foo", dt));
  EXPECT_EQ(3, count);
}

TEST(ContextTest, End_of_input_check) {
  parser pg(R"(S <- 'hello')");
  EXPECT_TRUE(pg);

  // Default: eoi check enabled
  EXPECT_TRUE(pg.parse("hello"));
  EXPECT_FALSE(pg.parse("hello world"));

  // Disable eoi check
  pg.disable_eoi_check();
  EXPECT_TRUE(pg.parse("hello world"));
}

// =============================================================================
// Feature Interaction Tests (combining multiple features)
