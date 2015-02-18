//
//  peglint.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <peglib.h>
#include <iostream>
#include "mmap.h"

using namespace std;

int main(int argc, const char** argv)
{
    if (argc < 2 || string("--help") == argv[1]) {
        cerr << "usage: peglint [grammar file path] [source file path]" << endl;
        return 1;
    }

    // Check PEG grammar
    auto syntax_path = argv[1];

    MemoryMappedFile syntax(syntax_path);
    if (!syntax.is_open()) {
        cerr << "can't open the grammar file." << endl;
        return -1;
    }

    peglib::peg peg(syntax.data(), syntax.size(), [&](size_t ln, size_t col, const string& msg) {
        cerr << syntax_path << ":" << ln << ":" << col << ": " << msg << endl;
    });

    if (!peg) {
        return -1;
    }

    if (argc < 3) {
        return 0;
    }

    // Check source
    auto source_path = argv[2];

    MemoryMappedFile source(source_path);
    if (!source.is_open()) {
        cerr << "can't open the source file." << endl;
        return -1;
    }

    auto ret = peg.lint(source.data(), source.size(), true, [&](size_t ln, size_t col, const string& msg) {
        cerr << source_path << ":" << ln << ":" << col << ": " << msg << endl;
    });

    if (ret) {
        peg.parse(source.data(), source.size());
    }

    return ret ? 0 : -1;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
