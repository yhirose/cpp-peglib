//
//  calc.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <iostream>
#include <map>

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
class Calculator
{
public:
    Calculator() {
        const char* syntax =
            "  EXPRESSION       <-  TERM (TERM_OPERATOR TERM)*        "
            "  TERM             <-  FACTOR (FACTOR_OPERATOR FACTOR)*  "
            "  FACTOR           <-  NUMBER / '(' EXPRESSION ')'       "
            "  TERM_OPERATOR    <-  [-+]                              "
            "  FACTOR_OPERATOR  <-  [/*]                              "
            "  NUMBER           <-  [0-9]+                            "
            ;

        parser.load_syntax(syntax);

        parser["EXPRESSION"]      = reduce;
        parser["TERM"]            = reduce;
        parser["TERM_OPERATOR"]   = [](const char* s, size_t l) { return (char)*s; };
        parser["FACTOR_OPERATOR"] = [](const char* s, size_t l) { return (char)*s; };
        parser["NUMBER"]          = [](const char* s, size_t l) { return stol(string(s, l), nullptr, 10); };
    }

    bool execute(const char* s, long& v) const {
        return parser.parse(s, v);
    }

private:
    Parser parser;

    static long reduce(const vector<Any>& v) {
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
    }
};

int main(int argc, const char** argv)
{
    if (argc < 2 || string("--help") == argv[1]) {
        cout << "usage: calc [formula]" << endl;
        return 1;
    }

    const char* s = argv[1];

    Calculator calc;

    long val = 0;
    if (calc.execute(s, val)) {
        cout << s << " = " << val << endl;
        return 0;
    }

    cout << "syntax error..." << endl;

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
