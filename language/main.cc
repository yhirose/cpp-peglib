#include "culebra.h"
#include "linenoise.hpp"
#include <fstream>
#include <iomanip>
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
    void operator()(const peglib::Ast& ast, shared_ptr<Environment> env, bool force_to_break) {
        if ((command == "n" && env->level <= level) ||
            (command == "s") ||
            (command == "o" && env->level < level)) {
            force_to_break = true;
        }

        if (force_to_break) {
            show_lines(ast);

            for (;;) {
                command = linenoise::Readline("debug> ");

                if (command == "bt") {
                    cout << "level: " << env->level << endl;
                } else if (command == "c") { // continue
                    break;
                } else if (command == "n") { // step over
                    break;
                } else if (command == "s") { // step into
                    break;
                } else if (command == "o") { // step out
                    break;
                }
            }
            level = env->level;;
        }
    }

    void show_lines(const peglib::Ast& ast) {
        cout << "break in " << ast.path << ":" << ast.line << endl;
        auto count = get_line_count(ast.path);
        auto digits = to_string(count).length();;
        size_t start = max((int)ast.line - 1, 1);
        auto end = min(ast.line + 3, count);
        for (auto l = start; l < end; l++) {
            auto s = get_line(ast.path, l);
            if (l == ast.line) {
                cout << "> ";
            } else {
                cout << "  ";
            }
            cout << setw(digits) << l << " " << s << endl;
        }
    }

    size_t get_line_count(const string& path) {
        prepare_cache(path);
        return sources[path].size();
    }

    string get_line(const string& path, size_t line) {
        prepare_cache(path);

        const auto& positions = sources[path];
        auto idx = line - 1;
        auto first = idx > 0 ? positions[idx - 1] : 0;
        auto last = positions[idx];
        auto size = last - first;

        string s(size, 0);
        ifstream ifs(path, ios::in | ios::binary);
        ifs.seekg(first, ios::beg).read((char*)s.data(), static_cast<streamsize>(s.size()));

        size_t count = 0;
        auto rit = s.rbegin();
        while (rit != s.rend()) {
            if (*rit == '\n') {
                count++;
            }
            ++rit;
        }

        s = s.substr(0, s.size() - count);

        return s;
    }

    void prepare_cache(const string& path) {
        auto it = sources.find(path);
        if (it == sources.end()) {
            vector<char> buff;
            read_file(path.c_str(), buff);

            auto& positions = sources[path];

            auto i = 0u;
            for (; i < buff.size(); i++) {
                if (buff[i] == '\n') {
                    positions.push_back(i + 1);
                }
            }
            positions.push_back(i);
        }
    }

    string command;
    size_t level = 0;
    map<string, vector<size_t>> sources;
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
            shared_ptr<peglib::Ast> ast;
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

            Value                   val;
            vector<string>          msgs;
            shared_ptr<peglib::Ast> ast;
            Debugger                dbg;

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
