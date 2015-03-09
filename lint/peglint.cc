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
    if (argc < 2 || string("--help") == argv[1]) {
        cerr << "usage: peglint [grammar file path] [source file path]" << endl;
        return 1;
    }

    // Check PEG grammar
    auto syntax_path = argv[1];

    vector<char> syntax;
    if (!read_file(syntax_path, syntax)) {
        cerr << "can't open the grammar file." << endl;
        return -1;
    }

    peglib::peg peg;

    bool firstError = true;
    peg.log = [&](size_t ln, size_t col, const string& msg) {
        if (firstError) {
            firstError = false;
            cerr << "# PEG grammar syntax error" << endl;
        }
        cerr << syntax_path << ":" << ln << ":" << col << ": " << msg << endl;
    };

    if (!peg.load_grammar(syntax.data(), syntax.size())) {
        return -1;
    }

    if (argc < 3) {
        return 0;
    }

    // Check source
    auto source_path = argv[2];

    vector<char> source;
    if (!read_file(source_path, source)) {
        cerr << "can't open the source file." << endl;
        return -1;
    }

    firstError = true;
    peg.log = [&](size_t ln, size_t col, const string& msg) {
        if (firstError) {
            firstError = false;
            cerr << "# Source syntax error" << endl;
        }
        cerr << syntax_path << ":" << ln << ":" << col << ": " << msg << endl;
    };

    if (!peg.parse_n(source.data(), source.size())) {
        return -1;
    }

    return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
