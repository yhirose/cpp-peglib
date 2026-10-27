#include <cstring>
#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

// Parse `input` with both the original grammar and a serialize/deserialize
// round-trip of it, and require identical accept/length.
static void expect_same(const Grammar &orig, const Grammar &deser,
                        const std::string &start, const char *input) {
  std::any d1, d2;
  auto r1 = orig.at(start).parse(input, std::strlen(input), d1);
  auto r2 = deser.at(start).parse(input, std::strlen(input), d2);
  EXPECT_EQ(r1.ret, r2.ret) << "input=" << input;
  EXPECT_EQ(r1.len, r2.len) << "input=" << input;
}

// Parse each input with both parsers, building ASTs, and require the same
// result and, where both accept, the same tree.
static void expect_same_asts(parser &p1, parser &p2,
                             std::initializer_list<const char *> inputs) {
  p1.enable_ast();
  p2.enable_ast();
  for (auto in : inputs) {
    std::shared_ptr<Ast> a1, a2;
    auto r1 = p1.parse(in, a1), r2 = p2.parse(in, a2);
    EXPECT_EQ(r1, r2) << in;
    if (r1 && r2) { EXPECT_EQ(ast_to_s(a1), ast_to_s(a2)) << in; }
  }
}

static std::shared_ptr<Grammar> load(const char *text, std::string &start) {
  Rules rules;
  auto ctx =
      ParserGenerator::parse(text, std::strlen(text), rules, nullptr, "", true);
  start = ctx.start;
  return ctx.grammar;
}

// Round-trip a grammar through a blob and require the deserialized grammar to
// accept/reject every input exactly like the original.
static void check_rt(const char *grammar,
                     std::initializer_list<const char *> inputs) {
  std::string start;
  auto orig = load(grammar, start);
  ASSERT_TRUE(orig != nullptr) << grammar;
  auto blob = GrammarBlob::serialize(*orig, start);
  std::string start2;
  auto deser = GrammarBlob::deserialize(blob, start2);
  EXPECT_EQ(start, start2);
  EXPECT_EQ(orig->size(), deser->size());
  for (auto in : inputs)
    expect_same(*orig, *deser, start, in);
}

TEST(GrammarBlobTest, RoundTripBasic) {
  std::string start;
  auto orig = load("S <- 'a' Digit* 'c'\nDigit <- [0-9]", start);
  ASSERT_TRUE(orig != nullptr);

  auto blob = GrammarBlob::serialize(*orig, start);
  std::string start2;
  auto deser = GrammarBlob::deserialize(blob, start2);
  EXPECT_EQ(start, start2);
  EXPECT_EQ(orig->size(), deser->size());

  for (auto in : {"ac", "a1c", "a123c", "ab", "axc", "a1", ""}) {
    expect_same(*orig, *deser, start, in);
  }
}

TEST(GrammarBlobTest, RoundTripPredefinedClasses) {
  check_rt("S <- [\\d]+ '-' [[:alpha:]]+ '-' [^\\s]+",
           {"12-ab-x!", "a2-ab-x", "12-a1-x", "12-ab-x y", "12-ab-"});
}

TEST(GrammarBlobTest, RoundTripNoWhitespace) {
  check_rt("S <- 'x' T 'y'\n"
           "T <- '\"' (!'\"' .)* '\"' { no_whitespace }\n"
           "%whitespace <- [ \\t]*",
           {"x \" a b \" y", "x \"ab\" y", "x \" a b \"y", "x\"\"y", "xy"});
}

TEST(GrammarBlobTest, RoundTripWhitespaceAndDictionary) {
  const char *g = R"(
    Program     <- Word+
    Word        <- Keyword / Name
    Keyword     <- 'if' | 'else' | 'while'
    Name        <- < [a-z]+ >
    %whitespace <- [ \t\r\n]*
  )";
  std::string start;
  auto orig = load(g, start);
  ASSERT_TRUE(orig != nullptr);
  auto blob = GrammarBlob::serialize(*orig, start);
  std::string start2;
  auto deser = GrammarBlob::deserialize(blob, start2);
  for (auto in : {"if else", "while foo bar", "  if  ", "abc", "if2"}) {
    expect_same(*orig, *deser, start, in);
  }
}

