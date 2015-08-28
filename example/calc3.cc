//
//  calc3.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <iostream>
#include <cstdlib>

using namespace peg;
using namespace std;

int main(int argc, const char** argv)
{
    if (argc < 2 || string("--help") == argv[1]) {
        cout << "usage: calc3 [formula]" << endl;
        return 1;
    }

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

    auto expr = argv[1];
    shared_ptr<Ast> ast;
    if (parser.parse(expr, ast)) {
        ast = AstOptimizer(true).optimize(ast);
        print_ast(ast);
        cout << expr << " = " << eval(*ast) << endl;
        return 0;
    }

    cout << "syntax error..." << endl;

    return -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
