
#define CATCH_CONFIG_MAIN
#include "catch.hh"

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

TEST_CASE("Action taking non const Semantic Values parameter", "[general]")
{
    peg::parser parser(R"(
        ROOT <- TEXT
        TEXT <- [a-zA-Z]+
    )");

    parser["ROOT"] = [&](peg::SemanticValues& sv) {
        auto s = peg::any_cast<std::string>(sv[0]);
        s[0] = 'H'; // mutate
        return std::string(std::move(s)); // move
    };

    parser["TEXT"] = [&](peg::SemanticValues& sv) {
        return sv.token();
    };

    std::string val;
    auto ret = parser.parse("hello", val);
    REQUIRE(ret == true);
    REQUIRE(val == "Hello");
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

using namespace peg;

TEST_CASE("String capture test2", "[general]")
{
    std::vector<std::string> tags;

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
    parser pg(R"(
        ROOT  <- _ TOKEN*
        TOKEN <- '[' < (!']' .)+ > ']' _
        _     <- [ \t\r\n]*
    )");


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
    parser parser(R"(
        EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _
        TERM_OPERATOR    <-  < [-+] > _
        FACTOR_OPERATOR  <-  < [/*] > _
        NUMBER           <-  < [0-9]+ > _
        _                <-  [ \t\r\n]*
    )");

    REQUIRE(parser["EXPRESSION"].is_token() == false);
    REQUIRE(parser["FACTOR"].is_token() == false);
    REQUIRE(parser["FACTOR_OPERATOR"].is_token() == true);
    REQUIRE(parser["NUMBER"].is_token() == true);
    REQUIRE(parser["_"].is_token() == true);
}

TEST_CASE("Lambda action test", "[general]")
{
    parser parser(R"(
       START <- (CHAR)*
       CHAR  <- .
    )");

    std::string ss;
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

    parser["LTOKEN"].enter = [&](const char*, size_t, any& dt) {
        auto& require_upper_case = *any_cast<bool*>(dt);
        require_upper_case = false;
    };
    parser["LTOKEN"].leave = [&](const char*, size_t, size_t, any&, any& dt) {
        auto& require_upper_case = *any_cast<bool*>(dt);
        require_upper_case = true;
    };

    auto message = "should be upper case string...";

    parser["TOKEN"] = [&](const SemanticValues& sv, any& dt) {
        auto& require_upper_case = *any_cast<bool*>(dt);
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

    parser.log = [&](size_t ln, size_t col, const std::string& msg) {
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

    std::vector<std::string> items;
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

TEST_CASE("WHITESPACE test3", "[general]") {
    peg::parser parser(R"(
        StrQuot      <- < '"' < (StrEscape / StrChars)* > '"' >
        StrEscape    <- '\\' any
        StrChars     <- (!'"' !'\\' any)+
        any          <- .
        %whitespace  <- [ \t]*
    )");

    parser["StrQuot"] = [](const SemanticValues& sv) {
        REQUIRE(sv.token() == R"(  aaa \" bbb  )");
    };

    auto ret = parser.parse(R"( "  aaa \" bbb  " )");
    REQUIRE(ret == true);
}

TEST_CASE("WHITESPACE test4", "[general]") {
    peg::parser parser(R"(
        ROOT         <-  HELLO OPE WORLD
        HELLO        <-  'hello'
        OPE          <-  < [-+] >
        WORLD        <-  'world' / 'WORLD'
        %whitespace  <-  [ \t\r\n]*
    )");

    parser["HELLO"] = [](const SemanticValues& sv) {
        REQUIRE(sv.token() == "hello");
    };

    parser["OPE"] = [](const SemanticValues& sv) {
        REQUIRE(sv.token() == "+");
    };

    parser["WORLD"] = [](const SemanticValues& sv) {
        REQUIRE(sv.token() == "world");
    };

    auto ret = parser.parse("  hello + world  ");
    REQUIRE(ret == true);
}

TEST_CASE("Word expression test", "[general]") {
    peg::parser parser(R"(
        ROOT         <-  'hello' ','? 'world'
        %whitespace  <-  [ \t\r\n]*
        %word        <-  [a-z]+
    )");

	REQUIRE(parser.parse("helloworld") == false);
	REQUIRE(parser.parse("hello world") == true);
	REQUIRE(parser.parse("hello,world") == true);
	REQUIRE(parser.parse("hello, world") == true);
	REQUIRE(parser.parse("hello , world") == true);
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
    peg::parser parser(R"(
       START <- PAT1 / PAT2
       PAT1  <- HELLO ' One'
       PAT2  <- HELLO ' Two'
       HELLO <- 'Hello'
    )");

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
    std::shared_ptr<Ast> ast;
    bool ret = parser.parse("ba", ast);
    REQUIRE(ret == true);
    REQUIRE(ast->nodes.size() == 2);
}

TEST_CASE("Octal/Hex/Unicode value test", "[general]")
{
    peg::parser parser(
        R"( ROOT <- '\132\x7a\u30f3' )"
    );

    auto ret = parser.parse("Zzン");

    REQUIRE(ret == true);
}

TEST_CASE("Ignore case test", "[general]") {
    peg::parser parser(R"(
        ROOT         <-  HELLO WORLD
        HELLO        <-  'hello'i
        WORLD        <-  'world'i
        %whitespace  <-  [ \t\r\n]*
    )");

    parser["HELLO"] = [](const SemanticValues& sv) {
        REQUIRE(sv.token() == "Hello");
    };

    parser["WORLD"] = [](const SemanticValues& sv) {
        REQUIRE(sv.token() == "World");
    };

    auto ret = parser.parse("  Hello World  ");
    REQUIRE(ret == true);
}

TEST_CASE("mutable lambda test", "[general]")
{
    std::vector<std::string> vec;

    parser pg("ROOT <- 'mutable lambda test'");

    // This test makes sure if the following code can be compiled.
    pg["TOKEN"] = [=](const SemanticValues& sv) mutable {
        vec.push_back(sv.str());
    };
}

TEST_CASE("Simple calculator test", "[general]")
{
    parser parser(R"(
        Additive  <- Multitive '+' Additive / Multitive
        Multitive <- Primary '*' Multitive / Primary
        Primary   <- '(' Additive ')' / Number
        Number    <- [0-9]+
    )");

    parser["Additive"] = [](const SemanticValues& sv) {
        switch (sv.choice()) {
        case 0:
            return any_cast<int>(sv[0]) + any_cast<int>(sv[1]);
        default:
            return any_cast<int>(sv[0]);
        }
    };

    parser["Multitive"] = [](const SemanticValues& sv) {
        switch (sv.choice()) {
        case 0:
            return any_cast<int>(sv[0]) * any_cast<int>(sv[1]);
        default:
            return any_cast<int>(sv[0]);
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
        long ret = any_cast<long>(sv[0]);
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = any_cast<long>(sv[i + 1]);
            switch (any_cast<char>(sv[i])) {
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
    auto syntax = R"(
        # Grammar for Calculator...
        EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' EXPRESSION ')'
        TERM_OPERATOR    <-  [-+]
        FACTOR_OPERATOR  <-  [/*]
        NUMBER           <-  [0-9]+
    )";

    std::string start;
    auto grammar = ParserGenerator::parse(syntax, strlen(syntax), start, nullptr);
    auto& g = *grammar;

    // Setup actions
    auto reduce = [](const SemanticValues& sv) -> long {
        long ret = any_cast<long>(sv[0]);
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = any_cast<long>(sv[i + 1]);
            switch (any_cast<char>(sv[i])) {
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
    parser parser(R"(
        # Grammar for Calculator...
        EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' EXPRESSION ')'
        TERM_OPERATOR    <-  [-+]
        FACTOR_OPERATOR  <-  [/*]
        NUMBER           <-  [0-9]+
    )");

    auto reduce = [](const SemanticValues& sv) -> long {
        long ret = any_cast<long>(sv[0]);
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = any_cast<long>(sv[i + 1]);
            switch (any_cast<char>(sv[i])) {
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
    parser parser(R"(
        EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _
        TERM_OPERATOR    <-  < [-+] > _
        FACTOR_OPERATOR  <-  < [/*] > _
        NUMBER           <-  < [0-9]+ > _
        ~_               <-  [ \t\r\n]*
    )");

    parser.enable_ast();

    std::function<long (const Ast&)> eval = [&](const Ast& ast) {
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

    std::shared_ptr<Ast> ast;
    auto ret = parser.parse("1+2*3*(4-5+6)/7-8", ast);
    ast = peg::AstOptimizer(true).optimize(ast);
    auto val = eval(*ast);

    REQUIRE(ret == true);
    REQUIRE(val == -3);
}

TEST_CASE("Ignore semantic value test", "[general]")
{
    parser parser(R"(
       START <-  ~HELLO WORLD
       HELLO <- 'Hello' _
       WORLD <- 'World' _
       _     <- [ \t\r\n]*
    )");

    parser.enable_ast();

    std::shared_ptr<Ast> ast;
    auto ret = parser.parse("Hello World", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->nodes.size() == 1);
    REQUIRE(ast->nodes[0]->name == "WORLD");
}

TEST_CASE("Ignore semantic value of 'or' predicate test", "[general]")
{
    parser parser(R"(
       START       <- _ !DUMMY HELLO_WORLD '.'
       HELLO_WORLD <- HELLO 'World' _
       HELLO       <- 'Hello' _
       DUMMY       <- 'dummy' _
       ~_          <- [ \t\r\n]*
   )");

    parser.enable_ast();

    std::shared_ptr<Ast> ast;
    auto ret = parser.parse("Hello World.", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->nodes.size() == 1);
    REQUIRE(ast->nodes[0]->name == "HELLO_WORLD");
}

TEST_CASE("Ignore semantic value of 'and' predicate test", "[general]")
{
    parser parser(R"(
       START       <- _ &HELLO HELLO_WORLD '.'
       HELLO_WORLD <- HELLO 'World' _
       HELLO       <- 'Hello' _
       ~_          <- [ \t\r\n]*
    )");

    parser.enable_ast();

    std::shared_ptr<Ast> ast;
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

    std::shared_ptr<Ast> ast;
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

    std::shared_ptr<Ast> ast;
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

    std::shared_ptr<Ast> ast;
    auto ret = parser.parse(R"("a\tb")", ast);

    REQUIRE(ret == true);
    REQUIRE(ast->is_token == true);
    REQUIRE(ast->token == R"("a\tb")");
    REQUIRE(ast->nodes.empty());
}

TEST_CASE("Missing missing definitions test", "[general]")
{
    parser parser(R"(
        A <- B C
    )");

    REQUIRE(!parser);
}

TEST_CASE("Definition duplicates test", "[general]")
{
    parser parser(R"(
        A <- ''
        A <- ''
    )");

    REQUIRE(!parser);
}

TEST_CASE("Semantic values test", "[general]")
{
    parser parser(R"(
        term <- ( a b c x )? a b c
        a <- 'a'
        b <- 'b'
        c <- 'c'
        x <- 'x'
    )");

	for (const auto& rule: parser.get_rule_names()){
		parser[rule.c_str()] = [rule](const SemanticValues& sv, any&) {
            if (rule == "term") {
                REQUIRE(any_cast<std::string>(sv[0]) == "a at 0");
                REQUIRE(any_cast<std::string>(sv[1]) == "b at 1");
                REQUIRE(any_cast<std::string>(sv[2]) == "c at 2");
                return std::string();
            } else {
                return rule + " at " + std::to_string(sv.c_str() - sv.ss);
            }
		};
	}

	REQUIRE(parser.parse("abc"));
}

TEST_CASE("Ordered choice count", "[general]")
{
    parser parser(R"(
        S <- 'a' / 'b'
    )");

    parser["S"] = [](const SemanticValues& sv) {
        REQUIRE(sv.choice() == 1);
        REQUIRE(sv.choice_count() == 2);
    };

    parser.parse("b");
}

TEST_CASE("Ordered choice count 2", "[general]")
{
    parser parser(R"(
        S <- ('a' / 'b')*
    )");

    parser["S"] = [](const SemanticValues& sv) {
        REQUIRE(sv.choice() == 0);
        REQUIRE(sv.choice_count() == 0);
    };

    parser.parse("b");
}

TEST_CASE("Semantic value tag", "[general]")
{
    parser parser(R"(
        S <- A? B* C?
        A <- 'a'
        B <- 'b'
        C <- 'c'
    )");

    {
        using namespace udl;
        parser["S"] = [](const SemanticValues& sv) {
            REQUIRE(sv.size() == 1);
            REQUIRE(sv.tags.size() == 1);
            REQUIRE(sv.tags[0] == "C"_);
        };
        auto ret = parser.parse("c");
        REQUIRE(ret == true);
    }

    {
        using namespace udl;
        parser["S"] = [](const SemanticValues& sv) {
            REQUIRE(sv.size() == 2);
            REQUIRE(sv.tags.size() == 2);
            REQUIRE(sv.tags[0] == "B"_);
            REQUIRE(sv.tags[1] == "B"_);
        };
        auto ret = parser.parse("bb");
        REQUIRE(ret == true);
    }

    {
        using namespace udl;
        parser["S"] = [](const SemanticValues& sv) {
            REQUIRE(sv.size() == 2);
            REQUIRE(sv.tags.size() == 2);
            REQUIRE(sv.tags[0] == "A"_);
            REQUIRE(sv.tags[1] == "C"_);
        };
        auto ret = parser.parse("ac");
        REQUIRE(ret == true);
    }
}

TEST_CASE("Packrat parser test with %whitespace%", "[packrat]")
{
    peg::parser parser(R"(
        ROOT         <-  'a'
        %whitespace  <-  SPACE*
        SPACE        <-  ' '
    )");

    parser.enable_packrat_parsing();

    auto ret = parser.parse("a");
    REQUIRE(ret == true);
}

TEST_CASE("Packrat parser test with macro", "[packrat]")
{
    parser parser(R"(
        EXPRESSION       <-  _ LIST(TERM, TERM_OPERATOR)
        TERM             <-  LIST(FACTOR, FACTOR_OPERATOR)
        FACTOR           <-  NUMBER / T('(') EXPRESSION T(')')
        TERM_OPERATOR    <-  T([-+])
        FACTOR_OPERATOR  <-  T([/*])
        NUMBER           <-  T([0-9]+)
		~_               <-  [ \t]*
		LIST(I, D)       <-  I (D I)*
		T(S)             <-  < S > _
	)");

    parser.enable_packrat_parsing();

    auto ret = parser.parse(" 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ");
    REQUIRE(ret == true);
}

TEST_CASE("Backreference test", "[backreference]")
{
    parser parser(R"(
        START  <- _ LQUOTE < (!RQUOTE .)* > RQUOTE _
        LQUOTE <- 'R"' $delm< [a-zA-Z]* > '('
        RQUOTE <- ')' $delm '"'
        ~_     <- [ \t\r\n]*
    )");

    std::string token;
    parser["START"] = [&](const SemanticValues& sv) {
        token = sv.token();
    };

    {
        token.clear();
        auto ret = parser.parse(R"delm(
            R"("hello world")"
        )delm");

        REQUIRE(ret == true);
        REQUIRE(token == "\"hello world\"");
    }

    {
        token.clear();
        auto ret = parser.parse(R"delm(
            R"foo("(hello world)")foo"
        )delm");

        REQUIRE(ret == true);
        REQUIRE(token == "\"(hello world)\"");
    }

    {
        token.clear();
        auto ret = parser.parse(R"delm(
            R"foo("(hello world)foo")foo"
        )delm");

        REQUIRE(ret == false);
        REQUIRE(token == "\"(hello world");
    }

    {
        token.clear();
        auto ret = parser.parse(R"delm(
            R"foo("(hello world)")bar"
        )delm");

        REQUIRE(ret == false);
        REQUIRE(token.empty());
    }
}

TEST_CASE("Invalid backreference test", "[backreference]")
{
    parser parser(R"(
        START  <- _ LQUOTE (!RQUOTE .)* RQUOTE _
        LQUOTE <- 'R"' $delm< [a-zA-Z]* > '('
        RQUOTE <- ')' $delm2 '"'
        ~_     <- [ \t\r\n]*
    )");

    REQUIRE_THROWS_AS(
        parser.parse(R"delm(
            R"foo("(hello world)")foo"
        )delm"),
        std::runtime_error);
}


TEST_CASE("Nested capture test", "[backreference]")
{
    parser parser(R"(
        ROOT      <- CONTENT
        CONTENT   <- (ELEMENT / TEXT)*
        ELEMENT   <- $(STAG CONTENT ETAG)
        STAG      <- '<' $tag< TAG_NAME > '>'
        ETAG      <- '</' $tag '>'
        TAG_NAME  <- 'b' / 'u'
        TEXT      <- TEXT_DATA
        TEXT_DATA <- ![<] .
    )");

    REQUIRE(parser.parse("This is <b>a <u>test</u> text</b>."));
    REQUIRE(!parser.parse("This is <b>a <u>test</b> text</u>."));
    REQUIRE(!parser.parse("This is <b>a <u>test text</b>."));
    REQUIRE(!parser.parse("This is a <u>test</u> text</b>."));
}

TEST_CASE("Backreference with Prioritized Choice test", "[backreference]")
{
    parser parser(R"(
        TREE           <- WRONG_BRANCH / CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

    REQUIRE_THROWS_AS(parser.parse("branchthatiscorrect"), std::runtime_error);
}

TEST_CASE("Backreference with Zero or More test", "[backreference]")
{
    parser parser(R"(
        TREE           <- WRONG_BRANCH* CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

    REQUIRE(parser.parse("branchthatiswrongbranchthatiscorrect"));
    REQUIRE(!parser.parse("branchthatiswrongbranchthatIscorrect"));
    REQUIRE(!parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
    REQUIRE(parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
    REQUIRE_THROWS_AS(parser.parse("branchthatiscorrect"), std::runtime_error);
    REQUIRE_THROWS_AS(parser.parse("branchthatiswron_branchthatiscorrect"), std::runtime_error);
}

TEST_CASE("Backreference with One or More test", "[backreference]")
{
    parser parser(R"(
        TREE           <- WRONG_BRANCH+ CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

    REQUIRE(parser.parse("branchthatiswrongbranchthatiscorrect"));
    REQUIRE(!parser.parse("branchthatiswrongbranchthatIscorrect"));
    REQUIRE(!parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
    REQUIRE(parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
    REQUIRE(!parser.parse("branchthatiscorrect"));
    REQUIRE(!parser.parse("branchthatiswron_branchthatiscorrect"));
}

TEST_CASE("Backreference with Option test", "[backreference]")
{
    parser parser(R"(
        TREE           <- WRONG_BRANCH? CORRECT_BRANCH
        WRONG_BRANCH   <- BRANCH THAT IS_capture WRONG
        CORRECT_BRANCH <- BRANCH THAT IS_backref CORRECT
        BRANCH         <- 'branch'
        THAT           <- 'that'
        IS_capture     <- $ref<..>
        IS_backref     <- $ref
        WRONG          <- 'wrong'
        CORRECT        <- 'correct'
    )");

    REQUIRE(parser.parse("branchthatiswrongbranchthatiscorrect"));
    REQUIRE(!parser.parse("branchthatiswrongbranchthatIscorrect"));
    REQUIRE(!parser.parse("branchthatiswrongbranchthatIswrongbranchthatiscorrect"));
    REQUIRE(!parser.parse("branchthatiswrongbranchthatIswrongbranchthatIscorrect"));
    REQUIRE_THROWS_AS(parser.parse("branchthatiscorrect"), std::runtime_error);
    REQUIRE_THROWS_AS(parser.parse("branchthatiswron_branchthatiscorrect"), std::runtime_error);
}

TEST_CASE("Left recursive test", "[left recursive]")
{
    parser parser(R"(
        A <- A 'a'
        B <- A 'a'
    )");

    REQUIRE(!parser);
}

TEST_CASE("Left recursive with option test", "[left recursive]")
{
    parser parser(R"(
        A  <- 'a' / 'b'? B 'c'
        B  <- A
    )");

    REQUIRE(!parser);
}

TEST_CASE("Left recursive with zom test", "[left recursive]")
{
    parser parser(R"(
        A <- 'a'* A*
    )");

    REQUIRE(!parser);
}

TEST_CASE("Left recursive with a ZOM content rule", "[left recursive]")
{
    parser parser(R"(
        A <- B
        B <- _ A
        _ <- ' '* # Zero or more
    )");

    REQUIRE(!parser);
}

TEST_CASE("Left recursive with empty string test", "[left recursive]")
{
    parser parser(
        " A <- '' A"
    );

    REQUIRE(!parser);
}

TEST_CASE("User defined rule test", "[user rule]")
{
    auto g = parser(R"(
        ROOT <- _ 'Hello' _ NAME '!' _
    )",
    {
        {
            "NAME", usr([](const char* s, size_t n, SemanticValues& /*sv*/, any& /*dt*/) -> size_t {
                static std::vector<std::string> names = { "PEG", "BNF" };
                for (const auto& name: names) {
                    if (name.size() <= n && !name.compare(0, name.size(), s, name.size())) {
                        return name.size();
                    }
                }
                return static_cast<size_t>(-1);
            })
        },
        {
            "~_", zom(cls(" \t\r\n"))
        }
    });

    REQUIRE(g.parse(" Hello BNF! ") == true);
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
    REQUIRE(parser.parse("100", val));
    REQUIRE(val == 100);

    REQUIRE(!parser.parse("200", val));
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

    bool ret = parser;
    REQUIRE(ret == true);

    REQUIRE(parser.parse(u8R"(サーバーを復旧します。)"));
}

