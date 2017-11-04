
#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <peglib.h>
#include <iostream>

#if !defined(PEGLIB_NO_UNICODE_CHARS)
TEST_CASE("Simple syntax test (with unicode)", "[general]")
{
    peg::parser parser(
        u8" ROOT ← _ "
        " _ <- ' ' "
    );

    bool ret = parser;
    REQUIRE(ret == true);
}
#endif

TEST_CASE("Simple syntax test", "[general]")
{
    peg::parser parser(
        " ROOT <- _ "
        " _ <- ' ' "
    );

    bool ret = parser;
    REQUIRE(ret == true);
}

TEST_CASE("Empty syntax test", "[general]")
{
    peg::parser parser("");
    bool ret = parser;
    REQUIRE(ret == false);
}

TEST_CASE("String capture test", "[general]")
{
    peg::parser parser(
        "  ROOT      <-  _ ('[' TAG_NAME ']' _)*  "
        "  TAG_NAME  <-  (!']' .)+                "
        "  _         <-  [ \t]*                   "
    );

    std::vector<std::string> tags;

    parser["TAG_NAME"] = [&](const peg::SemanticValues& sv) {
        tags.push_back(sv.str());
    };

    auto ret = parser.parse(" [tag1] [tag:2] [tag-3] ");

    REQUIRE(ret == true);
    REQUIRE(tags.size() == 3);
    REQUIRE(tags[0] == "tag1");
    REQUIRE(tags[1] == "tag:2");
    REQUIRE(tags[2] == "tag-3");
}

TEST_CASE("String capture test with match", "[general]")
{
    peg::match m;
    auto ret = peg::peg_match(
        "  ROOT      <-  _ ('[' $< TAG_NAME > ']' _)*  "
        "  TAG_NAME  <-  (!']' .)+                "
        "  _         <-  [ \t]*                   ",
        " [tag1] [tag:2] [tag-3] ",
        m);

    REQUIRE(ret == true);
    REQUIRE(m.size() == 4);
    REQUIRE(m.str(1) == "tag1");
    REQUIRE(m.str(2) == "tag:2");
    REQUIRE(m.str(3) == "tag-3");
}

using namespace peg;
using namespace std;

TEST_CASE("String capture test2", "[general]")
{
    vector<string> tags;

    Definition ROOT, TAG, TAG_NAME, WS;
    ROOT     <= seq(WS, zom(TAG));
    TAG      <= seq(chr('['), TAG_NAME, chr(']'), WS);
    TAG_NAME <= oom(seq(npd(chr(']')), dot())), [&](const SemanticValues& sv) { tags.push_back(sv.str()); };
    WS       <= zom(cls(" \t"));

    auto r = ROOT.parse(" [tag1] [tag:2] [tag-3] ");

    REQUIRE(r.ret == true);
    REQUIRE(tags.size() == 3);
    REQUIRE(tags[0] == "tag1");
    REQUIRE(tags[1] == "tag:2");
    REQUIRE(tags[2] == "tag-3");
}

TEST_CASE("String capture test3", "[general]")
{
   auto syntax =
       " ROOT  <- _ TOKEN*                "
       " TOKEN <- '[' < (!']' .)+ > ']' _ "
       " _     <- [ \t\r\n]*              "
       ;

   parser pg(syntax);

   std::vector<std::string> tags;

   pg["TOKEN"] = [&](const SemanticValues& sv) {
       tags.push_back(sv.token());
   };

   auto ret = pg.parse(" [tag1] [tag:2] [tag-3] ");

   REQUIRE(ret == true);
   REQUIRE(tags.size() == 3);
   REQUIRE(tags[0] == "tag1");
   REQUIRE(tags[1] == "tag:2");
   REQUIRE(tags[2] == "tag-3");
}

TEST_CASE("Named capture test", "[general]")
{
    peg::match m;

    auto ret = peg::peg_match(
        "  ROOT      <-  _ ('[' $test< TAG_NAME > ']' _)*  "
        "  TAG_NAME  <-  (!']' .)+                "
        "  _         <-  [ \t]*                   ",
        " [tag1] [tag:2] [tag-3] ",
        m);

    auto cap = m.named_capture("test");

    REQUIRE(ret == true);
    REQUIRE(m.size() == 4);
    REQUIRE(cap.size() == 3);
    REQUIRE(m.str(cap[2]) == "tag-3");
}

