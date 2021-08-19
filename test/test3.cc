#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

inline bool exact(Grammar &g, const char *d, const char *s) {
  auto n = strlen(s);
  auto r = g[d].parse(s, n);
  return r.ret && r.len == n;
}

inline Grammar &make_peg_grammar() { return ParserGenerator::grammar(); }

TEST(PEGTest, PEG_Grammar) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(
      exact(g, "Grammar",
            " Definition <- a / ( b c ) / d \n rule2 <- [a-zA-Z][a-z0-9-]+ "));
}

TEST(LeftRecursiveTest, PEG_Definition) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Definition", "Definition <- a / (b c) / d "));
  EXPECT_TRUE(exact(g, "Definition", "Definition <- a / b c / d "));
  EXPECT_TRUE(exact(g, "Definition", u8"Definitiond ← a "));
  EXPECT_FALSE(exact(g, "Definition", "Definition "));
  EXPECT_FALSE(exact(g, "Definition", " "));
  EXPECT_FALSE(exact(g, "Definition", ""));
  EXPECT_FALSE(exact(g, "Definition", "Definition = a / (b c) / d "));
  EXPECT_TRUE(exact(g, "Definition", "Macro(param) <- a "));
  EXPECT_FALSE(exact(g, "Definition", "Macro (param) <- a "));
}

TEST(LeftRecursiveTest, PEG_Expression) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Expression", "a / (b c) / d "));
  EXPECT_TRUE(exact(g, "Expression", "a / b c / d "));
  EXPECT_TRUE(exact(g, "Expression", "a b "));
  EXPECT_TRUE(exact(g, "Expression", ""));
  EXPECT_FALSE(exact(g, "Expression", " "));
  EXPECT_FALSE(exact(g, "Expression", " a b "));

  // NOTE: The followings are actually allowed in the original Ford's paper...
  EXPECT_TRUE(exact(g, "Expression", "a//b "));
  EXPECT_TRUE(exact(g, "Expression", "a // b "));
  EXPECT_TRUE(exact(g, "Expression", "a / / b "));
}

TEST(LeftRecursiveTest, PEG_Sequence) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Sequence", "a b c d "));
  EXPECT_TRUE(exact(g, "Sequence", ""));
  EXPECT_FALSE(exact(g, "Sequence", "!"));
  EXPECT_FALSE(exact(g, "Sequence", "<-"));
  EXPECT_FALSE(exact(g, "Sequence", " a"));
}

TEST(LeftRecursiveTest, PEG_Prefix) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Prefix", "&[a]"));
  EXPECT_TRUE(exact(g, "Prefix", "![']"));
  EXPECT_FALSE(exact(g, "Prefix", "-[']"));
  EXPECT_FALSE(exact(g, "Prefix", ""));
  EXPECT_FALSE(exact(g, "Prefix", " a"));
}

TEST(LeftRecursiveTest, PEG_Suffix) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Suffix", "aaa "));
  EXPECT_TRUE(exact(g, "Suffix", "aaa? "));
  EXPECT_TRUE(exact(g, "Suffix", "aaa* "));
  EXPECT_TRUE(exact(g, "Suffix", "aaa+ "));
  EXPECT_FALSE(exact(g, "Suffix", "aaa{} "));
  EXPECT_TRUE(exact(g, "Suffix", "aaa{10} "));
  EXPECT_TRUE(exact(g, "Suffix", "aaa{10,} "));
  EXPECT_TRUE(exact(g, "Suffix", "aaa{10,100} "));
  EXPECT_TRUE(exact(g, "Suffix", "aaa{,100} "));
  EXPECT_TRUE(exact(g, "Suffix", ". + "));
  EXPECT_TRUE(exact(g, "Suffix", ". {10} "));
  EXPECT_FALSE(exact(g, "Suffix", "?"));
  EXPECT_FALSE(exact(g, "Suffix", "+"));
  EXPECT_FALSE(exact(g, "Suffix", "{10}"));
  EXPECT_FALSE(exact(g, "Suffix", ""));
  EXPECT_FALSE(exact(g, "Suffix", " a"));
}