TEST_CASE("dot with a code", "[unicode]")
{
    peg::parser parser(" S <- 'a' . 'b' ");
    REQUIRE(parser.parse(u8R"(aあb)"));
}

TEST_CASE("dot with a char", "[unicode]")
{
    peg::parser parser(" S <- 'a' . 'b' ");
    REQUIRE(parser.parse(u8R"(aåb)"));
}

TEST_CASE("character class", "[unicode]")
{
    peg::parser parser(R"(
        S <- 'a' [い-おAさC-Eた-とは] 'b'
    )");

    bool ret = parser;
    REQUIRE(ret == true);

    REQUIRE(!parser.parse(u8R"(aあb)"));
    REQUIRE(parser.parse(u8R"(aいb)"));
    REQUIRE(parser.parse(u8R"(aうb)"));
    REQUIRE(parser.parse(u8R"(aおb)"));
    REQUIRE(!parser.parse(u8R"(aかb)"));
    REQUIRE(parser.parse(u8R"(aAb)"));
    REQUIRE(!parser.parse(u8R"(aBb)"));
    REQUIRE(parser.parse(u8R"(aEb)"));
    REQUIRE(!parser.parse(u8R"(aFb)"));
    REQUIRE(!parser.parse(u8R"(aそb)"));
    REQUIRE(parser.parse(u8R"(aたb)"));
    REQUIRE(parser.parse(u8R"(aちb)"));
    REQUIRE(parser.parse(u8R"(aとb)"));
    REQUIRE(!parser.parse(u8R"(aなb)"));
    REQUIRE(parser.parse(u8R"(aはb)"));
    REQUIRE(!parser.parse(u8R"(a?b)"));
}

