//
//  calc2.cc
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
        EXPRESSION      = seq(TERM, zom(seq(TERM_OPERATOR, TERM)));
        TERM            = seq(FACTOR, zom(seq(FACTOR_OPERATOR, FACTOR)));
        FACTOR          = cho(NUMBER, seq(chr('('), EXPRESSION, chr(')')));
        TERM_OPERATOR   = cls("+-");
        FACTOR_OPERATOR = cls("*/");
        NUMBER          = oom(cls("0-9"));

        actions[EXPRESSION]      = reduce;
        actions[TERM]            = reduce;
        actions[TERM_OPERATOR]   = [](const char* s, size_t l) { return (char)*s; };
        actions[FACTOR_OPERATOR] = [](const char* s, size_t l) { return (char)*s; };
        actions[NUMBER]          = [](const char* s, size_t l) { return stol(string(s, l), nullptr, 10); };
    }

    bool execute(const char* s, long& v) const {
        Any val;
        auto ret = EXPRESSION.parse(s, actions, val);
        if (ret) {
            v = val.get<long>();
        }
        return ret;
    }

private:
    Definition EXPRESSION, TERM, FACTOR, TERM_OPERATOR, FACTOR_OPERATOR, NUMBER;
    SemanticActions<Any> actions;

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