TEST_CASE("String capture test with embedded match action", "[general]")
{
    Definition ROOT, TAG, TAG_NAME, WS;

    vector<string> tags;

    ROOT     <= seq(WS, zom(TAG));
    TAG      <= seq(chr('['),
                    cap(TAG_NAME, [&](const char* s, size_t n, size_t /*id*/, const std::string& /*name*/) {
                        tags.push_back(string(s, n));
                    }),
                    chr(']'),
                    WS);
    TAG_NAME <= oom(seq(npd(chr(']')), dot()));
    WS       <= zom(cls(" \t"));

    auto r = ROOT.parse(" [tag1] [tag:2] [tag-3] ");

    REQUIRE(r.ret == true);
    REQUIRE(tags.size() == 3);
    REQUIRE(tags[0] == "tag1");
    REQUIRE(tags[1] == "tag:2");
    REQUIRE(tags[2] == "tag-3");
}

TEST_CASE("Cyclic grammer test", "[general]")
{
    Definition PARENT;
    Definition CHILD;

    PARENT <= seq(CHILD);
    CHILD  <= seq(PARENT);
}

TEST_CASE("Visit test", "[general]")
{
    Definition ROOT, TAG, TAG_NAME, WS;

    ROOT     <= seq(WS, zom(TAG));
    TAG      <= seq(chr('['), TAG_NAME, chr(']'), WS);
    TAG_NAME <= oom(seq(npd(chr(']')), dot()));
    WS       <= zom(cls(" \t"));

    AssignIDToDefinition defIds;
    ROOT.accept(defIds);

    REQUIRE(defIds.ids.size() == 4);
}

TEST_CASE("Token check test", "[general]")
{
    parser parser(
        "  EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*      "
        "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
        "  FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _   "
        "  TERM_OPERATOR    <-  < [-+] > _                        "
        "  FACTOR_OPERATOR  <-  < [/*] > _                        "
        "  NUMBER           <-  < [0-9]+ > _                      "
        "  _               <-  [ \t\r\n]*                        "
        );

    REQUIRE(parser["EXPRESSION"].is_token == false);
    REQUIRE(parser["FACTOR"].is_token == false);
    REQUIRE(parser["FACTOR_OPERATOR"].is_token == true);
    REQUIRE(parser["NUMBER"].is_token == true);
    REQUIRE(parser["_"].is_token == true);
}

TEST_CASE("Lambda action test", "[general]")
{
    parser parser(
       "  START <- (CHAR)* "
       "  CHAR  <- .       ");

    string ss;
    parser["CHAR"] = [&](const SemanticValues& sv) {
        ss += *sv.c_str();
    };

    bool ret = parser.parse("hello");
    REQUIRE(ret == true);
    REQUIRE(ss == "hello");
}

TEST_CASE("enter/leave handlers test", "[general]")
{
    parser parser(R"(
        START  <- LTOKEN '=' RTOKEN
        LTOKEN <- TOKEN
        RTOKEN <- TOKEN
        TOKEN  <- [A-Za-z]+
    )");

    parser["LTOKEN"].enter = [&](any& dt) {
        auto& require_upper_case = *dt.get<bool*>();
        require_upper_case = false;
    };
    parser["LTOKEN"].leave = [&](any& dt) {
        auto& require_upper_case = *dt.get<bool*>();
        require_upper_case = true;
    };

    auto message = "should be upper case string...";

    parser["TOKEN"] = [&](const SemanticValues& sv, any& dt) {
        auto& require_upper_case = *dt.get<bool*>();
        if (require_upper_case) {
            const auto& s = sv.str();
            if (!std::all_of(s.begin(), s.end(), ::isupper)) {
                throw parse_error(message);
            }
        }
    };

    bool require_upper_case = false;
    any dt = &require_upper_case;
    REQUIRE(parser.parse("hello=world", dt) == false);
    REQUIRE(parser.parse("HELLO=world", dt) == false);
    REQUIRE(parser.parse("hello=WORLD", dt) == true);
    REQUIRE(parser.parse("HELLO=WORLD", dt) == true);

    parser.log = [&](size_t ln, size_t col, const string& msg) {
        REQUIRE(ln == 1);
        REQUIRE(col == 7);
        REQUIRE(msg == message);
    };
    parser.parse("hello=world", dt);
}

TEST_CASE("WHITESPACE test", "[general]")
{
    peg::parser parser(R"(
        # Rules
        ROOT         <-  ITEM (',' ITEM)*
        ITEM         <-  WORD / PHRASE

        # Tokens
        WORD         <-  < [a-zA-Z0-9_]+ >
        PHRASE       <-  < '"' (!'"' .)* '"' >

        %whitespace  <-  [ \t\r\n]*
    )");

    auto ret = parser.parse(R"(  one, 	 "two, three",   four  )");

    REQUIRE(ret == true);
}