#if 0 // TODO: Unicode Grapheme support
TEST_CASE("dot with a grapheme", "[unicode]")
{
    peg::parser parser(" S <- 'a' . 'b' ");
    REQUIRE(parser.parse(u8R"(aसिb)"));
}
#endif

TEST_CASE("Macro simple test", "[macro]")
{
    parser parser(R"(
		S     <- HELLO WORLD
		HELLO <- T('hello')
		WORLD <- T('world')
		T(a)  <- a [ \t]*
	)");

    REQUIRE(parser.parse("hello \tworld "));
}

TEST_CASE("Macro two parameters", "[macro]")
{
    parser parser(R"(
		S           <- HELLO_WORLD
		HELLO_WORLD <- T('hello', 'world')
		T(a, b)     <- a [ \t]* b [ \t]*
	)");

    REQUIRE(parser.parse("hello \tworld "));
}

TEST_CASE("Macro syntax error", "[macro]")
{
    parser parser(R"(
		S     <- T('hello')
		T (a) <- a [ \t]*
	)");

    bool ret = parser;
    REQUIRE(ret == false);
}

TEST_CASE("Macro missing argument", "[macro]")
{
    parser parser(R"(
		S       <- T ('hello')
		T(a, b) <- a [ \t]* b
	)");

    bool ret = parser;
    REQUIRE(ret == false);
}

