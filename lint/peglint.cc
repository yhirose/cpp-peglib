//
//  peglint.cc
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <fstream>
#include <peglib.h>
#include <sstream>

using namespace std;

inline bool read_file(const char *path, vector<char> &buff) {
  ifstream ifs(path, ios::in | ios::binary);
  if (ifs.fail()) { return false; }

  buff.resize(static_cast<unsigned int>(ifs.seekg(0, ios::end).tellg()));
  if (!buff.empty()) {
    ifs.seekg(0, ios::beg).read(&buff[0], static_cast<streamsize>(buff.size()));
  }
  return true;
}

inline vector<string> split(const string &s, char delim) {
  vector<string> elems;
  stringstream ss(s);
  string elem;
  while (getline(ss, elem, delim)) {
    elems.push_back(elem);
  }
  return elems;
}

int main(int argc, const char **argv) {
  auto opt_packrat = false;
  auto opt_ast = false;
  auto opt_optimize = false;
  auto opt_mode = true;
  auto opt_help = false;
  auto opt_source = false;
  vector<char> source;
  auto opt_trace = false;
  auto opt_verbose = false;
  auto opt_profile = false;
  vector<const char *> path_list;

  auto argi = 1;
  while (argi < argc) {
    auto arg = argv[argi++];
    if (string("--help") == arg) {
      opt_help = true;
    } else if (string("--packrat") == arg) {
      opt_packrat = true;
    } else if (string("--ast") == arg) {
      opt_ast = true;
    } else if (string("--opt") == arg || string("--opt-all") == arg) {
      opt_optimize = true;
      opt_mode = true;
    } else if (string("--opt-only") == arg) {
      opt_optimize = true;
      opt_mode = false;
    } else if (string("--source") == arg) {
      opt_source = true;
      if (argi < argc) {
        std::string text = argv[argi++];
        source.assign(text.begin(), text.end());
      }
    } else if (string("--trace") == arg) {
      opt_trace = true;
    } else if (string("--profile") == arg) {
      opt_profile = true;
    } else if (string("--verbose") == arg) {
      opt_verbose = true;
    } else {
      path_list.push_back(arg);
    }
  }

  if (path_list.empty() || opt_help) {
    cerr << R"(usage: grammar_file_path [source_file_path]

  options:
    --source: source text
    --packrat: enable packrat memoise
    --ast: show AST tree
    --opt, --opt-all: optimize all AST nodes except nodes selected with `no_ast_opt` instruction
    --opt-only: optimize only AST nodes selected with `no_ast_opt` instruction
    --trace: show concise trace messages
    --profile: show profile report
    --verbose: verbose output for trace and profile
)";

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

  parser.set_logger([&](size_t ln, size_t col, const string &msg) {
    cerr << syntax_path << ":" << ln << ":" << col << ": " << msg << endl;
  });

  if (!parser.load_grammar(syntax.data(), syntax.size())) { return -1; }

  if (path_list.size() < 2 && !opt_source) { return 0; }

  // Check source
  std::string source_path = "[commandline]";
  if (path_list.size() >= 2) {
    if (!read_file(path_list[1], source)) {
      cerr << "can't open the code file." << endl;
      return -1;
    }
    source_path = path_list[1];
  }

  parser.set_logger([&](size_t ln, size_t col, const string &msg) {
    cerr << source_path << ":" << ln << ":" << col << ": " << msg << endl;
  });

  if (opt_packrat) { parser.enable_packrat_parsing(); }

  if (opt_trace) { enable_tracing(parser, std::cout); }

  if (opt_profile) { enable_profiling(parser, std::cout); }

  parser.set_verbose_trace(opt_verbose);

  if (opt_ast) {
    parser.enable_ast();

    std::shared_ptr<peg::Ast> ast;
    auto ret = parser.parse_n(source.data(), source.size(), ast);

    if (ast) {
      if (opt_optimize) { ast = parser.optimize_ast(ast, opt_mode); }
      std::cout << peg::ast_to_s(ast);
    }

    if (!ret) { return -1; }
  } else {
    if (!parser.parse_n(source.data(), source.size())) { return -1; }
  }

  return 0;
}
