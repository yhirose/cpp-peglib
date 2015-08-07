//
//  calc.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <iostream>
#include <cstdlib>

using namespace peglib;
using namespace std;

int main(int argc, const char** argv)
{
    if (argc < 2 || string("--help") == argv[1]) {
        cout << "usage: calc [formula]" << endl;
        return 1;
    }

    auto reduce = [](const SemanticValues& sv) -> long {
        auto result = sv[0].get<long>();
        for (auto i = 1u; i < sv.size(); i += 2) {
            auto num = sv[i + 1].get<long>();
            auto ope = sv[i].get<char>();
            switch (ope) {
                case '+': result += num; break;
                case '-': result -= num; break;
                case '*': result *= num; break;
                case '/': result /= num; break;
            }
        }
        return result;
    };

    peg parser(R"(
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
    parser["TERM_OPERATOR"]   = [](const SemanticValues& sv) { return (char)*sv.s; };
    parser["FACTOR_OPERATOR"] = [](const SemanticValues& sv) { return (char)*sv.s; };
    parser["NUMBER"]          = [](const SemanticValues& sv) { return atol(sv.s); };

    auto expr = argv[1];
    long val = 0;
    if (parser.parse(expr, val)) {
        cout << expr << " = " << val << endl;
        return 0;
    }

    cout << "syntax error..." << endl;

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