TEST_CASE("WHITESPACE test2", "[general]")
{
    peg::parser parser(R"(
        # Rules
        ROOT         <-  ITEM (',' ITEM)*
        ITEM         <-  '[' < [a-zA-Z0-9_]+ > ']'

        %whitespace  <-  (SPACE / TAB)*
        SPACE        <-  ' '
        TAB          <-  '\t'
    )");

    vector<string> items;
    parser["ITEM"] = [&](const SemanticValues& sv) {
        items.push_back(sv.token());
    };

    auto ret = parser.parse(R"([one], 	[two] ,[three] )");

    REQUIRE(ret == true);
    REQUIRE(items.size() == 3);
    REQUIRE(items[0] == "one");
    REQUIRE(items[1] == "two");
    REQUIRE(items[2] == "three");
}

TEST_CASE("Skip token test", "[general]")
{
    peg::parser parser(
        "  ROOT  <-  _ ITEM (',' _ ITEM _)* "
        "  ITEM  <-  ([a-z0-9])+  "
        "  ~_    <-  [ \t]*    "
    );

    parser["ROOT"] = [&](const SemanticValues& sv) {
        REQUIRE(sv.size() == 2);
    };

    auto ret = parser.parse(" item1, item2 ");

    REQUIRE(ret == true);
}

TEST_CASE("Skip token test2", "[general]")
{
    peg::parser parser(R"(
        ROOT        <-  ITEM (',' ITEM)*
        ITEM        <-  < ([a-z0-9])+ >
        %whitespace <-  [ \t]*
    )");

    parser["ROOT"] = [&](const SemanticValues& sv) {
        REQUIRE(sv.size() == 2);
    };

    auto ret = parser.parse(" item1, item2 ");

    REQUIRE(ret == true);
}

TEST_CASE("Backtracking test", "[general]")
{
    parser parser(
       "  START <- PAT1 / PAT2  "
       "  PAT1  <- HELLO ' One' "
       "  PAT2  <- HELLO ' Two' "
       "  HELLO <- 'Hello'      "
    );

    size_t count = 0;
    parser["HELLO"] = [&](const SemanticValues& /*sv*/) {
        count++;
    };

    parser.enable_packrat_parsing();

    bool ret = parser.parse("Hello Two");
    REQUIRE(ret == true);
    REQUIRE(count == 1); // Skip second time
}

TEST_CASE("Backtracking with AST", "[general]")
{
    parser parser(R"(
        S <- A? B (A B)* A
        A <- 'a'
        B <- 'b'
    )");

    parser.enable_ast();
    shared_ptr<Ast> ast;
    bool ret = parser.parse("ba", ast);
    REQUIRE(ret == true);
    REQUIRE(ast->nodes.size() == 2);
}

TEST_CASE("Octal/Hex value test", "[general]")
{
    peg::parser parser(
        R"( ROOT <- '\132\x7a' )"
    );

    auto ret = parser.parse("Zz");

    REQUIRE(ret == true);
}

TEST_CASE("mutable lambda test", "[general]")
{
    vector<string> vec;

    parser pg("ROOT <- 'mutable lambda test'");

    // This test makes sure if the following code can be compiled.
    pg["TOKEN"] = [=](const SemanticValues& sv) mutable {
        vec.push_back(sv.str());
    };
}

TEST_CASE("Simple calculator test", "[general]")
{
    auto syntax =
        " Additive  <- Multitive '+' Additive / Multitive "
        " Multitive <- Primary '*' Multitive / Primary "
        " Primary   <- '(' Additive ')' / Number "
        " Number    <- [0-9]+ ";

    parser parser(syntax);

    parser["Additive"] = [](const SemanticValues& sv) {
        switch (sv.choice()) {
        case 0:
            return sv[0].get<int>() + sv[1].get<int>();
        default:
            return sv[0].get<int>();
        }
    };

    parser["Multitive"] = [](const SemanticValues& sv) {
        switch (sv.choice()) {
        case 0:
            return sv[0].get<int>() * sv[1].get<int>();
        default:
            return sv[0].get<int>();
        }
    };

    parser["Number"] = [](const SemanticValues& sv) {
        return atoi(sv.c_str());
    };

    int val;
    parser.parse("(1+2)*3", val);

    REQUIRE(val == 9);
}