TEST(LeftRecursiveTest, PEG_Primary) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Primary", "_Identifier0_ "));
  EXPECT_FALSE(exact(g, "Primary", "_Identifier0_<-"));
  EXPECT_TRUE(exact(g, "Primary", "( _Identifier0_ _Identifier1_ )"));
  EXPECT_TRUE(exact(g, "Primary", "'Literal String'"));
  EXPECT_TRUE(exact(g, "Primary", "\"Literal String\""));
  EXPECT_TRUE(exact(g, "Primary", "[a-zA-Z]"));
  EXPECT_TRUE(exact(g, "Primary", "."));
  EXPECT_FALSE(exact(g, "Primary", ""));
  EXPECT_FALSE(exact(g, "Primary", " "));
  EXPECT_FALSE(exact(g, "Primary", " a"));
  EXPECT_FALSE(exact(g, "Primary", ""));
}

TEST(LeftRecursiveTest, PEG_Identifier) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Identifier", "_Identifier0_ "));
  EXPECT_FALSE(exact(g, "Identifier", "0Identifier_ "));
  EXPECT_FALSE(exact(g, "Identifier", "Iden|t "));
  EXPECT_FALSE(exact(g, "Identifier", " "));
  EXPECT_FALSE(exact(g, "Identifier", " a"));
  EXPECT_FALSE(exact(g, "Identifier", ""));
}

TEST(LeftRecursiveTest, PEG_IdentStart) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "IdentStart", "_"));
  EXPECT_TRUE(exact(g, "IdentStart", "a"));
  EXPECT_TRUE(exact(g, "IdentStart", "Z"));
  EXPECT_FALSE(exact(g, "IdentStart", ""));
  EXPECT_FALSE(exact(g, "IdentStart", " "));
  EXPECT_FALSE(exact(g, "IdentStart", "0"));
}

TEST(LeftRecursiveTest, PEG_IdentRest) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "IdentRest", "_"));
  EXPECT_TRUE(exact(g, "IdentRest", "a"));
  EXPECT_TRUE(exact(g, "IdentRest", "Z"));
  EXPECT_FALSE(exact(g, "IdentRest", ""));
  EXPECT_FALSE(exact(g, "IdentRest", " "));
  EXPECT_TRUE(exact(g, "IdentRest", "0"));
}

TEST(LeftRecursiveTest, PEG_Literal) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Literal", "'abc' "));
  EXPECT_TRUE(exact(g, "Literal", "'a\\nb\\tc' "));
  EXPECT_TRUE(exact(g, "Literal", "'a\\277\tc' "));
  EXPECT_TRUE(exact(g, "Literal", "'a\\77\tc' "));
  EXPECT_FALSE(exact(g, "Literal", "'a\\80\tc' "));
  EXPECT_TRUE(exact(g, "Literal", "'\n' "));
  EXPECT_TRUE(exact(g, "Literal", "'a\\'b' "));
  EXPECT_FALSE(exact(g, "Literal", "'a'b' "));
  EXPECT_FALSE(exact(g, "Literal", "'a\"'b' "));
  EXPECT_TRUE(exact(g, "Literal", "\"'\\\"abc\\\"'\" "));
  EXPECT_FALSE(exact(g, "Literal", "\"'\"abc\"'\" "));
  EXPECT_FALSE(exact(g, "Literal", "abc"));
  EXPECT_FALSE(exact(g, "Literal", ""));
  EXPECT_FALSE(exact(g, "Literal", "\\"));
  EXPECT_TRUE(exact(g, "Literal", u8"'日本語'"));
  EXPECT_TRUE(exact(g, "Literal", u8"\"日本語\""));
  EXPECT_FALSE(exact(g, "Literal", u8"日本語"));
}