TEST(GrammarBlobTest, RoundTripMacro) {
  const char *g = "S <- List('a', ',')\nList(I, D) <- (I (D I)*)?";
  std::string start;
  auto orig = load(g, start);
  ASSERT_TRUE(orig != nullptr);
  auto blob = GrammarBlob::serialize(*orig, start);
  std::string start2;
  auto deser = GrammarBlob::deserialize(blob, start2);
  for (auto in : {"a", "a,a", "a,a,a", "", "a,", "b"}) {
    expect_same(*orig, *deser, start, in);
  }
}

// A 'precedence' instruction lowers a rule to a PrecedenceClimbing operator
// whose operator table (levels + associativity) is structural, so it survives a
// blob round-trip. Compare the parsed AST to prove operator precedence and
// associativity are reproduced, not just accept/reject parity.
TEST(GrammarBlobTest, RoundTripPrecedence) {
  const char *g = R"(
    EXPRESSION <- ATOM (OPERATOR ATOM)* {
      precedence
        L + -
        L * /
    }
    ATOM       <- NUMBER / '(' EXPRESSION ')'
    OPERATOR   <- < [-+*/] >
    NUMBER     <- < [0-9]+ >
    %whitespace <- [ \t]*
  )";
  peg::parser p1;
  ASSERT_TRUE(p1.load_grammar(g));
  auto blob = p1.serialize_grammar();

  peg::parser p2;
  ASSERT_TRUE(p2.load_blob(blob));
  expect_same_asts(p1, p2,
                   {"1", "1 + 2 * 3", "1 * 2 + 3", "1 - 2 - 3", "2 * 3 * 4",
                    "(1 + 2) * 3", "1 + 2 * 3 - 4 / 2", ""});
}

// Macro-based precedence: the binary operator is a macro parameter, so
// get_reference_for_binop() takes its is_macro path and resolves the operator
// through the macro arguments. Exercises that the deserialized rule_ reference
// reports is_macro correctly.
TEST(GrammarBlobTest, RoundTripPrecedenceMacro) {
  const char *g = "E <- Infix(P, O)\nInfix(A, B) <- A (B A)* {\n  precedence\n "
                  "   L '+' '-'\n}\nP <- [0-9]\nO <- < [-+] >";
  check_rt(g, {"1", "1+2", "1+2-3", "1-2+3", "", "1+", "+1"});
}

// The %whitespace rule may reference other rules (here a comment rule). Its
// operator is shared with the %whitespace definition and must be re-linked on
// load; otherwise the reference inside it dereferences a null rule and crashes.
// Combined with a macro precedence rule this mirrors grammar/monkey.peg.
TEST(GrammarBlobTest, RoundTripWhitespaceWithReference) {
  const char *g = R"(
    EXPRESSION      <- INFIX(PRIMARY, OPE)
    INFIX(ATOM, OP) <- ATOM (OP ATOM)* {
                         precedence
                           L + -
                           L * /
                       }
    PRIMARY         <- < [0-9]+ >
    OPE             <- < [-+*/] >
    COMMENT         <- '#' (!LINE_END .)* &LINE_END
    LINE_END        <- '\n' / !.
    %whitespace     <- ([ \t\r\n]+ / COMMENT)*
  )";
  check_rt(g, {"1 + 2 * 3", "1 # c\n + 2", "1*2*3", "  1  ", "1 +", ""});
}

TEST(GrammarBlobTest, RejectsBadMagic) {
  std::vector<uint8_t> junk = {0, 1, 2, 3, 4, 5, 6, 7};
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(junk, start), std::runtime_error);
}