TEST_CASE("Calculator test", "[general]")
{
    // Construct grammer
    Definition EXPRESSION, TERM, FACTOR, TERM_OPERATOR, FACTOR_OPERATOR, NUMBER;

    EXPRESSION      <= seq(TERM, zom(seq(TERM_OPERATOR, TERM)));
    TERM            <= seq(FACTOR, zom(seq(FACTOR_OPERATOR, FACTOR)));
    FACTOR          <= cho(NUMBER, seq(chr('('), EXPRESSION, chr(')')));
    TERM_OPERATOR   <= cls("+-");
    FACTOR_OPERATOR <= cls("*/");
    NUMBER          <= oom(cls("0-9"));

    // Setup actions
    auto reduce = [](const SemanticValues& sv) -> long {
        long ret = sv[0].get<long>();
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = sv[i + 1].get<long>();
            switch (sv[i].get<char>()) {
                case '+': ret += num; break;
                case '-': ret -= num; break;
                case '*': ret *= num; break;
                case '/': ret /= num; break;
            }
        }
        return ret;
    };

    EXPRESSION      = reduce;
    TERM            = reduce;
    TERM_OPERATOR   = [](const SemanticValues& sv) { return *sv.c_str(); };
    FACTOR_OPERATOR = [](const SemanticValues& sv) { return *sv.c_str(); };
    NUMBER          = [](const SemanticValues& sv) { return stol(sv.str(), nullptr, 10); };

    // Parse
    long val;
    auto r = EXPRESSION.parse_and_get_value("1+2*3*(4-5+6)/7-8", val);

    REQUIRE(r.ret == true);
    REQUIRE(val == -3);
}

TEST_CASE("Calculator test2", "[general]")
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
    auto grammar = ParserGenerator::parse(syntax, strlen(syntax), start, nullptr, nullptr);
    auto& g = *grammar;

    // Setup actions
    auto reduce = [](const SemanticValues& sv) -> long {
        long ret = sv[0].get<long>();
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = sv[i + 1].get<long>();
            switch (sv[i].get<char>()) {
                case '+': ret += num; break;
                case '-': ret -= num; break;
                case '*': ret *= num; break;
                case '/': ret /= num; break;
            }
        }
        return ret;
    };

    g["EXPRESSION"]      = reduce;
    g["TERM"]            = reduce;
    g["TERM_OPERATOR"]   = [](const SemanticValues& sv) { return *sv.c_str(); };
    g["FACTOR_OPERATOR"] = [](const SemanticValues& sv) { return *sv.c_str(); };
    g["NUMBER"]          = [](const SemanticValues& sv) { return stol(sv.str(), nullptr, 10); };

    // Parse
    long val;
    auto r = g[start].parse_and_get_value("1+2*3*(4-5+6)/7-8", val);

    REQUIRE(r.ret == true);
    REQUIRE(val == -3);
}