TEST(LeftRecursiveTest, PEG_Class) {
  auto g = ParserGenerator::grammar();
  EXPECT_FALSE(exact(g, "Class", "[]")); // NOTE: This is different from the Brian Ford's paper, but
                  // same as RegExp
  EXPECT_TRUE(exact(g, "Class", "[a]"));
  EXPECT_TRUE(exact(g, "Class", "[a-z]"));
  EXPECT_TRUE(exact(g, "Class", "[az]"));
  EXPECT_TRUE(exact(g, "Class", "[a-zA-Z-]"));
  EXPECT_TRUE(exact(g, "Class", "[a-zA-Z-0-9]"));
  EXPECT_FALSE(exact(g, "Class", "[a-]"));
  EXPECT_TRUE(exact(g, "Class", "[-a]"));
  EXPECT_FALSE(exact(g, "Class", "["));
  EXPECT_FALSE(exact(g, "Class", "[a"));
  EXPECT_FALSE(exact(g, "Class", "]"));
  EXPECT_FALSE(exact(g, "Class", "a]"));
  EXPECT_TRUE(exact(g, "Class", u8"[あ-ん]"));
  EXPECT_FALSE(exact(g, "Class", u8"あ-ん"));
  EXPECT_TRUE(exact(g, "Class", "[-+]"));
  EXPECT_FALSE(exact(g, "Class", "[+-]"));
  EXPECT_TRUE(exact(g, "Class", "[\\^]"));
}

TEST(LeftRecursiveTest, PEG_Negated_Class) {
  auto g = ParserGenerator::grammar();
  EXPECT_FALSE(exact(g, "NegatedClass", "[^]"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^a]"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^a-z]"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^az]"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^a-zA-Z-]"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^a-zA-Z-0-9]"));
  EXPECT_FALSE(exact(g, "NegatedClass", "[^a-]"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^-a]"));
  EXPECT_FALSE(exact(g, "NegatedClass", "[^"));
  EXPECT_FALSE(exact(g, "NegatedClass", "[^a"));
  EXPECT_FALSE(exact(g, "NegatedClass", "^]"));
  EXPECT_FALSE(exact(g, "NegatedClass", "^a]"));
  EXPECT_TRUE(exact(g, "NegatedClass", u8"[^あ-ん]"));
  EXPECT_FALSE(exact(g, "NegatedClass", u8"^あ-ん"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^-+]"));
  EXPECT_FALSE(exact(g, "NegatedClass", "[^+-]"));
  EXPECT_TRUE(exact(g, "NegatedClass", "[^^]"));
}

TEST(LeftRecursiveTest, PEG_Range) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Range", "a"));
  EXPECT_TRUE(exact(g, "Range", "a-z"));
  EXPECT_FALSE(exact(g, "Range", "az"));
  EXPECT_FALSE(exact(g, "Range", ""));
  EXPECT_FALSE(exact(g, "Range", "a-"));
  EXPECT_FALSE(exact(g, "Range", "-a"));
}

