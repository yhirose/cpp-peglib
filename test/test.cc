
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <peglib.h>
#include <iostream>

TEST_CASE("String capture test", "[general]")
{
    {
#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
    auto parser = peglib::make_parser(
        "  ROOT      <-  _ ('[' TAG_NAME ']' _)*  "
        "  TAG_NAME  <-  (!']' .)+                "
        "  _         <-  [ \t]*                   "
    );
#else
    auto parser = peglib::make_parser(R"(
        ROOT      <-  _ ('[' TAG_NAME ']' _)*
        TAG_NAME  <-  (!']' .)+
        _         <-  [ \t]*
    )");
#endif

    std::vector<std::string> tags;

    parser["TAG_NAME"] = [&](const char* s, size_t l) {
        tags.push_back(std::string(s, l));
    };

    auto ret = parser.parse(" [tag1] [tag:2] [tag-3] ");

    REQUIRE(ret == true);
    REQUIRE(tags.size() == 3);
    REQUIRE(tags[0] == "tag1");
    REQUIRE(tags[1] == "tag:2");
    REQUIRE(tags[2] == "tag-3");
    }

    REQUIRE(peglib::VARINT_COUNT == 0);
}

using namespace peglib;
using namespace std;

TEST_CASE("String capture test with match", "[general]")
{
    {
    Definition ROOT, TAG, TAG_NAME, WS;

    ROOT     = seq(WS, zom(TAG));
    TAG      = seq(chr('['), TAG_NAME, chr(']'), WS);
    TAG_NAME = oom(seq(npd(chr(']')), any()));
    WS       = zom(cls(" \t"));

    vector<string> tags;

    TAG_NAME.match = [&](const char* s, size_t l) {
        tags.push_back(string(s, l));
    };

    auto ret = ROOT.parse(" [tag1] [tag:2] [tag-3] ");

    REQUIRE(ret == true);
    REQUIRE(tags.size() == 3);
    REQUIRE(tags[0] == "tag1");
    REQUIRE(tags[1] == "tag:2");
    REQUIRE(tags[2] == "tag-3");
    }

    REQUIRE(VARINT_COUNT == 0);
}

TEST_CASE("Cyclic grammer test", "[general]")
{
    {
    Definition PARENT;
    Definition CHILD;

    PARENT = seq(CHILD);
    CHILD  = seq(PARENT);
    }

    REQUIRE(VARINT_COUNT == 0);
}

TEST_CASE("Lambda action test", "[general]")
{
    auto parser = make_parser(
       "  START <- (CHAR)* "
       "  CHAR  <- .       ");

    string ss;
    parser["CHAR"] = [&](const char* s, size_t l) {
        ss += *s;
    };

    bool ret = parser.parse("hello");
    REQUIRE(ret == true);
    REQUIRE(ss == "hello");
}

TEST_CASE("Calculator test", "[general]")
{
    {
    // Construct grammer
    Definition EXPRESSION, TERM, FACTOR, TERM_OPERATOR, FACTOR_OPERATOR, NUMBER;

    EXPRESSION      = seq(TERM, zom(seq(TERM_OPERATOR, TERM)));
    TERM            = seq(FACTOR, zom(seq(FACTOR_OPERATOR, FACTOR)));
    FACTOR          = cho(NUMBER, seq(chr('('), EXPRESSION, chr(')')));
    TERM_OPERATOR   = cls("+-");
    FACTOR_OPERATOR = cls("*/");
    NUMBER          = oom(cls("0-9"));

    // Setup actions
    SemanticActions<Any> actions;

    auto reduce = [](const vector<Any>& v) -> long {
        long ret = v[0].get<long>();
        for (auto i = 1u; i < v.size(); i += 2) {
            auto num = v[i + 1].get<long>();
            switch (v[i].get<char>()) {
                case '+': ret += num; break;
                case '-': ret -= num; break;
                case '*': ret *= num; break;
                case '/': ret /= num; break;
            }
        }
        return ret;
    };

    actions[EXPRESSION] = reduce;
    actions[TERM] = reduce;
    actions[TERM_OPERATOR] = [](const char* s, size_t l) { return *s; };
    actions[FACTOR_OPERATOR] = [](const char* s, size_t l) { return *s; };
    actions[NUMBER] = [&](const char* s, size_t l) { return stol(string(s, l), nullptr, 10); };

    // Parse
    Any val;
    auto ret = EXPRESSION.parse("1+2*3*(4-5+6)/7-8", actions, val);

    REQUIRE(ret == true);
    REQUIRE(val.get<long>() == -3);
    }

    REQUIRE(VARINT_COUNT == 0);
}