TEST_CASE("Macro reference syntax error", "[macro]")
{
    parser parser(R"(
		S    <- T ('hello')
		T(a) <- a [ \t]*
	)");

    bool ret = parser;
    REQUIRE(ret == false);
}

TEST_CASE("Macro invalid macro reference error", "[macro]")
{
    parser parser(R"(
		S <- T('hello')
		T <- 'world'
	)");

    bool ret = parser;
    REQUIRE(ret == false);
}

TEST_CASE("Macro calculator", "[macro]")
{
	// Create a PEG parser
    parser parser(R"(
        # Grammar for simple calculator...
        EXPRESSION       <-  _ LIST(TERM, TERM_OPERATOR)
        TERM             <-  LIST(FACTOR, FACTOR_OPERATOR)
        FACTOR           <-  NUMBER / T('(') EXPRESSION T(')')
        TERM_OPERATOR    <-  T([-+])
        FACTOR_OPERATOR  <-  T([/*])
        NUMBER           <-  T([0-9]+)
		~_               <-  [ \t]*
		LIST(I, D)       <-  I (D I)*
		T(S)             <-  < S > _
	)");

	// Setup actions
    auto reduce = [](const SemanticValues& sv) -> long {
        auto result = any_cast<long>(sv[0]);
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = any_cast<long>(sv[i + 1]);
            auto ope = any_cast<char>(sv[i]);
            switch (ope) {
                case '+': result += num; break;
                case '-': result -= num; break;
                case '*': result *= num; break;
                case '/': result /= num; break;
            }
        }
        return result;
    };

    parser["EXPRESSION"]      = reduce;
    parser["TERM"]            = reduce;
    parser["TERM_OPERATOR"]   = [](const SemanticValues& sv) { return static_cast<char>(*sv.c_str()); };
    parser["FACTOR_OPERATOR"] = [](const SemanticValues& sv) { return static_cast<char>(*sv.c_str()); };
    parser["NUMBER"]          = [](const SemanticValues& sv) { return atol(sv.c_str()); };

    bool ret = parser;
    REQUIRE(ret == true);

	auto expr = " 1 + 2 * 3 * (4 - 5 + 6) / 7 - 8 ";
    long val = 0;
    ret = parser.parse(expr, val);

    REQUIRE(ret == true);
    REQUIRE(val == -3);
}

TEST_CASE("Macro expression arguments", "[macro]")
{
    parser parser(R"(
		S             <- M('hello' / 'Hello', 'world' / 'World')
		M(arg0, arg1) <- arg0 [ \t]+ arg1
	)");

    REQUIRE(parser.parse("Hello world"));
}

TEST_CASE("Macro recursive", "[macro]")
{
    parser parser(R"(
		S    <- M('abc')
		M(s) <- !s / s ' ' M(s / '123') / s
	)");

    REQUIRE(parser.parse(""));
    REQUIRE(parser.parse("abc"));
    REQUIRE(parser.parse("abc abc"));
    REQUIRE(parser.parse("abc 123 abc"));
}

TEST_CASE("Macro recursive2", "[macro]")
{
	auto syntaxes = std::vector<const char*>{
		"S <- M('abc') M(s) <- !s / s ' ' M(s* '-' '123') / s",
		"S <- M('abc') M(s) <- !s / s ' ' M(s+ '-' '123') / s",
		"S <- M('abc') M(s) <- !s / s ' ' M(s? '-' '123') / s",
		"S <- M('abc') M(s) <- !s / s ' ' M(&s s+ '-' '123') / s",
		"S <- M('abc') M(s) <- !s / s ' ' M(s '-' !s '123') / s",
		"S <- M('abc') M(s) <- !s / s ' ' M(< s > '-' '123') / s",
		"S <- M('abc') M(s) <- !s / s ' ' M(~s '-' '123') / s",
	};

	for (const auto& syntax: syntaxes) {
        parser parser(syntax);
        REQUIRE(parser.parse("abc abc-123"));
	}
}

TEST_CASE("Macro exclusive modifiers", "[macro]")
{
    parser parser(R"(
		S                   <- Modifiers(!"") _
		Modifiers(Appeared) <- (!Appeared) (
								   Token('public') Modifiers(Appeared / 'public') /
								   Token('static') Modifiers(Appeared / 'static') /
								   Token('final') Modifiers(Appeared / 'final') /
								   "")
		Token(t)            <- t _
		_                   <- [ \t\r\n]*
	)");

	REQUIRE(parser.parse("public"));
	REQUIRE(parser.parse("static"));
	REQUIRE(parser.parse("final"));
	REQUIRE(parser.parse("public static final"));
	REQUIRE(!parser.parse("public public"));
	REQUIRE(!parser.parse("public static public"));
}

TEST_CASE("Macro token check test", "[macro]")
{
    parser parser(R"(
        # Grammar for simple calculator...
        EXPRESSION       <-  _ LIST(TERM, TERM_OPERATOR)
        TERM             <-  LIST(FACTOR, FACTOR_OPERATOR)
        FACTOR           <-  NUMBER / T('(') EXPRESSION T(')')
        TERM_OPERATOR    <-  T([-+])
        FACTOR_OPERATOR  <-  T([/*])
        NUMBER           <-  T([0-9]+)
		~_               <-  [ \t]*
		LIST(I, D)       <-  I (D I)*
		T(S)             <-  < S > _
	)");

    REQUIRE(parser["EXPRESSION"].is_token() == false);
    REQUIRE(parser["TERM"].is_token() == false);
    REQUIRE(parser["FACTOR"].is_token() == false);
    REQUIRE(parser["FACTOR_OPERATOR"].is_token() == true);
    REQUIRE(parser["NUMBER"].is_token() == true);
    REQUIRE(parser["_"].is_token() == true);
    REQUIRE(parser["LIST"].is_token() == false);
    REQUIRE(parser["T"].is_token() == true);
}

TEST_CASE("Macro rule-parameter collision", "[macro]")
{
    parser parser(R"(
        A    <- B(C)
        B(D) <- D
        C    <- 'c'
        D    <- 'd'
	)");

    REQUIRE(parser.parse("c"));
}

TEST_CASE("Line information test", "[line information]")
{
    parser parser(R"(
        S    <- _ (WORD _)+
        WORD <- [A-Za-z]+
        ~_   <- [ \t\r\n]+
    )");

    std::vector<std::pair<size_t, size_t>> locations;
    parser["WORD"] = [&](const peg::SemanticValues& sv) {
        locations.push_back(sv.line_info());
    };

    bool ret = parser;
    REQUIRE(ret == true);

    ret = parser.parse(" Mon Tue Wed \nThu  Fri  Sat\nSun\n");
    REQUIRE(ret == true);

    REQUIRE(locations[0] == std::make_pair<size_t, size_t>(1, 2));
    REQUIRE(locations[1] == std::make_pair<size_t, size_t>(1, 6));
    REQUIRE(locations[2] == std::make_pair<size_t, size_t>(1, 10));
    REQUIRE(locations[3] == std::make_pair<size_t, size_t>(2, 1));
    REQUIRE(locations[4] == std::make_pair<size_t, size_t>(2, 6));
    REQUIRE(locations[5] == std::make_pair<size_t, size_t>(2, 11));
    REQUIRE(locations[6] == std::make_pair<size_t, size_t>(3, 1));
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
	REQUIRE(exact(g, "Definition", "Macro(param) <- a ") == true);
	REQUIRE(exact(g, "Definition", "Macro (param) <- a ") == false);
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
    REQUIRE(exact(g, "Literal", u8"'日本語'") == true);
    REQUIRE(exact(g, "Literal", u8"\"日本語\"") == true);
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
    REQUIRE(exact(g, "Class", u8"[あ-ん]") == true);
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
    REQUIRE(exact(g, "Char", u8"あ") == true);
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