TEST(LeftRecursiveTest, PEG_Char) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Char", "\\n"));
  EXPECT_TRUE(exact(g, "Char", "\\r"));
  EXPECT_TRUE(exact(g, "Char", "\\t"));
  EXPECT_TRUE(exact(g, "Char", "\\'"));
  EXPECT_TRUE(exact(g, "Char", "\\\""));
  EXPECT_TRUE(exact(g, "Char", "\\["));
  EXPECT_TRUE(exact(g, "Char", "\\]"));
  EXPECT_TRUE(exact(g, "Char", "\\\\"));
  EXPECT_TRUE(exact(g, "Char", "\\000"));
  EXPECT_TRUE(exact(g, "Char", "\\377"));
  EXPECT_FALSE(exact(g, "Char", "\\477"));
  EXPECT_FALSE(exact(g, "Char", "\\087"));
  EXPECT_FALSE(exact(g, "Char", "\\079"));
  EXPECT_TRUE(exact(g, "Char", "\\00"));
  EXPECT_TRUE(exact(g, "Char", "\\77"));
  EXPECT_FALSE(exact(g, "Char", "\\80"));
  EXPECT_FALSE(exact(g, "Char", "\\08"));
  EXPECT_TRUE(exact(g, "Char", "\\0"));
  EXPECT_TRUE(exact(g, "Char", "\\7"));
  EXPECT_FALSE(exact(g, "Char", "\\8"));
  EXPECT_TRUE(exact(g, "Char", "\\x0"));
  EXPECT_TRUE(exact(g, "Char", "\\x00"));
  EXPECT_FALSE(exact(g, "Char", "\\x000"));
  EXPECT_TRUE(exact(g, "Char", "\\xa"));
  EXPECT_TRUE(exact(g, "Char", "\\xab"));
  EXPECT_FALSE(exact(g, "Char", "\\xabc"));
  EXPECT_TRUE(exact(g, "Char", "\\xA"));
  EXPECT_TRUE(exact(g, "Char", "\\xAb"));
  EXPECT_FALSE(exact(g, "Char", "\\xAbc"));
  EXPECT_FALSE(exact(g, "Char", "\\xg"));
  EXPECT_FALSE(exact(g, "Char", "\\xga"));
  EXPECT_FALSE(exact(g, "Char", "\\u0"));
  EXPECT_FALSE(exact(g, "Char", "\\u00"));
  EXPECT_TRUE(exact(g, "Char", "\\u0000"));
  EXPECT_TRUE(exact(g, "Char", "\\u000000"));
  EXPECT_FALSE(exact(g, "Char", "\\u0000000"));
  EXPECT_TRUE(exact(g, "Char", "\\uFFFF"));
  EXPECT_TRUE(exact(g, "Char", "\\u10000"));
  EXPECT_TRUE(exact(g, "Char", "\\u10FFFF"));
  EXPECT_FALSE(exact(g, "Char", "\\u110000"));
  EXPECT_FALSE(exact(g, "Char", "\\uFFFFFF"));
  EXPECT_TRUE(exact(g, "Char", "a"));
  EXPECT_TRUE(exact(g, "Char", "."));
  EXPECT_TRUE(exact(g, "Char", "0"));
  EXPECT_FALSE(exact(g, "Char", "\\"));
  EXPECT_TRUE(exact(g, "Char", " "));
  EXPECT_FALSE(exact(g, "Char", "  "));
  EXPECT_FALSE(exact(g, "Char", ""));
  EXPECT_TRUE(exact(g, "Char", u8"あ"));
}

TEST(LeftRecursiveTest, PEG_Operators) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "LEFTARROW", "<-"));
  EXPECT_TRUE(exact(g, "SLASH", "/ "));
  EXPECT_TRUE(exact(g, "AND", "& "));
  EXPECT_TRUE(exact(g, "NOT", "! "));
  EXPECT_TRUE(exact(g, "QUESTION", "? "));
  EXPECT_TRUE(exact(g, "STAR", "* "));
  EXPECT_TRUE(exact(g, "PLUS", "+ "));
  EXPECT_TRUE(exact(g, "OPEN", "( "));
  EXPECT_TRUE(exact(g, "CLOSE", ") "));
  EXPECT_TRUE(exact(g, "DOT", ". "));
}

TEST(LeftRecursiveTest, PEG_Comment) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Comment", "# Comment.\n"));
  EXPECT_FALSE(exact(g, "Comment", "# Comment."));
  EXPECT_FALSE(exact(g, "Comment", " "));
  EXPECT_FALSE(exact(g, "Comment", "a"));
}

TEST(LeftRecursiveTest, PEG_Space) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "Space", " "));
  EXPECT_TRUE(exact(g, "Space", "\t"));
  EXPECT_TRUE(exact(g, "Space", "\n"));
  EXPECT_FALSE(exact(g, "Space", ""));
  EXPECT_FALSE(exact(g, "Space", "a"));
}

TEST(LeftRecursiveTest, PEG_EndOfLine) {
  auto g = ParserGenerator::grammar();
  EXPECT_TRUE(exact(g, "EndOfLine", "\r\n"));
  EXPECT_TRUE(exact(g, "EndOfLine", "\n"));
  EXPECT_TRUE(exact(g, "EndOfLine", "\r"));
  EXPECT_FALSE(exact(g, "EndOfLine", " "));
  EXPECT_FALSE(exact(g, "EndOfLine", ""));
  EXPECT_FALSE(exact(g, "EndOfLine", "a"));
}

TEST(LeftRecursiveTest, PEG_EndOfFile) {
  Grammar g = make_peg_grammar();
  EXPECT_TRUE(exact(g, "EndOfFile", ""));
  EXPECT_FALSE(exact(g, "EndOfFile", " "));
}
