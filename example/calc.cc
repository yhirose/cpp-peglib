//
//  calc.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <iostream>
#include <cstdlib>

using namespace peg;

int main(int argc, const char** argv)
{
    if (argc < 2 || std::string("--help") == argv[1]) {
        std::cout << "usage: calc [formula]" << std::endl;
        return 1;
    }

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

    parser parser(R"(
        EXPRESSION       <-  _ TERM (TERM_OPERATOR TERM)*
        TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
        FACTOR           <-  NUMBER / '(' _ EXPRESSION ')' _
        TERM_OPERATOR    <-  < [-+] > _
        FACTOR_OPERATOR  <-  < [/*] > _
        NUMBER           <-  < [0-9]+ > _
        ~_               <-  [ \t\r\n]*
    )");

    parser["EXPRESSION"]      = reduce;
    parser["TERM"]            = reduce;
    parser["TERM_OPERATOR"]   = [](const SemanticValues& sv) { return static_cast<char>(*sv.c_str()); };
    parser["FACTOR_OPERATOR"] = [](const SemanticValues& sv) { return static_cast<char>(*sv.c_str()); };
    parser["NUMBER"]          = [](const SemanticValues& sv) { return atol(sv.c_str()); };

    auto expr = argv[1];
    long val = 0;
    if (parser.parse(expr, val)) {
        std::cout << expr << " = " << val << std::endl;
        return 0;
    }

    std::cout << "syntax error..." << std::endl;

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
