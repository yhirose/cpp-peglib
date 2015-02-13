//
//  peglint.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <iostream>
#include "mmap.h"

using namespace peglib;
using namespace std;

int main(int argc, const char** argv)
{
    if (argc < 2 || string("--help") == argv[1]) {
        cerr << "usage: peglint [grammar file path] [source file path]" << endl;
        return 1;
    }

    // Check PEG grammar
    cerr << "checking grammar file..." << endl;

    MemoryMappedFile syntax(argv[1]);
    if (!syntax.is_open()) {
        cerr << "can't open the grammar file." << endl;
        return -1;
    }

    auto parser = make_parser(syntax.data(), syntax.size(), [](size_t ln, size_t col, const string& msg) {
        cerr << ln << ":" << col << ": " << msg << endl;
    });

    if (parser) {
        cerr << "success" << endl;
    } else {
        cerr << "invalid grammar file." << endl;
        return -1;
    }

    if (argc < 3) {
        return 0;
    }

    // Check source
    cerr << "checking source file..." << endl;

    MemoryMappedFile source(argv[2]);
    if (!source.is_open()) {
        cerr << "can't open the source file." << endl;
        return -1;
    }

    auto m = parser.lint(source.data(), source.size());

    if (m.ret) {
        cerr << "success" << endl;
    } else {
        auto info = line_info(source.data(), m.ptr);
        cerr << info.first << ":" << info.second << ": syntax error" << endl;
    }
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