TEST_CASE("Calculator test3", "[general]")
{
    // Parse syntax
    parser parser(
        "  # Grammar for Calculator...\n                          "
        "  EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*        "
        "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
        "  FACTOR           <-  NUMBER / '(' EXPRESSION ')'       "
        "  TERM_OPERATOR    <-  [-+]                              "
        "  FACTOR_OPERATOR  <-  [/*]                              "
        "  NUMBER           <-  [0-9]+                            "
        );

    auto reduce = [](const SemanticValues& sv) -> long {
        long ret = sv[0].get<long>();
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = sv[i + 1].get<long>();
            switch (sv[i].get<char>()) {
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
    parser["TERM_OPERATOR"]   = [](const SemanticValues& sv) { return static_cast<char>(*sv.c_str()); };
    parser["FACTOR_OPERATOR"] = [](const SemanticValues& sv) { return static_cast<char>(*sv.c_str()); };
    parser["NUMBER"]          = [](const SemanticValues& sv) { return stol(sv.str(), nullptr, 10); };

    // Parse
    long val;
    auto ret = parser.parse("1+2*3*(4-5+6)/7-8", val);

    REQUIRE(ret == true);
    REQUIRE(val == -3);
}

TEST_CASE("Calculator test with AST", "[general]")
{
    parser parser(
        "  EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*      "
        "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
        "  FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _   "
        "  TERM_OPERATOR    <-  < [-+] > _                        "
        "  FACTOR_OPERATOR  <-  < [/*] > _                        "
        "  NUMBER           <-  < [0-9]+ > _                      "
        "  ~_               <-  [ \t\r\n]*                        "
        );

    parser.enable_ast();

    function<long (const Ast&)> eval = [&](const Ast& ast) {
        if (ast.name == "NUMBER") {
            return stol(ast.token);
        } else {
            const auto& nodes = ast.nodes;
            auto result = eval(*nodes[0]);
            for (auto i = 1u; i < nodes.size(); i += 2) {
                auto num = eval(*nodes[i + 1]);
                auto ope = nodes[i]->token[0];
                switch (ope) {
                    case '+': result += num; break;
                    case '-': result -= num; break;
                    case '*': result *= num; break;
                    case '/': result /= num; break;
                }
            }
            return result;
        }
    };

    shared_ptr<Ast> ast;
    auto ret = parser.parse("1+2*3*(4-5+6)/7-8", ast);
    ast = peg::AstOptimizer(true).optimize(ast);
    auto val = eval(*ast);

    REQUIRE(ret == true);
    REQUIRE(val == -3);
}

TEST_CASE("Ignore semantic value test", "[general]")
{
    parser parser(
       " START <-  ~HELLO WORLD "
       " HELLO <- 'Hello' _     "
       " WORLD <- 'World' _     "
       " _     <- [ \t\r\n]*    "
    );

    parser.enable_ast();

    shared_ptr<Ast> ast;
    auto ret = parser.parse("Hello World", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->nodes.size() == 1);
    REQUIRE(ast->nodes[0]->name == "WORLD");
}

TEST_CASE("Ignore semantic value of 'or' predicate test", "[general]")
{
    parser parser(
       " START       <- _ !DUMMY HELLO_WORLD '.' "
       " HELLO_WORLD <- HELLO 'World' _          "
       " HELLO       <- 'Hello' _                "
       " DUMMY       <- 'dummy' _                "
       " ~_          <- [ \t\r\n]*               "
    );

    parser.enable_ast();

    shared_ptr<Ast> ast;
    auto ret = parser.parse("Hello World.", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->nodes.size() == 1);
    REQUIRE(ast->nodes[0]->name == "HELLO_WORLD");
}

TEST_CASE("Ignore semantic value of 'and' predicate test", "[general]")
{
    parser parser(
       " START       <- _ &HELLO HELLO_WORLD '.' "
       " HELLO_WORLD <- HELLO 'World' _          "
       " HELLO       <- 'Hello' _                "
       " ~_          <- [ \t\r\n]*               "
    );

    parser.enable_ast();

    shared_ptr<Ast> ast;
    auto ret = parser.parse("Hello World.", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->nodes.size() == 1);
    REQUIRE(ast->nodes[0]->name == "HELLO_WORLD");
}

TEST_CASE("Literal token on AST test1", "[general]")
{
    parser parser(R"(
        STRING_LITERAL  <- '"' (('\\"' / '\\t' / '\\n') / (!["] .))* '"'
    )");
    parser.enable_ast();

    shared_ptr<Ast> ast;
    auto ret = parser.parse(R"("a\tb")", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->is_token == true);
    REQUIRE(ast->token == R"("a\tb")");
    REQUIRE(ast->nodes.empty());
}

TEST_CASE("Literal token on AST test2", "[general]")
{
    parser parser(R"(
        STRING_LITERAL  <-  '"' (ESC / CHAR)* '"' 
        ESC             <-  ('\\"' / '\\t' / '\\n')
        CHAR            <-  (!["] .)
    )");
    parser.enable_ast();

    shared_ptr<Ast> ast;
    auto ret = parser.parse(R"("a\tb")", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->is_token == false);
    REQUIRE(ast->token.empty());
    REQUIRE(ast->nodes.size() == 3);
}

TEST_CASE("Literal token on AST test3", "[general]")
{
    parser parser(R"(
        STRING_LITERAL  <-  < '"' (ESC / CHAR)* '"' >
        ESC             <-  ('\\"' / '\\t' / '\\n')
        CHAR            <-  (!["] .)
    )");
    parser.enable_ast();

    shared_ptr<Ast> ast;
    auto ret = parser.parse(R"("a\tb")", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->is_token == true);
    REQUIRE(ast->token == R"("a\tb")");
    REQUIRE(ast->nodes.empty());
}

TEST_CASE("Missing missing definitions test", "[general]")
{
    parser parser(
        " A <- B C "
    );

    REQUIRE(parser == false);
}

TEST_CASE("Definition duplicates test", "[general]")
{
    parser parser(
        " A <- ''"
        " A <- ''"
    );

    REQUIRE(parser == false);
}

TEST_CASE("Left recursive test", "[left recursive]")
{
    parser parser(
        " A <- A 'a'"
        " B <- A 'a'"
    );

    REQUIRE(parser == false);
}

TEST_CASE("Left recursive with option test", "[left recursive]")
{
    parser parser(
        " A  <- 'a' / 'b'? B 'c' "
        " B  <- A                "
    );

    REQUIRE(parser == false);
}

TEST_CASE("Left recursive with zom test", "[left recursive]")
{
    parser parser(
        " A <- 'a'* A* "
    );

    REQUIRE(parser == false);
}

TEST_CASE("Left recursive with empty string test", "[left recursive]")
{
    parser parser(
        " A <- '' A"
    );

    REQUIRE(parser == false);
}

TEST_CASE("Semantic predicate test", "[predicate]")
{
    parser parser("NUMBER  <-  [0-9]+");

    parser["NUMBER"] = [](const SemanticValues& sv) {
        auto val = stol(sv.token(), nullptr, 10);
        if (val != 100) {
            throw parse_error("value error!!");
        }
        return val;
    };

    long val;
    auto ret = parser.parse("100", val);
    REQUIRE(ret == true);
    REQUIRE(val == 100);

    ret = parser.parse("200", val);
    REQUIRE(ret == false);
}

TEST_CASE("Japanese character", "[unicode]")
{
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

    auto ret = parser.parse(u8R"(サーバーを復旧します。)");

    REQUIRE(ret == true);
}

TEST_CASE("Line information test", "[line information]")
{
    parser parser(R"(
        S    <- _ (WORD _)+
        WORD <- [A-Za-z]+
        ~_   <- [ \t\r\n]+
    )");

    std::vector<std::pair<int, int>> locations;
    parser["WORD"] = [&](const peg::SemanticValues& sv) {
        locations.push_back(sv.line_info());
    };

    bool ret = parser;
    REQUIRE(ret == true);

    ret = parser.parse(" Mon Tue Wed \nThu  Fri  Sat\nSun\n");
    REQUIRE(ret == true);

    REQUIRE(locations[0] == std::make_pair(1, 2));
    REQUIRE(locations[1] == std::make_pair(1, 6));
    REQUIRE(locations[2] == std::make_pair(1, 10));
    REQUIRE(locations[3] == std::make_pair(2, 1));
    REQUIRE(locations[4] == std::make_pair(2, 6));
    REQUIRE(locations[5] == std::make_pair(2, 11));
    REQUIRE(locations[6] == std::make_pair(3, 1));
}

bool exact(Grammar& g, const char* d, const char* s) {
    auto n = strlen(s);
    auto r = g[d].parse(s, n);
    return r.ret && r.len == n;
}

Grammar& make_peg_grammar() {
    return ParserGenerator::grammar();
}

TEST_CASE("PEG Grammar", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Grammar", " Definition <- a / ( b c ) / d \n rule2 <- [a-zA-Z][a-z0-9-]+ ") == true);
}

TEST_CASE("PEG Definition", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Definition", "Definition <- a / (b c) / d ") == true);
    REQUIRE(exact(g, "Definition", "Definition <- a / b c / d ") == true);
    REQUIRE(exact(g, "Definition", u8"Definitiond ← a ") == true);
    REQUIRE(exact(g, "Definition", "Definition ") == false);
    REQUIRE(exact(g, "Definition", " ") == false);
    REQUIRE(exact(g, "Definition", "") == false);
    REQUIRE(exact(g, "Definition", "Definition = a / (b c) / d ") == false);
}

TEST_CASE("PEG Expression", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Expression", "a / (b c) / d ") == true);
    REQUIRE(exact(g, "Expression", "a / b c / d ") == true);
    REQUIRE(exact(g, "Expression", "a b ") == true);
    REQUIRE(exact(g, "Expression", "") == true);
    REQUIRE(exact(g, "Expression", " ") == false);
    REQUIRE(exact(g, "Expression", " a b ") == false);
}

TEST_CASE("PEG Sequence", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Sequence", "a b c d ") == true);
    REQUIRE(exact(g, "Sequence", "") == true);
    REQUIRE(exact(g, "Sequence", "!") == false);
    REQUIRE(exact(g, "Sequence", "<-") == false);
    REQUIRE(exact(g, "Sequence", " a") == false);
}

TEST_CASE("PEG Prefix", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Prefix", "&[a]") == true);
    REQUIRE(exact(g, "Prefix", "![']") == true);
    REQUIRE(exact(g, "Prefix", "-[']") == false);
    REQUIRE(exact(g, "Prefix", "") == false);
    REQUIRE(exact(g, "Prefix", " a") == false);
}

TEST_CASE("PEG Suffix", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Suffix", "aaa ") == true);
    REQUIRE(exact(g, "Suffix", "aaa? ") == true);
    REQUIRE(exact(g, "Suffix", "aaa* ") == true);
    REQUIRE(exact(g, "Suffix", "aaa+ ") == true);
    REQUIRE(exact(g, "Suffix", ". + ") == true);
    REQUIRE(exact(g, "Suffix", "?") == false);
    REQUIRE(exact(g, "Suffix", "") == false);
    REQUIRE(exact(g, "Suffix", " a") == false);
}

TEST_CASE("PEG Primary", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Primary", "_Identifier0_ ") == true);
    REQUIRE(exact(g, "Primary", "_Identifier0_<-") == false);
    REQUIRE(exact(g, "Primary", "( _Identifier0_ _Identifier1_ )") == true);
    REQUIRE(exact(g, "Primary", "'Literal String'") == true);
    REQUIRE(exact(g, "Primary", "\"Literal String\"") == true);
    REQUIRE(exact(g, "Primary", "[a-zA-Z]") == true);
    REQUIRE(exact(g, "Primary", ".") == true);
    REQUIRE(exact(g, "Primary", "") == false);
    REQUIRE(exact(g, "Primary", " ") == false);
    REQUIRE(exact(g, "Primary", " a") == false);
    REQUIRE(exact(g, "Primary", "") == false);
}

TEST_CASE("PEG Identifier", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Identifier", "_Identifier0_ ") == true);
    REQUIRE(exact(g, "Identifier", "0Identifier_ ") == false);
    REQUIRE(exact(g, "Identifier", "Iden|t ") == false);
    REQUIRE(exact(g, "Identifier", " ") == false);
    REQUIRE(exact(g, "Identifier", " a") == false);
    REQUIRE(exact(g, "Identifier", "") == false);
}

TEST_CASE("PEG IdentStart", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "IdentStart", "_") == true);
    REQUIRE(exact(g, "IdentStart", "a") == true);
    REQUIRE(exact(g, "IdentStart", "Z") == true);
    REQUIRE(exact(g, "IdentStart", "") == false);
    REQUIRE(exact(g, "IdentStart", " ") == false);
    REQUIRE(exact(g, "IdentStart", "0") == false);
}

TEST_CASE("PEG IdentRest", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "IdentRest", "_") == true);
    REQUIRE(exact(g, "IdentRest", "a") == true);
    REQUIRE(exact(g, "IdentRest", "Z") == true);
    REQUIRE(exact(g, "IdentRest", "") == false);
    REQUIRE(exact(g, "IdentRest", " ") == false);
    REQUIRE(exact(g, "IdentRest", "0") == true);
}

TEST_CASE("PEG Literal", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Literal", "'abc' ") == true);
    REQUIRE(exact(g, "Literal", "'a\\nb\\tc' ") == true);
    REQUIRE(exact(g, "Literal", "'a\\277\tc' ") == true);
    REQUIRE(exact(g, "Literal", "'a\\77\tc' ") == true);
    REQUIRE(exact(g, "Literal", "'a\\80\tc' ") == false);
    REQUIRE(exact(g, "Literal", "'\n' ") == true);
    REQUIRE(exact(g, "Literal", "'a\\'b' ") == true);
    REQUIRE(exact(g, "Literal", "'a'b' ") == false);
    REQUIRE(exact(g, "Literal", "'a\"'b' ") == false);
    REQUIRE(exact(g, "Literal", "\"'\\\"abc\\\"'\" ") == true);
    REQUIRE(exact(g, "Literal", "\"'\"abc\"'\" ") == false);
    REQUIRE(exact(g, "Literal", "abc") == false);
    REQUIRE(exact(g, "Literal", "") == false);
    REQUIRE(exact(g, "Literal", u8"日本語") == false);
}

TEST_CASE("PEG Class", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Class", "[]") == true);
    REQUIRE(exact(g, "Class", "[a]") == true);
    REQUIRE(exact(g, "Class", "[a-z]") == true);
    REQUIRE(exact(g, "Class", "[az]") == true);
    REQUIRE(exact(g, "Class", "[a-zA-Z-]") == true);
    REQUIRE(exact(g, "Class", "[a-zA-Z-0-9]") == true);
    REQUIRE(exact(g, "Class", "[a-]") == false);
    REQUIRE(exact(g, "Class", "[-a]") == true);
    REQUIRE(exact(g, "Class", "[") == false);
    REQUIRE(exact(g, "Class", "[a") == false);
    REQUIRE(exact(g, "Class", "]") == false);
    REQUIRE(exact(g, "Class", "a]") == false);
    REQUIRE(exact(g, "Class", u8"あ-ん") == false);
    REQUIRE(exact(g, "Class", "[-+]") == true);
    REQUIRE(exact(g, "Class", "[+-]") == false);
}

TEST_CASE("PEG Range", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Range", "a") == true);
    REQUIRE(exact(g, "Range", "a-z") == true);
    REQUIRE(exact(g, "Range", "az") == false);
    REQUIRE(exact(g, "Range", "") == false);
    REQUIRE(exact(g, "Range", "a-") == false);
    REQUIRE(exact(g, "Range", "-a") == false);
}

TEST_CASE("PEG Char", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Char", "\\n") == true);
    REQUIRE(exact(g, "Char", "\\r") == true);
    REQUIRE(exact(g, "Char", "\\t") == true);
    REQUIRE(exact(g, "Char", "\\'") == true);
    REQUIRE(exact(g, "Char", "\\\"") == true);
    REQUIRE(exact(g, "Char", "\\[") == true);
    REQUIRE(exact(g, "Char", "\\]") == true);
    REQUIRE(exact(g, "Char", "\\\\") == true);
    REQUIRE(exact(g, "Char", "\\000") == true);
    REQUIRE(exact(g, "Char", "\\377") == true);
    REQUIRE(exact(g, "Char", "\\477") == false);
    REQUIRE(exact(g, "Char", "\\087") == false);
    REQUIRE(exact(g, "Char", "\\079") == false);
    REQUIRE(exact(g, "Char", "\\00") == true);
    REQUIRE(exact(g, "Char", "\\77") == true);
    REQUIRE(exact(g, "Char", "\\80") == false);
    REQUIRE(exact(g, "Char", "\\08") == false);
    REQUIRE(exact(g, "Char", "\\0") == true);
    REQUIRE(exact(g, "Char", "\\7") == true);
    REQUIRE(exact(g, "Char", "\\8") == false);
    REQUIRE(exact(g, "Char", "a") == true);
    REQUIRE(exact(g, "Char", ".") == true);
    REQUIRE(exact(g, "Char", "0") == true);
    REQUIRE(exact(g, "Char", "\\") == false);
    REQUIRE(exact(g, "Char", " ") == true);
    REQUIRE(exact(g, "Char", "  ") == false);
    REQUIRE(exact(g, "Char", "") == false);
    REQUIRE(exact(g, "Char", u8"あ") == false);
}

TEST_CASE("PEG Operators", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "LEFTARROW", "<-") == true);
    REQUIRE(exact(g, "SLASH", "/ ") == true);
    REQUIRE(exact(g, "AND", "& ") == true);
    REQUIRE(exact(g, "NOT", "! ") == true);
    REQUIRE(exact(g, "QUESTION", "? ") == true);
    REQUIRE(exact(g, "STAR", "* ") == true);
    REQUIRE(exact(g, "PLUS", "+ ") == true);
    REQUIRE(exact(g, "OPEN", "( ") == true);
    REQUIRE(exact(g, "CLOSE", ") ") == true);
    REQUIRE(exact(g, "DOT", ". ") == true);
}

TEST_CASE("PEG Comment", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Comment", "# Comment.\n") == true);
    REQUIRE(exact(g, "Comment", "# Comment.") == false);
    REQUIRE(exact(g, "Comment", " ") == false);
    REQUIRE(exact(g, "Comment", "a") == false);
}

TEST_CASE("PEG Space", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "Space", " ") == true);
    REQUIRE(exact(g, "Space", "\t") == true);
    REQUIRE(exact(g, "Space", "\n") == true);
    REQUIRE(exact(g, "Space", "") == false);
    REQUIRE(exact(g, "Space", "a") == false);
}

TEST_CASE("PEG EndOfLine", "[peg]")
{
    auto g = ParserGenerator::grammar();
    REQUIRE(exact(g, "EndOfLine", "\r\n") == true);
    REQUIRE(exact(g, "EndOfLine", "\n") == true);
    REQUIRE(exact(g, "EndOfLine", "\r") == true);
    REQUIRE(exact(g, "EndOfLine", " ") == false);
    REQUIRE(exact(g, "EndOfLine", "") == false);
    REQUIRE(exact(g, "EndOfLine", "a") == false);
}

TEST_CASE("PEG EndOfFile", "[peg]")
{
    Grammar g = make_peg_grammar();
    REQUIRE(exact(g, "EndOfFile", "") == true);
    REQUIRE(exact(g, "EndOfFile", " ") == false);
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