// Blobs of earlier versions are not read: v1.20.0 stored `[[:^lower:]]i` as
// ranges that now match every character, and v1.22.0 a recovery as a call of
// a built-in `%recover` macro. The version was raised again because readers
// of v1.28.0, which do not check flag bits, would read a body where a rule
// given on load has none; only the current version is read.
TEST(GrammarBlobTest, RejectsBlobsOfEarlierVersions) {
  struct Case {
    const char *grammar;
    uint8_t magic[4];
  };
  for (const auto &[grammar, magic] : std::initializer_list<Case>{
           {"S <- [[:^lower:]]i", {0x32, 0x47, 0x45, 0x50}},
           {"S <- 'a' 'b'^skip\nskip <- (!'b' .)*", {0x33, 0x47, 0x45, 0x50}},
           {"S <- 'a'", {0x34, 0x47, 0x45, 0x50}},
       }) {
    parser pg(grammar);
    auto blob = pg.serialize_grammar();
    std::copy(std::begin(magic), std::end(magic), blob.begin());

    parser restored;
    EXPECT_FALSE(restored.load_blob(blob)) << grammar;
  }
}

// parser-level API: serialize_grammar() -> load_blob(), then parse with AST and
// compare against a freshly loaded grammar.
TEST(GrammarBlobTest, ParserLoadBlobWithAst) {
  const char *g = R"(
    Expr   <- Sum
    Sum    <- Product (('+' / '-') Product)*
    Product<- Value (('*' / '/') Value)*
    Value  <- < [0-9]+ > / '(' Expr ')'
    %whitespace <- [ \t]*
  )";
  peg::parser p1;
  ASSERT_TRUE(p1.load_grammar(g));
  auto blob = p1.serialize_grammar();

  peg::parser p2;
  ASSERT_TRUE(p2.load_blob(blob));
  expect_same_asts(p1, p2,
                   {"1 + 2 * 3", "(1 + 2) * 3", "10 - 2 - 3", "", "1 +"});
}

// Regression: enable_packrat_parsing() after load_blob() must keep packrat on
// when the blob has it baked in. Previously load_blob left the parser-level
// flag at its false default, so enable_packrat_parsing() silently disabled
// packrat on the start rule after a blob round-trip.
TEST(GrammarBlobTest, LoadBlobPreservesPackrat) {
  const char *g = R"(
    Expr   <- Sum
    Sum    <- Product (('+' / '-') Product)*
    Product<- Value (('*' / '/') Value)*
    Value  <- < [0-9]+ > / '(' Expr ')'
    %whitespace <- [ \t]*
  )";
  peg::parser p1;
  ASSERT_TRUE(p1.load_grammar(g));
  p1.enable_packrat_parsing();
  auto blob = p1.serialize_grammar(); // start rule has packrat baked in

  std::string start1;
  auto g1 = GrammarBlob::deserialize(blob, start1);
  ASSERT_TRUE((*g1)[start1].enablePackratParsing);

  peg::parser p2;
  ASSERT_TRUE(p2.load_blob(blob));
  p2.enable_packrat_parsing(); // must NOT clear the blob's packrat flag
  auto blob2 = p2.serialize_grammar();

  std::string start2;
  auto g2 = GrammarBlob::deserialize(blob2, start2);
  EXPECT_TRUE((*g2)[start2].enablePackratParsing);
}

// A blob made without packrat must still let the loaded parser turn it on.
TEST(GrammarBlobTest, LoadBlobAllowsEnablingPackrat) {
  peg::parser p1(R"(
    START <- PAT1 / PAT2
    PAT1  <- HELLO ' One'
    PAT2  <- HELLO ' Two'
    HELLO <- 'Hello'
  )");
  ASSERT_TRUE(!!p1);
  auto blob = p1.serialize_grammar();

  peg::parser p2;
  ASSERT_TRUE(p2.load_blob(blob));
  size_t count = 0;
  p2["HELLO"] = [&](const peg::SemanticValues &) { count++; };
  p2.enable_packrat_parsing();

  EXPECT_TRUE(p2.parse("Hello Two"));
  EXPECT_EQ(1, count); // PAT2 reads HELLO from the cache
}

