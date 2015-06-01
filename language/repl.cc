#include "linenoise.hpp"
#include "repl.hpp"
#include <iostream>

using namespace std;

int repl(shared_ptr<Environment>& env, bool print_ast)
{
    for (;;) {
        auto line = linenoise::Readline("cul> ");

        if (line == "exit" || line == "quit") {
            break;
        }

        if (!line.empty()) {
            Value val;
            string msg;
            if (run("(repl)", env, line.c_str(), line.size(), val, msg, print_ast)) {
                cout << val << endl;
                linenoise::AddHistory(line.c_str());
            } else if (!msg.empty()) {
                cout << msg;
            }
        }
    }

    return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