TEST_CASE("Calculator test2", "[general]")
{
    {
        // Parse syntax
        auto syntax =
            "  # Grammar for Calculator...\n                          "
            "  EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*        "
            "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
            "  FACTOR           <-  NUMBER / '(' EXPRESSION ')'       "
            "  TERM_OPERATOR    <-  [-+]                              "
            "  FACTOR_OPERATOR  <-  [/*]                              "
            "  NUMBER           <-  [0-9]+                            "
            ;

        string start;
        auto grammar = make_grammar(syntax, start);
        auto& g = *grammar;

        // Setup actions
        SemanticActions<Any> a;

        auto reduce = [](const vector<Any>& v) -> long {
            long ret = v[0].get<long>();
            for (auto i = 1u; i < v.size(); i += 2) {
                auto num = v[i + 1].get<long>();
                switch (v[i].get<char>()) {
                    case '+': ret += num; break;
                    case '-': ret -= num; break;
                    case '*': ret *= num; break;
                    case '/': ret /= num; break;
                }
            }
            return ret;
        };

        a[g["EXPRESSION"]] = reduce;
        a[g["TERM"]] = reduce;
        a[g["TERM_OPERATOR"]] = [](const char* s, size_t l) { return *s; };
        a[g["FACTOR_OPERATOR"]] = [](const char* s, size_t l) { return *s; };
        a[g["NUMBER"]] = [&](const char* s, size_t l) { return stol(string(s, l), nullptr, 10); };

        // Parse
        Any val;
        auto ret = g[start].parse("1+2*3*(4-5+6)/7-8", a, val);

        REQUIRE(ret == true);
        REQUIRE(val.get<long>() == -3);
    }

    REQUIRE(VARINT_COUNT == 0);
}

