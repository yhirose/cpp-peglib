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

TEST(GrammarBlobTest, RejectsPrecedence) {
  const char *g = "E <- Infix(P, O)\nInfix(A, B) <- A (B A)* {\n  precedence\n "
                  "   L '+' '-'\n}\nP <- [0-9]\nO <- < [-+] >";
  std::string start;
  auto orig = load(g, start);
  ASSERT_TRUE(orig != nullptr);
  EXPECT_THROW(GrammarBlob::serialize(*orig, start), std::runtime_error);
}

TEST(GrammarBlobTest, RejectsBadMagic) {
  std::vector<uint8_t> junk = {0, 1, 2, 3, 4, 5, 6, 7};
  std::string start;
  EXPECT_THROW(GrammarBlob::deserialize(junk, start), std::runtime_error);
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
  p1.enable_ast();
  p2.enable_ast();

  for (auto in : {"1 + 2 * 3", "(1 + 2) * 3", "10 - 2 - 3", "", "1 +"}) {
    std::shared_ptr<peg::Ast> a1, a2;
    bool r1 = p1.parse(in, a1);
    bool r2 = p2.parse(in, a2);
    EXPECT_EQ(r1, r2) << in;
    if (r1 && r2) EXPECT_EQ(peg::ast_to_s(a1), peg::ast_to_s(a2)) << in;
  }
}

// Regression: load_blob() must restore the parser-level packrat flag from the
// blob so a subsequent enable_packrat_parsing() re-applies packrat instead of
// clearing the flag baked into the blob. Previously load_blob left the parser
// member at its false default, so enable_packrat_parsing() silently disabled
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

TEST(GrammarBlobTest, LoadBlobRejectsGarbage) {
  peg::parser p;
  std::vector<uint8_t> junk = {9, 9, 9, 9};
  EXPECT_FALSE(p.load_blob(junk));
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
