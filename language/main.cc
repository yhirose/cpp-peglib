#include "culebra.h"
#include "linenoise.hpp"
#include <fstream>
#include <iostream>
#include <vector>

using namespace culebra;
using namespace std;

bool read_file(const char* path, vector<char>& buff)
{
    ifstream ifs(path, ios::in|ios::binary);
    if (ifs.fail()) {
        return false;
    }

    auto size = static_cast<unsigned int>(ifs.seekg(0, ios::end).tellg());

    if (size > 0) {
        buff.resize(size);
        ifs.seekg(0, ios::beg).read(&buff[0], static_cast<streamsize>(buff.size()));
    }

    return true;
}

struct CommandLineDebugger
{
    void operator()(const peglib::Ast& ast, std::shared_ptr<Environment> env, bool force_to_break) {
        if (quit) {
            force_to_break = false;
        } if (line == "n" && env->level <= level) {
            force_to_break = true;
        } else if (line == "s") {
            force_to_break = true;
        } else if (line == "o" && env->level < level) {
            force_to_break = true;
        }

        if (force_to_break) {
            for (;;) {
                line = linenoise::Readline("debug> ");

                if (line == "bt") {
                    std::cout << "level: " << env->level << std::endl;
                } else if (line == "l") { // show source file
                    std::cout << "line: " << ast.line << " in " << ast.path << std::endl;
                } else if (line == "c") { // continue
                    break;
                } else if (line == "n") { // step over
                    break;
                } else if (line == "s") { // step into
                    break;
                } else if (line == "o") { // step out
                    break;
                } else if (line == "q") { // quit
                    quit = true;
                    break;
                }
            }
            level = env->level;;
        }
    }

    std::string line;
    size_t      level = 0;
    bool        quit = false;
};

int repl(shared_ptr<Environment> env, bool print_ast)
{
    for (;;) {
        auto line = linenoise::Readline("cul> ");

        if (line == "exit" || line == "quit") {
            break;
        }

        if (!line.empty()) {
            Value val;
            vector<string> msgs;
            std::shared_ptr<peglib::Ast> ast;
            auto ret = run("(repl)", env, line.c_str(), line.size(), val, msgs, ast);
            if (ret) {
                if (print_ast) {
                    ast->print();
                }
                cout << val << endl;
                linenoise::AddHistory(line.c_str());
            } else if (!msgs.empty()) {
                for (const auto& msg: msgs) {
                    cout << msg << endl;;
                }
            }
        }
    }

    return 0;
}

int main(int argc, const char** argv)
{
    auto print_ast = false;
    auto shell = false;
    auto debug = false;
    vector<const char*> path_list;

    int argi = 1;
    while (argi < argc) {
        auto arg = argv[argi++];
        if (string("--shell") == arg) {
            shell = true;
        } else if (string("--ast") == arg) {
            print_ast = true;
        } else if (string("--debug") == arg) {
            debug = true;
        } else {
            path_list.push_back(arg);
        }
    }

    if (!shell) {
        shell = path_list.empty();
    }

    try {
        auto env = make_shared<Environment>();
        setup_built_in_functions(*env);

        for (auto path: path_list) {
            vector<char> buff;
            if (!read_file(path, buff)) {
                cerr << "can't open '" << path << "'." << endl;
                return -1;
            }

            Value                        val;
            vector<string>               msgs;
            std::shared_ptr<peglib::Ast> ast;
            Debugger                     dbg;

            CommandLineDebugger debugger;
            if (debug) {
                dbg = debugger;
            }

            auto ret = run(path, env, buff.data(), buff.size(), val, msgs, ast, dbg);

            if (!ret) {
                for (const auto& msg: msgs) {
                    cerr << msg << endl;
                }
                return -1;
            } else if (print_ast) {
                ast->print();
            }
        }

        if (shell) {
            repl(env, print_ast);
        }
    } catch (exception& e) {
        cerr << e.what() << endl;
        return -1;
    }

    return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
