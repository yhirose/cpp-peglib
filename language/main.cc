#include "interpreter.hpp"
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

int repl(shared_ptr<Environment> env, bool print_ast)
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

int main(int argc, const char** argv)
{
    auto print_ast = false;
    auto shell = false;
    vector<const char*> path_list;

    int argi = 1;
    while (argi < argc) {
        auto arg = argv[argi++];
        if (string("--shell") == arg) {
            shell = true;
        } else if (string("--ast") == arg) {
            print_ast = true;
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

            Value val;
            string msg;
            if (!run(path, env, buff.data(), buff.size(), val, msg, print_ast)) {
                cerr << msg << endl;
                return -1;
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