TEST(GrammarBlobTest, LoadBlobRejectsGarbage) {
  peg::parser p;
  std::vector<uint8_t> junk = {9, 9, 9, 9};
  EXPECT_FALSE(p.load_blob(junk));
}

// A blob whose start rule is `start` and which defines the one rule `S`, with
// `flags`, `params`, `flags2` and the body `body` writes.
static std::vector<uint8_t>
blob_of(const std::string &start,
        const std::function<void(GrammarBlob::Writer &)> &body,
        uint8_t flags = GrammarBlob::F_EoiCheck,
        const std::vector<std::string> &params = {}, uint8_t flags2 = 0) {
  GrammarBlob::Writer w;
  w.u32(GrammarBlob::MAGIC);
  w.str(start);
  w.u32(1);
  w.str("S");
  w.u8(flags);
  w.u8(flags2);
  w.u32(static_cast<uint32_t>(params.size()));
  for (const auto &param : params) {
    w.str(param);
  }
  w.str(""); // ast_name
  w.str(""); // error_message
  body(w);
  return std::move(w.b);
}

static void write_literal_a(GrammarBlob::Writer &w) {
  w.u8(GrammarBlob::T_Literal);
  w.u8(0);
  w.str("a");
}

TEST(GrammarBlobTest, HandMadeBlobIsRead) {
  std::string start;
  auto g = GrammarBlob::deserialize(blob_of("S", write_literal_a), start);
  EXPECT_EQ("S", start);
  EXPECT_TRUE((*g)["S"].parse("a").ret);
}

TEST(GrammarBlobTest, RejectsStringPastTheEnd) {
  GrammarBlob::Writer w;
  w.u32(GrammarBlob::MAGIC);
  w.u32(0x0fffffff); // the start rule's name runs past the blob
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(w.b, start), std::runtime_error);
}

TEST(GrammarBlobTest, RejectsOperatorsNestedTooDeeply) {
  auto blob = blob_of("S", [](GrammarBlob::Writer &w) {
    for (auto i = 0; i < 100000; i++) {
      w.u8(GrammarBlob::T_Not);
    }
    w.u8(GrammarBlob::T_AnyChar);
  });
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);
}

TEST(GrammarBlobTest, RejectsTrailingBytes) {
  auto blob = blob_of("S", write_literal_a);
  blob.push_back(0);
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);
}

TEST(GrammarBlobTest, RejectsUndefinedStartRule) {
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob_of("T", write_literal_a), start),
               std::runtime_error);
}

TEST(GrammarBlobTest, RejectsUndefinedReference) {
  auto blob = blob_of("S", [](GrammarBlob::Writer &w) {
    w.u8(GrammarBlob::T_Reference);
    w.u8(0); // not a macro call
    w.str("Undefined");
    w.u32(0); // arguments
  });
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);
}

// operator[] makes a rule for a name the grammar does not define, here by a
// typo. Such a rule has no body to write.
TEST(GrammarBlobTest, RuleWithoutABodyIsLeftOut) {
  peg::parser p("S <- 'a'");
  ASSERT_TRUE(p);
  p["Typo"] = [](const SemanticValues &) { return 1; };

  auto blob = p.serialize_grammar();
  std::string start;
  EXPECT_FALSE(GrammarBlob::deserialize(blob, start)->count("Typo"));
  peg::parser q;
  ASSERT_TRUE(q.load_blob(blob));
  EXPECT_TRUE(q.parse("a"));
}