TEST_CASE("Calculator test3", "[general]")
{
    {
        // Parse syntax
#if defined(_MSC_VER) && _MSC_VER < 1900 // Less than Visual Studio 2015
        auto parser = make_parser(
            "  # Grammar for Calculator...\n                          "
            "  EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*        "
            "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
            "  FACTOR           <-  NUMBER / '(' EXPRESSION ')'       "
            "  TERM_OPERATOR    <-  [-+]                              "
            "  FACTOR_OPERATOR  <-  [/*]                              "
            "  NUMBER           <-  [0-9]+                            "
            );
#else
        auto parser = make_parser(R"(
            # Grammar for Calculator...
            EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*
            TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
            FACTOR           <-  NUMBER / '(' EXPRESSION ')'
            TERM_OPERATOR    <-  [-+]
            FACTOR_OPERATOR  <-  [/*]
            NUMBER           <-  [0-9]+
        )");
#endif

        auto reduce = [](const vector<Any>& v) -> long {
            long ret = v[0].get<long>();
            for (auto i = 1u; i < v.size(); i += 2) {
                auto num = v[i + 1].get<long>();
                switch (v[i].get<char>()) {
                    case '+': ret += num; break;
                    case '-': ret -= num; break;
                    case '*': ret *= num; break;
                    case '/': ret /= num; break;
                }
            }
            return ret;
        };

        // Setup actions
        parser["EXPRESSION"]      = reduce;
        parser["TERM"]            = reduce;
        parser["TERM_OPERATOR"]   = [](const char* s, size_t l) { return (char)*s; };
        parser["FACTOR_OPERATOR"] = [](const char* s, size_t l) { return (char)*s; };
        parser["NUMBER"]          = [](const char* s, size_t l) { return stol(string(s, l), nullptr, 10); };

        // Parse
        long val;
        auto ret = parser.parse("1+2*3*(4-5+6)/7-8", val);

        REQUIRE(ret == true);
        REQUIRE(val == -3);
    }

    REQUIRE(VARINT_COUNT == 0);
}

TEST_CASE("PEG Grammar", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Grammar"].parse(" Definition <- a / ( b c ) / d \n rule2 <- [a-zA-Z][a-z0-9-]+ ") == true);
}

TEST_CASE("PEG Definition", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Definition"].parse("Definition <- a / (b c) / d ") == true);
    REQUIRE(g["Definition"].parse("Definition <- a / b c / d ") == true);
    REQUIRE(g["Definition"].parse("Definition ") == false);
    REQUIRE(g["Definition"].parse(" ") == false);
    REQUIRE(g["Definition"].parse("") == false);
    REQUIRE(g["Definition"].parse("Definition = a / (b c) / d ") == false);
}

TEST_CASE("PEG Expression", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Expression"].parse("a / (b c) / d ") == true);
    REQUIRE(g["Expression"].parse("a / b c / d ") == true);
    REQUIRE(g["Expression"].parse("a b ") == true);
    REQUIRE(g["Expression"].parse("") == true);
    REQUIRE(g["Expression"].parse(" ") == false);
    REQUIRE(g["Expression"].parse(" a b ") == false);
}

TEST_CASE("PEG Sequence", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Sequence"].parse("a b c d ") == true);
    REQUIRE(g["Sequence"].parse("") == true);
    REQUIRE(g["Sequence"].parse("!") == false);
    REQUIRE(g["Sequence"].parse("<-") == false);
    REQUIRE(g["Sequence"].parse(" a") == false);
}

TEST_CASE("PEG Prefix", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Prefix"].parse("&[a]") == true);
    REQUIRE(g["Prefix"].parse("![']") == true);
    REQUIRE(g["Prefix"].parse("-[']") == false);
    REQUIRE(g["Prefix"].parse("") == false);
    REQUIRE(g["Sequence"].parse(" a") == false);
}

TEST_CASE("PEG Suffix", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Suffix"].parse("aaa ") == true);
    REQUIRE(g["Suffix"].parse("aaa? ") == true);
    REQUIRE(g["Suffix"].parse("aaa* ") == true);
    REQUIRE(g["Suffix"].parse("aaa+ ") == true);
    REQUIRE(g["Suffix"].parse(". + ") == true);
    REQUIRE(g["Suffix"].parse("?") == false);
    REQUIRE(g["Suffix"].parse("") == false);
    REQUIRE(g["Sequence"].parse(" a") == false);
}

TEST_CASE("PEG Primary", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Primary"].parse("_Identifier0_ ") == true);
    REQUIRE(g["Primary"].parse("_Identifier0_<-") == false);
    REQUIRE(g["Primary"].parse("( _Identifier0_ _Identifier1_ )") == true);
    REQUIRE(g["Primary"].parse("'Literal String'") == true);
    REQUIRE(g["Primary"].parse("\"Literal String\"") == true);
    REQUIRE(g["Primary"].parse("[a-zA-Z]") == true);
    REQUIRE(g["Primary"].parse(".") == true);
    REQUIRE(g["Primary"].parse("") == false);
    REQUIRE(g["Primary"].parse(" ") == false);
    REQUIRE(g["Primary"].parse(" a") == false);
    REQUIRE(g["Primary"].parse("") == false);
}

TEST_CASE("PEG Identifier", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Identifier"].parse("_Identifier0_ ") == true);
    REQUIRE(g["Identifier"].parse("0Identifier_ ") == false);
    REQUIRE(g["Identifier"].parse("Iden|t ") == false);
    REQUIRE(g["Identifier"].parse(" ") == false);
    REQUIRE(g["Identifier"].parse(" a") == false);
    REQUIRE(g["Identifier"].parse("") == false);
}

TEST_CASE("PEG IdentStart", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["IdentStart"].parse("_") == true);
    REQUIRE(g["IdentStart"].parse("a") == true);
    REQUIRE(g["IdentStart"].parse("Z") == true);
    REQUIRE(g["IdentStart"].parse("") == false);
    REQUIRE(g["IdentStart"].parse(" ") == false);
    REQUIRE(g["IdentStart"].parse("0") == false);
}

TEST_CASE("PEG IdentCont", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["IdentCont"].parse("_") == true);
    REQUIRE(g["IdentCont"].parse("a") == true);
    REQUIRE(g["IdentCont"].parse("Z") == true);
    REQUIRE(g["IdentCont"].parse("") == false);
    REQUIRE(g["IdentCont"].parse(" ") == false);
    REQUIRE(g["IdentCont"].parse("0") == true);
}

TEST_CASE("PEG Literal", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Literal"].parse("'abc' ") == true);
    REQUIRE(g["Literal"].parse("'a\\nb\\tc' ") == true);
    REQUIRE(g["Literal"].parse("'a\\277\tc' ") == true);
    REQUIRE(g["Literal"].parse("'a\\77\tc' ") == true);
    REQUIRE(g["Literal"].parse("'a\\80\tc' ") == false);
    REQUIRE(g["Literal"].parse("'\n' ") == true);
    REQUIRE(g["Literal"].parse("'a\\'b' ") == true);
    REQUIRE(g["Literal"].parse("'a'b' ") == false);
    REQUIRE(g["Literal"].parse("'a\"'b' ") == false);
    REQUIRE(g["Literal"].parse("\"'\\\"abc\\\"'\" ") == true);
    REQUIRE(g["Literal"].parse("\"'\"abc\"'\" ") == false);
    REQUIRE(g["Literal"].parse("abc") == false);
    REQUIRE(g["Literal"].parse("") == false);
    REQUIRE(g["Literal"].parse("日本語") == false);
}

TEST_CASE("PEG Class", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Class"].parse("[]") == true);
    REQUIRE(g["Class"].parse("[a]") == true);
    REQUIRE(g["Class"].parse("[a-z]") == true);
    REQUIRE(g["Class"].parse("[az]") == true);
    REQUIRE(g["Class"].parse("[a-zA-Z-]") == true);
    REQUIRE(g["Class"].parse("[a-zA-Z-0-9]") == true);
    REQUIRE(g["Class"].parse("[a-]") == false);
    REQUIRE(g["Class"].parse("[-a]") == true);
    REQUIRE(g["Class"].parse("[") == false);
    REQUIRE(g["Class"].parse("[a") == false);
    REQUIRE(g["Class"].parse("]") == false);
    REQUIRE(g["Class"].parse("a]") == false);
    REQUIRE(g["Class"].parse("あ-ん") == false);
    REQUIRE(g["Class"].parse("[-+]") == true);
    REQUIRE(g["Class"].parse("[+-]") == false);
}

TEST_CASE("PEG Range", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Range"].parse("a") == true);
    REQUIRE(g["Range"].parse("a-z") == true);
    REQUIRE(g["Range"].parse("az") == false);
    REQUIRE(g["Range"].parse("") == false);
    REQUIRE(g["Range"].parse("a-") == false);
    REQUIRE(g["Range"].parse("-a") == false);
}

TEST_CASE("PEG Char", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Char"].parse("\\n") == true);
    REQUIRE(g["Char"].parse("\\r") == true);
    REQUIRE(g["Char"].parse("\\t") == true);
    REQUIRE(g["Char"].parse("\\'") == true);
    REQUIRE(g["Char"].parse("\\\"") == true);
    REQUIRE(g["Char"].parse("\\[") == true);
    REQUIRE(g["Char"].parse("\\]") == true);
    REQUIRE(g["Char"].parse("\\\\") == true);
    REQUIRE(g["Char"].parse("\\000") == true);
    REQUIRE(g["Char"].parse("\\277") == true);
    REQUIRE(g["Char"].parse("\\377") == false);
    REQUIRE(g["Char"].parse("\\087") == false);
    REQUIRE(g["Char"].parse("\\079") == false);
    REQUIRE(g["Char"].parse("\\00") == true);
    REQUIRE(g["Char"].parse("\\77") == true);
    REQUIRE(g["Char"].parse("\\80") == false);
    REQUIRE(g["Char"].parse("\\08") == false);
    REQUIRE(g["Char"].parse("\\0") == true);
    REQUIRE(g["Char"].parse("\\7") == true);
    REQUIRE(g["Char"].parse("\\8") == false);
    REQUIRE(g["Char"].parse("a") == true);
    REQUIRE(g["Char"].parse(".") == true);
    REQUIRE(g["Char"].parse("0") == true);
    REQUIRE(g["Char"].parse("\\") == false);
    REQUIRE(g["Char"].parse(" ") == true);
    REQUIRE(g["Char"].parse("  ") == false);
    REQUIRE(g["Char"].parse("") == false);
    REQUIRE(g["Char"].parse("あ") == false);
}

TEST_CASE("PEG Operators", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["LEFTARROW"].parse("<-") == true);
    REQUIRE(g["SLASH"].parse("/ ") == true);
    REQUIRE(g["AND"].parse("& ") == true);
    REQUIRE(g["NOT"].parse("! ") == true);
    REQUIRE(g["QUESTION"].parse("? ") == true);
    REQUIRE(g["STAR"].parse("* ") == true);
    REQUIRE(g["PLUS"].parse("+ ") == true);
    REQUIRE(g["OPEN"].parse("( ") == true);
    REQUIRE(g["CLOSE"].parse(") ") == true);
    REQUIRE(g["DOT"].parse(". ") == true);
}

TEST_CASE("PEG Comment", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Comment"].parse("# Comment.\n") == true);
    REQUIRE(g["Comment"].parse("# Comment.") == false);
    REQUIRE(g["Comment"].parse(" ") == false);
    REQUIRE(g["Comment"].parse("a") == false);
}

TEST_CASE("PEG Space", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["Space"].parse(" ") == true);
    REQUIRE(g["Space"].parse("\t") == true);
    REQUIRE(g["Space"].parse("\n") == true);
    REQUIRE(g["Space"].parse("") == false);
    REQUIRE(g["Space"].parse("a") == false);
}

TEST_CASE("PEG EndOfLine", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["EndOfLine"].parse("\r\n") == true);
    REQUIRE(g["EndOfLine"].parse("\n") == true);
    REQUIRE(g["EndOfLine"].parse("\r") == true);
    REQUIRE(g["EndOfLine"].parse(" ") == false);
    REQUIRE(g["EndOfLine"].parse("") == false);
    REQUIRE(g["EndOfLine"].parse("a") == false);
}

TEST_CASE("PEG EndOfFile", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(g["EndOfFile"].parse("") == true);
    REQUIRE(g["EndOfFile"].parse(" ") == false);
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
