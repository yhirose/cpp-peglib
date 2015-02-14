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

//
//  PEG syntax:
//
//      EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*
//      TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*
//      FACTOR           <-  NUMBER / '(' EXPRESSION ')'
//      TERM_OPERATOR    <-  [-+]
//      FACTOR_OPERATOR  <-  [/*]
//      NUMBER           <-  [0-9]+
//
int main(int argc, const char** argv)
{
    if (argc < 2 || string("--help") == argv[1]) {
        cout << "usage: calc [formula]" << endl;
        return 1;
    }

    const char* s = argv[1];

    auto reduce = [](const vector<any>& v) -> long {
        auto result = v[0].get<long>();
        for (auto i = 1u; i < v.size(); i += 2) {
            auto num = v[i + 1].get<long>();
            auto ope = v[i].get<char>();
            switch (ope) {
                case '+': result += num; break;
                case '-': result -= num; break;
                case '*': result *= num; break;
                case '/': result /= num; break;
            }
        }
        return result;
    };

    const char* syntax =
        "  EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*        "
        "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
        "  FACTOR           <-  NUMBER / '(' EXPRESSION ')'       "
        "  TERM_OPERATOR    <-  [-+]                              "
        "  FACTOR_OPERATOR  <-  [/*]                              "
        "  NUMBER           <-  [0-9]+                            "
        ;

    peg parser(syntax);

    parser["EXPRESSION"]      = reduce;
    parser["TERM"]            = reduce;
    parser["TERM_OPERATOR"]   = [](const char* s, size_t l) { return (char)*s; };
    parser["FACTOR_OPERATOR"] = [](const char* s, size_t l) { return (char)*s; };
    parser["NUMBER"]          = [](const char* s, size_t l) { return atol(s); };

    long val = 0;
    if (parser.match(s, val)) {
        cout << s << " = " << val << endl;
        return 0;
    }

    cout << "syntax error..." << endl;

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