// Whether a rule can match empty is worked out again, not read from the blob.
// One saying that E cannot would have P skipped where P cannot start, and E's
// action not run on its empty match.
TEST(GrammarBlobTest, WhetherARuleCanMatchEmptyIsWorkedOutAgain) {
  peg::parser p("S <- P? 'q'\nP <- E 'p'\nE <- 'e'?");
  ASSERT_TRUE(p);
  p["E"].can_be_empty = false; // as a blob with F_CanBeEmpty cleared says
  auto blob = p.serialize_grammar();

  peg::parser q;
  ASSERT_TRUE(q.load_blob(blob));
  EXPECT_TRUE(q["E"].can_be_empty);
  auto runs = 0;
  q["E"] = [&](const SemanticValues &) { runs++; };
  EXPECT_TRUE(q.parse("q"));
  EXPECT_EQ(1, runs);
}

TEST(GrammarBlobTest, RejectsAMissingOperator) {
  auto blob = blob_of("S", [](GrammarBlob::Writer &w) {
    w.u8(GrammarBlob::T_Sequence);
    w.u32(2);
    write_literal_a(w);
    w.u8(20); // what marked a missing operator
  });
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);
}

TEST(GrammarBlobTest, RejectsAClassWithoutARange) {
  auto blob = blob_of("S", [](GrammarBlob::Writer &w) {
    w.u8(GrammarBlob::T_CharClass);
    w.u8(0);  // not negated
    w.u8(0);  // not ignoring case
    w.u32(0); // ranges
  });
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);
}

// A recovery skips with whatever expression it holds, a label or not.
TEST(GrammarBlobTest, RecoveryOfAnyExpressionIsRead) {
  auto blob = blob_of("S", [](GrammarBlob::Writer &w) {
    w.u8(GrammarBlob::T_Recovery);
    write_literal_a(w);
  });
  std::string start;
  auto g = GrammarBlob::deserialize(blob, start);
  EXPECT_NO_THROW((*g)["S"].parse("a"));
}

// A bit this version does not know may change how the rest is read.
TEST(GrammarBlobTest, RejectsAnUnknownFlag) {
  auto blob = blob_of("S", write_literal_a, GrammarBlob::F_EoiCheck, {}, 0x80);
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);
}

TEST(GrammarBlobTest, RejectsAnIgnoredStartRule) {
  auto blob =
      blob_of("S", write_literal_a,
              GrammarBlob::F_EoiCheck | GrammarBlob::F_IgnoreSemanticValue);
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);
}

// A blob defining the one rule `S(x) <- x`, as a macro or not.
static std::vector<uint8_t> blob_with_a_parameter(bool is_macro) {
  return blob_of(
      "S",
      [](GrammarBlob::Writer &w) {
        w.u8(GrammarBlob::T_Reference);
        w.u8(0); // not a macro call
        w.str("x");
        w.u32(0); // arguments
      },
      GrammarBlob::F_EoiCheck | (is_macro ? GrammarBlob::F_IsMacro : 0), {"x"});
}

TEST(GrammarBlobTest, RejectsAMacroStartRule) {
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob_with_a_parameter(true), start),
               std::runtime_error);
}

TEST(GrammarBlobTest, RejectsParametersOfARuleThatIsNoMacro) {
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob_with_a_parameter(false), start),
               std::runtime_error);
}

TEST(GrammarBlobTest, FailedLoadBlobKeepsTheGrammar) {
  peg::parser p("A <- 'a'");
  ASSERT_TRUE(p);

  auto blob = peg::parser("T <- 'x'").serialize_grammar();
  blob.pop_back();
  EXPECT_FALSE(p.load_blob(blob));

  // Still the old grammar, from its own start rule.
  EXPECT_TRUE(p.parse("a"));
  EXPECT_FALSE(p.parse("x"));
}

