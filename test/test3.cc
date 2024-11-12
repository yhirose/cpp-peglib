#include <gtest/gtest.h>
#include <peglib.h>

using namespace peg;

TEST(PEGTest, PEG_Grammar) {
  EXPECT_TRUE(ParserGenerator::parse_test(
      "Grammar",
      " Definition <- a / ( b c ) / d \n rule2 <- [a-zA-Z][a-z0-9-]+ "));
}

TEST(PEGTest, PEG_Definition) {
  EXPECT_TRUE(ParserGenerator::parse_test("Definition",
                                          "Definition <- a / (b c) / d "));
  EXPECT_TRUE(
      ParserGenerator::parse_test("Definition", "Definition <- a / b c / d "));
  EXPECT_TRUE(ParserGenerator::parse_test("Definition", u8"Definitiond ← a "));
  EXPECT_FALSE(ParserGenerator::parse_test("Definition", "Definition "));
  EXPECT_FALSE(ParserGenerator::parse_test("Definition", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("Definition", ""));
  EXPECT_FALSE(
      ParserGenerator::parse_test("Definition", "Definition = a / (b c) / d "));
  EXPECT_TRUE(ParserGenerator::parse_test("Definition", "Macro(param) <- a "));
  EXPECT_FALSE(
      ParserGenerator::parse_test("Definition", "Macro (param) <- a "));
}

TEST(PEGTest, PEG_Expression) {
  EXPECT_TRUE(ParserGenerator::parse_test("Expression", "a / (b c) / d "));
  EXPECT_TRUE(ParserGenerator::parse_test("Expression", "a / b c / d "));
  EXPECT_TRUE(ParserGenerator::parse_test("Expression", "a b "));
  EXPECT_TRUE(ParserGenerator::parse_test("Expression", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Expression", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("Expression", " a b "));

  // NOTE: The followings are actually allowed in the original Ford's paper...
  EXPECT_TRUE(ParserGenerator::parse_test("Expression", "a//b "));
  EXPECT_TRUE(ParserGenerator::parse_test("Expression", "a // b "));
  EXPECT_TRUE(ParserGenerator::parse_test("Expression", "a / / b "));
}

TEST(PEGTest, PEG_Sequence) {
  EXPECT_TRUE(ParserGenerator::parse_test("Sequence", "a b c d "));
  EXPECT_TRUE(ParserGenerator::parse_test("Sequence", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Sequence", "!"));
  EXPECT_FALSE(ParserGenerator::parse_test("Sequence", "<-"));
  EXPECT_FALSE(ParserGenerator::parse_test("Sequence", " a"));
}

TEST(PEGTest, PEG_Prefix) {
  EXPECT_TRUE(ParserGenerator::parse_test("Prefix", "&[a]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Prefix", "![']"));
  EXPECT_FALSE(ParserGenerator::parse_test("Prefix", "-[']"));
  EXPECT_FALSE(ParserGenerator::parse_test("Prefix", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Prefix", " a"));
}

TEST(PEGTest, PEG_Suffix) {
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa? "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa* "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa+ "));
  EXPECT_FALSE(ParserGenerator::parse_test("Suffix", "aaa{} "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa{10} "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa{10,} "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa{10,100} "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", "aaa{,100} "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", ". + "));
  EXPECT_TRUE(ParserGenerator::parse_test("Suffix", ". {10} "));
  EXPECT_FALSE(ParserGenerator::parse_test("Suffix", "?"));
  EXPECT_FALSE(ParserGenerator::parse_test("Suffix", "+"));
  EXPECT_FALSE(ParserGenerator::parse_test("Suffix", "{10}"));
  EXPECT_FALSE(ParserGenerator::parse_test("Suffix", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Suffix", " a"));
}

TEST(PEGTest, PEG_Primary) {
  EXPECT_TRUE(ParserGenerator::parse_test("Primary", "_Identifier0_ "));
  EXPECT_FALSE(ParserGenerator::parse_test("Primary", "_Identifier0_<-"));
  EXPECT_TRUE(ParserGenerator::parse_test("Primary",
                                          "( _Identifier0_ _Identifier1_ )"));
  EXPECT_TRUE(ParserGenerator::parse_test("Primary", "'Literal String'"));
  EXPECT_TRUE(ParserGenerator::parse_test("Primary", "\"Literal String\""));
  EXPECT_TRUE(ParserGenerator::parse_test("Primary", "[a-zA-Z]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Primary", "."));
  EXPECT_FALSE(ParserGenerator::parse_test("Primary", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Primary", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("Primary", " a"));
  EXPECT_FALSE(ParserGenerator::parse_test("Primary", ""));
}

TEST(PEGTest, PEG_Identifier) {
  EXPECT_TRUE(ParserGenerator::parse_test("Identifier", "_Identifier0_ "));
  EXPECT_FALSE(ParserGenerator::parse_test("Identifier", "0Identifier_ "));
  EXPECT_FALSE(ParserGenerator::parse_test("Identifier", "Iden|t "));
  EXPECT_FALSE(ParserGenerator::parse_test("Identifier", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("Identifier", " a"));
  EXPECT_FALSE(ParserGenerator::parse_test("Identifier", ""));
}

TEST(PEGTest, PEG_IdentStart) {
  EXPECT_TRUE(ParserGenerator::parse_test("IdentStart", "_"));
  EXPECT_TRUE(ParserGenerator::parse_test("IdentStart", "a"));
  EXPECT_TRUE(ParserGenerator::parse_test("IdentStart", "Z"));
  EXPECT_FALSE(ParserGenerator::parse_test("IdentStart", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("IdentStart", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("IdentStart", "0"));
}

TEST(PEGTest, PEG_IdentRest) {
  EXPECT_TRUE(ParserGenerator::parse_test("IdentRest", "_"));
  EXPECT_TRUE(ParserGenerator::parse_test("IdentRest", "a"));
  EXPECT_TRUE(ParserGenerator::parse_test("IdentRest", "Z"));
  EXPECT_FALSE(ParserGenerator::parse_test("IdentRest", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("IdentRest", " "));
  EXPECT_TRUE(ParserGenerator::parse_test("IdentRest", "0"));
}

TEST(PEGTest, PEG_Literal) {
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", "'abc' "));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", "'a\\nb\\tc' "));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", "'a\\277\tc' "));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", "'a\\77\tc' "));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", "'a\\80\tc' "));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", "'\n' "));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", "'a\\'b' "));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", "'a'b' "));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", "'a\"'b' "));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", "\"'\\\"abc\\\"'\" "));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", "\"'\"abc\"'\" "));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", "abc"));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", "\\"));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", u8"'日本語'"));
  EXPECT_TRUE(ParserGenerator::parse_test("Literal", u8"\"日本語\""));
  EXPECT_FALSE(ParserGenerator::parse_test("Literal", u8"日本語"));
}

TEST(PEGTest, PEG_Class) {
  EXPECT_FALSE(ParserGenerator::parse_test(
      "Class", "[]")); // NOTE: This is different from the Brian Ford's paper,
                       // but same as RegExp
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[a]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[a-z]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[az]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[a-zA-Z-]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[a-zA-Z-0-9]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[a-]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[-a]"));
  EXPECT_FALSE(ParserGenerator::parse_test("Class", "["));
  EXPECT_FALSE(ParserGenerator::parse_test("Class", "[a"));
  EXPECT_FALSE(ParserGenerator::parse_test("Class", "]"));
  EXPECT_FALSE(ParserGenerator::parse_test("Class", "a]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", u8"[あ-ん]"));
  EXPECT_FALSE(ParserGenerator::parse_test("Class", u8"あ-ん"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[-+]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[+-]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Class", "[\\^]"));
}

TEST(PEGTest, PEG_Negated_Class) {
  EXPECT_FALSE(ParserGenerator::parse_test("NegatedClass", "[^]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^a]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^a-z]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^az]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^a-zA-Z-]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^a-zA-Z-0-9]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^a-]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^-a]"));
  EXPECT_FALSE(ParserGenerator::parse_test("NegatedClass", "[^"));
  EXPECT_FALSE(ParserGenerator::parse_test("NegatedClass", "[^a"));
  EXPECT_FALSE(ParserGenerator::parse_test("NegatedClass", "^]"));
  EXPECT_FALSE(ParserGenerator::parse_test("NegatedClass", "^a]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", u8"[^あ-ん]"));
  EXPECT_FALSE(ParserGenerator::parse_test("NegatedClass", u8"^あ-ん"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^-+]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^+-]"));
  EXPECT_TRUE(ParserGenerator::parse_test("NegatedClass", "[^^]"));
}

TEST(PEGTest, PEG_Range) {
  EXPECT_TRUE(ParserGenerator::parse_test("Range", "a"));
  EXPECT_TRUE(ParserGenerator::parse_test("Range", "a-z"));
  EXPECT_FALSE(ParserGenerator::parse_test("Range", "az"));
  EXPECT_FALSE(ParserGenerator::parse_test("Range", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Range", "a-"));
  EXPECT_FALSE(ParserGenerator::parse_test("Range", "-a"));
}

TEST(PEGTest, PEG_Char) {
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\f"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\n"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\r"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\t"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\v"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\'"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\\""));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\["));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\]"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\\\"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\000"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\377"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\477"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\087"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\079"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\00"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\77"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\80"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\08"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\0"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\7"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\8"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\x0"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\x00"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\x000"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\xa"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\xab"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\xabc"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\xA"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\xAb"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\xAbc"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\xg"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\xga"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\u0"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\u00"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\u0000"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\u000000"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\u0000000"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\uFFFF"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\u10000"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "\\u10FFFF"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\u110000"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\uFFFFFF"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "a"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "."));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", "0"));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "\\"));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", "  "));
  EXPECT_FALSE(ParserGenerator::parse_test("Char", ""));
  EXPECT_TRUE(ParserGenerator::parse_test("Char", u8"あ"));
}

TEST(PEGTest, PEG_Operators) {
  EXPECT_TRUE(ParserGenerator::parse_test("LEFTARROW", "<-"));
  EXPECT_TRUE(ParserGenerator::parse_test("SLASH", "/ "));
  EXPECT_TRUE(ParserGenerator::parse_test("AND", "& "));
  EXPECT_TRUE(ParserGenerator::parse_test("NOT", "! "));
  EXPECT_TRUE(ParserGenerator::parse_test("QUESTION", "? "));
  EXPECT_TRUE(ParserGenerator::parse_test("STAR", "* "));
  EXPECT_TRUE(ParserGenerator::parse_test("PLUS", "+ "));
  EXPECT_TRUE(ParserGenerator::parse_test("OPEN", "( "));
  EXPECT_TRUE(ParserGenerator::parse_test("CLOSE", ") "));
  EXPECT_TRUE(ParserGenerator::parse_test("DOT", ". "));
}

TEST(PEGTest, PEG_Comment) {
  EXPECT_TRUE(ParserGenerator::parse_test("Comment", "# Comment.\n"));
  EXPECT_FALSE(ParserGenerator::parse_test("Comment", "# Comment."));
  EXPECT_FALSE(ParserGenerator::parse_test("Comment", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("Comment", "a"));
}

TEST(PEGTest, PEG_Space) {
  EXPECT_TRUE(ParserGenerator::parse_test("Space", " "));
  EXPECT_TRUE(ParserGenerator::parse_test("Space", "\t"));
  EXPECT_TRUE(ParserGenerator::parse_test("Space", "\n"));
  EXPECT_FALSE(ParserGenerator::parse_test("Space", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("Space", "a"));
}

TEST(PEGTest, PEG_EndOfLine) {
  EXPECT_TRUE(ParserGenerator::parse_test("EndOfLine", "\r\n"));
  EXPECT_TRUE(ParserGenerator::parse_test("EndOfLine", "\n"));
  EXPECT_TRUE(ParserGenerator::parse_test("EndOfLine", "\r"));
  EXPECT_FALSE(ParserGenerator::parse_test("EndOfLine", " "));
  EXPECT_FALSE(ParserGenerator::parse_test("EndOfLine", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("EndOfLine", "a"));
}

TEST(PEGTest, PEG_EndOfFile) {
  EXPECT_TRUE(ParserGenerator::parse_test("EndOfFile", ""));
  EXPECT_FALSE(ParserGenerator::parse_test("EndOfFile", " "));
}
