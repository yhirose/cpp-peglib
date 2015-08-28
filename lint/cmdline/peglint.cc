//
//  peglint.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <fstream>

using namespace std;

bool read_file(const char* path, vector<char>& buff)
{
    ifstream ifs(path, ios::in | ios::binary);
    if (ifs.fail()) {
        return false;
    }

    buff.resize(static_cast<unsigned int>(ifs.seekg(0, ios::end).tellg()));
    if (!buff.empty()) {
        ifs.seekg(0, ios::beg).read(&buff[0], static_cast<streamsize>(buff.size()));
    }
    return true;
}

int main(int argc, const char** argv)
{
    auto opt_ast = false;
    auto opt_optimize_ast_nodes = false;
    auto opt_help = false;
    vector<const char*> path_list;

    auto argi = 1;
    while (argi < argc) {
        auto arg = argv[argi++];
        if (string("--help") == arg) {
            opt_help = true;
        } else if (string("--ast") == arg) {
            opt_ast = true;
        } else if (string("--optimize_ast_nodes") == arg || string("--opt") == arg) {
            opt_optimize_ast_nodes = true;
        } else {
            path_list.push_back(arg);
        }
    }

    if (path_list.empty() || opt_help) {
        cerr << "usage: peglint [--ast] [--optimize_ast_nodes|--opt] [grammar file path] [source file path]" << endl;
        return 1;
    }

    // Check PEG grammar
    auto syntax_path = path_list[0];

    vector<char> syntax;
    if (!read_file(syntax_path, syntax)) {
        cerr << "can't open the grammar file." << endl;
        return -1;
    }

    peg::parser parser;

    parser.log = [&](auto ln, auto col, const auto& msg) {
        cerr << syntax_path << ":" << ln << ":" << col << ": " << msg << endl;
    };

    if (!parser.load_grammar(syntax.data(), syntax.size())) {
        return -1;
    }

    if (path_list.size() < 2) {
        return 0;
    }

    // Check source
    auto source_path = path_list[1];

    vector<char> source;
    if (!read_file(source_path, source)) {
        auto beg = source_path;
        auto end = source_path + strlen(source_path);
        source.assign(beg, end);
        source_path = "[commendline]";
    }

    parser.log = [&](auto ln, auto col, const auto& msg) {
        cerr << source_path << ":" << ln << ":" << col << ": " << msg << endl;
    };

    if (opt_ast) {
	    parser.enable_ast();

	    std::shared_ptr<peg::Ast> ast;
	    if (!parser.parse_n(source.data(), source.size(), ast)) {
	        return -1;
	    }

        ast = peg::AstOptimizer(opt_optimize_ast_nodes).optimize(ast);
	    peg::print_ast(ast);
    } else {
	    if (!parser.parse_n(source.data(), source.size())) {
	        return -1;
	    }
    }

    return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