// load_blob() returns only a bool, so the logger is told why.
TEST(GrammarBlobTest, FailedLoadBlobTellsTheLoggerWhy) {
  peg::parser p;
  std::vector<std::string> messages;
  p.set_logger([&](size_t line, size_t col, const std::string &msg) {
    EXPECT_EQ(0u, line);
    EXPECT_EQ(0u, col);
    messages.push_back(msg);
  });

  EXPECT_FALSE(p.load_blob({0, 1, 2, 3}));
  EXPECT_EQ(
      std::vector<std::string>{"GrammarBlob: bad magic / not a grammar blob"},
      messages);
}

// A rule whose body is a User operator is stored by name, and the
// application gives its body again on load.
static const char *grammar_with_rules_given = R"(
  S    <- ITEM (',' ITEM)*
  ITEM <- NAME / NUM / MARK
  NUM  <- < [0-9]+ >
)";

// NAME matches lowercase letters, and MARK a '#'. MARK is the kind of `usr`
// operator that is given the semantic values.
static Rules rules_given() {
  return {
      {"NAME", usr([](const char *s, size_t n) -> size_t {
         size_t i = 0;
         while (i < n && 'a' <= s[i] && s[i] <= 'z') {
           i++;
         }
         return i ? i : kFail;
       })},
      {"MARK",
       usr([](const char *s, size_t n, SemanticValues &, std::any &) -> size_t {
         return n && s[0] == '#' ? 1 : kFail;
       })},
  };
}

TEST(GrammarBlobTest, RulesGivenOnLoad) {
  auto rules = rules_given();
  parser text(grammar_with_rules_given, rules);
  ASSERT_TRUE(text);
  auto blob = text.serialize_grammar();

  parser loaded;
  ASSERT_TRUE(loaded.load_blob(blob, rules));
  expect_same_asts(text, loaded,
                   {"ab", "12", "#", "ab,12,#,cd", "ab,", "AB", "", "#,#"});
  EXPECT_TRUE(loaded.parse("ab,12,#,cd"));

  // A grammar loaded from the blob is stored the same way again.
  EXPECT_EQ(blob, loaded.serialize_grammar());
}

// The reader alone, on a blob that holds `S` by name.
TEST(GrammarBlobTest, HandMadeBlobOfARuleGivenOnLoad) {
  auto blob = blob_of(
      "S", [](GrammarBlob::Writer &) {}, GrammarBlob::F_EoiCheck, {},
      GrammarBlob::F2_GivenOnLoad);
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(blob, start), std::runtime_error);

  auto g = GrammarBlob::deserialize(
      blob, start, {{"S", usr([](const char *, size_t n) { return n; })}});
  EXPECT_TRUE((*g)["S"].parse("abc").ret);
}

TEST(GrammarBlobTest, LoadBlobNeedsTheRulesGivenOnLoad) {
  auto blob =
      parser(grammar_with_rules_given, rules_given()).serialize_grammar();
  auto only_mark = rules_given();
  only_mark.erase("NAME");
  auto other = only_mark;
  other["OTHER"] = usr([](const char *, size_t) -> size_t { return 0; });

  struct Case {
    Rules rules;
    const char *lacking; // the first lacking rule, in name order
  };
  for (const auto &[rules, lacking] :
       {Case{Rules{}, "'MARK'"}, Case{only_mark, "'NAME'"},
        Case{other, "'NAME'"}}) {
    parser p("A <- 'a'");
    std::vector<std::string> messages;
    p.set_logger([&](size_t, size_t, const std::string &msg) {
      messages.push_back(msg);
    });
    EXPECT_FALSE(p.load_blob(blob, rules));
    EXPECT_TRUE(p.parse("a")); // the grammar it had
    ASSERT_EQ(1u, messages.size());
    EXPECT_NE(std::string::npos, messages[0].find(lacking)) << messages[0];
  }
  EXPECT_FALSE(parser().load_blob(blob));
}

// The body given on load is a User operator, as the one stored by name was:
// the blob keeps what was worked out from that, such as whether the rule is
// left recursive.
TEST(GrammarBlobTest, RuleGivenOnLoadIsAUserOperator) {
  auto blob =
      parser(grammar_with_rules_given, rules_given()).serialize_grammar();
  for (const auto &body : {lit("a"), std::shared_ptr<Ope>()}) {
    auto rules = rules_given();
    rules["NAME"] = body;
    EXPECT_FALSE(parser().load_blob(blob, rules));
  }
}

// `~` in the rules is the text loader's way to ignore a rule's value. On load
// the name is found either way, and the blob says whether the value is
// ignored.
TEST(GrammarBlobTest, RuleGivenOnLoadByNameWithATilde) {
  auto name_body = rules_given().at("NAME");
  auto blob = parser("S <- NAME NUM\nNUM <- < [0-9]+ >", {{"~NAME", name_body}})
                  .serialize_grammar();
  for (const auto &key : {"NAME", "~NAME"}) {
    parser p;
    ASSERT_TRUE(p.load_blob(blob, {{key, name_body}})) << key;
    size_t values = 0;
    p["S"] = [&](const SemanticValues &vs) { values = vs.size(); };
    ASSERT_TRUE(p.parse("ab12")) << key;
    EXPECT_EQ(1u, values) << key;
  }
}

TEST(GrammarBlobTest, UserOperatorInsideARuleIsNotSerializable) {
  parser p("S <- NAME", {{"NAME", seq(lit("a"), rules_given().at("NAME"))}});
  ASSERT_TRUE(p);
  EXPECT_THROW(p.serialize_grammar(), std::runtime_error);
}

TEST(GrammarBlobTest, SerializeWithoutAGrammarIsEmpty) {
  peg::parser p("S <- A"); // A is undefined
  ASSERT_FALSE(p);
  EXPECT_TRUE(p.serialize_grammar().empty());
}

TEST(GrammarBlobTest, CharacterClasses) {
  // ranges, negated, and case-insensitive classes
  check_rt("S <- ([a-z] / [^0-9 ] / [A-C]i)+",
           {"abc", "ABC", "aBc", "x_y", "a1", "  ", ""});
}

TEST(GrammarBlobTest, PredicatesTokenBoundaryDot) {
  check_rt("S <- &'a' < (!'z' .)* > 'z'",
           {"abz", "z", "az", "abc", "", "ayz", "bz"});
}

TEST(GrammarBlobTest, RepetitionCounts) {
  check_rt(
      "S <- 'a'{2} 'b'{1,3} 'c'{2,}",
      {"aabcc", "aabbbccc", "aabcccc", "ab", "aabc", "aaabcc", "aabbbbcc"});
}

TEST(GrammarBlobTest, CaseInsensitiveLiteralAndDictionary) {
  check_rt("S <- 'hello'i / ('cat' | 'dog' | 'fish')",
           {"hello", "HELLO", "HeLLo", "cat", "dog", "fish", "bird", ""});
}

// A Dictionary reports vs.choice() as the matched word's index in declaration
// order. The Trie stores words sorted, so serialization must re-emit them by id
// or the choice index gets renumbered.
TEST(GrammarBlobTest, DictionaryChoiceIndexRoundTrip) {
  const char *g = "S <- 'Jan' | 'January' | 'Feb' | 'February'";
  peg::parser p1;
  ASSERT_TRUE(p1.load_grammar(g));
  peg::parser p2;
  ASSERT_TRUE(p2.load_blob(p1.serialize_grammar()));
  auto choice = [](const peg::SemanticValues &vs) -> long {
    return static_cast<long>(vs.choice());
  };
  p1["S"] = choice;
  p2["S"] = choice;
  // Declaration order: Jan=0, January=1, Feb=2, February=3.
  for (auto [in, idx] : std::vector<std::pair<const char *, long>>{
           {"Jan", 0}, {"January", 1}, {"Feb", 2}, {"February", 3}}) {
    long v1 = -1, v2 = -2;
    ASSERT_TRUE(p1.parse(in, v1)) << in;
    ASSERT_TRUE(p2.parse(in, v2)) << in;
    EXPECT_EQ(idx, v1) << "source " << in;
    EXPECT_EQ(idx, v2) << "deserialized " << in;
  }
}

TEST(GrammarBlobTest, WordBoundary) {
  check_rt("S <- ('if' / Name)+\nName <- < [a-z]+ >\n"
           "%whitespace <- [ ]*\n%word <- [a-z]+",
           {"if", "iff", "if foo", "ifelse", "foo if", "if if"});
}

TEST(GrammarBlobTest, Cut) {
  // ↑ (U+2191) commits the enclosing choice; round-trip must preserve it.
  check_rt("S <- A / B\nA <- 'a' \xe2\x86\x91 'b'\nB <- 'a' 'c'",
           {"ab", "ac", "a", "ax"});
}

TEST(GrammarBlobTest, RejectsCapture) {
  // A capture ($name<...>) carries a match-action callback that cannot be
  // serialized (its captured name is not recoverable), so it is rejected.
  std::string start;
  auto g = load("S <- $tag< [a-z]+ > '=' $tag", start);
  ASSERT_TRUE(g != nullptr);
  EXPECT_THROW(GrammarBlob::serialize(*g, start), std::runtime_error);
}

TEST(GrammarBlobTest, OptionalAndChoiceAndSequence) {
  check_rt("S <- '-'? Num ('.' Num)?\nNum <- [0-9]+",
           {"1", "-1", "1.5", "-12.34", "1.", ".5", "-", "12"});
}

TEST(GrammarBlobTest, SerializationIsDeterministic) {
  std::string start;
  auto g = load("S <- 'a' / 'b'\nT <- [0-9]+", start);
  ASSERT_TRUE(g != nullptr);
  EXPECT_EQ(GrammarBlob::serialize(*g, start),
            GrammarBlob::serialize(*g, start));
}

// Grammar is an unordered_map, so serializing in its iteration order would
// give a different byte sequence on another standard library. The definitions
// go out sorted by name, which keeps a checked-in blob byte-comparable across
// platforms. Iteration order almost never puts the smallest name first, so
// checking the leading record catches a return to raw map order.
TEST(GrammarBlobTest, DefinitionsAreSerializedInNameOrder) {
  std::string start;
  auto g = load("S <- Zed / Alpha\nZed <- 'z'\nAlpha <- 'a'", start);
  ASSERT_TRUE(g != nullptr);
  auto blob = GrammarBlob::serialize(*g, start);

  std::string smallest;
  for (auto &kv : *g)
    if (smallest.empty() || kv.first < smallest) smallest = kv.first;

  // Header: magic (u32), start rule (u32 length + bytes), definition count
  // (u32); then each definition, name first.
  size_t pos = 4;
  auto read_u32 = [&]() {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
      v |= (uint32_t)blob[pos++] << (8 * i);
    return v;
  };
  auto read_str = [&]() {
    auto n = read_u32();
    std::string s((const char *)blob.data() + pos, n);
    pos += n;
    return s;
  };
  EXPECT_EQ(read_str(), start);
  EXPECT_EQ(read_u32(), (uint32_t)g->size());
  EXPECT_EQ(read_str(), smallest);
}

TEST(GrammarBlobTest, TruncatedBlobThrows) {
  std::string start;
  auto g = load("S <- 'abcdef' B+\nB <- [0-9]", start);
  ASSERT_TRUE(g != nullptr);
  auto blob = GrammarBlob::serialize(*g, start);
  blob.resize(blob.size() / 2);
  std::string s;
  EXPECT_THROW(GrammarBlob::deserialize(blob, s), std::runtime_error);
}

TEST(GrammarBlobTest, SingleLiteralRule) {
  check_rt("S <- 'hello'", {"hello", "hell", "helloo", "", "Hello"});
}
