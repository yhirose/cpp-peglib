//
//  peglint.cc
//
//  Copyright (c) 2015 Yuji Hirose. All rights reserved.
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
  auto opt_ast = false;
  auto opt_optimize_ast_nodes = false;
  vector<string> optimize_ast_nodes_filters;
  auto opt_help = false;
  auto opt_source = false;
  vector<char> source;
  auto opt_trace = false;
  vector<const char *> path_list;

  auto argi = 1;
  while (argi < argc) {
    auto arg = argv[argi++];
    if (string("--help") == arg) {
      opt_help = true;
    } else if (string("--ast") == arg) {
      opt_ast = true;
    } else if (string("--opt") == arg) {
      opt_optimize_ast_nodes = true;
    } else if (string("--filters") == arg) {
      if (argi < argc) {
        std::string s = argv[argi++];
        optimize_ast_nodes_filters = split(s, ',');
      }
    } else if (string("--source") == arg) {
      opt_source = true;
      if (argi < argc) {
        std::string text = argv[argi++];
        source.assign(text.begin(), text.end());
      }
    } else if (string("--trace") == arg) {
      opt_trace = true;
    } else {
      path_list.push_back(arg);
    }
  }

  if (path_list.empty() || opt_help) {
    cerr << "usage: grammar_file_path [source_file_path]" << endl
         << endl
         << "  options:" << endl
         << "    --ast: show AST tree" << endl
         << "    --opt: optimaze AST nodes" << endl
         << "    --filters definition_names: comma delimitted definition names "
            "for optimazation"
         << endl
         << "    --source: source text" << endl
         << "    --trace: show trace messages" << endl;
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

  parser.log = [&](size_t ln, size_t col, const string &msg) {
    cerr << syntax_path << ":" << ln << ":" << col << ": " << msg << endl;
  };

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

  parser.log = [&](size_t ln, size_t col, const string &msg) {
    cerr << source_path << ":" << ln << ":" << col << ": " << msg << endl;
  };

  if (opt_trace) {
    size_t prev_pos = 0;
    parser.enable_trace(
        [&](const char *name, const char *s, size_t /*n*/,
            const peg::SemanticValues & /*sv*/, const peg::Context &c,
            const peg::any & /*dt*/) {
          auto pos = static_cast<size_t>(s - c.s);
          auto backtrack = (pos < prev_pos ? "*" : "");
          string indent;
          auto level = c.trace_ids.size() - 1;
          while (level--) {
            indent += "│";
          }
          std::cout << "E " << pos << backtrack << "\t" << indent << "┌" << name
                    << " #" << c.trace_ids.back() << std::endl;
          prev_pos = static_cast<size_t>(pos);
        },
        [&](const char *name, const char *s, size_t /*n*/,
            const peg::SemanticValues &sv, const peg::Context &c,
            const peg::any & /*dt*/, size_t len) {
          auto pos = static_cast<size_t>(s - c.s);
          if (len != static_cast<size_t>(-1)) { pos += len; }
          string indent;
          auto level = c.trace_ids.size() - 1;
          while (level--) {
            indent += "│";
          }
          auto ret = len != static_cast<size_t>(-1) ? "└o " : "└x ";
          std::stringstream choice;
          if (sv.choice_count() > 0) {
            choice << " " << sv.choice() << "/" << sv.choice_count();
          }
          std::string token;
          if (!sv.tokens.empty()) {
            const auto &tok = sv.tokens[0];
            token += " '" + std::string(tok.first, tok.second) + "'";
          }
          std::cout << "L " << pos << "\t" << indent << ret << name << " #"
                    << c.trace_ids.back() << choice.str() << token << std::endl;
        });
  }

  if (opt_ast) {
    parser.enable_ast();

    std::shared_ptr<peg::Ast> ast;
    if (!parser.parse_n(source.data(), source.size(), ast)) { return -1; }

    std::cout << "filters..." << std::endl;
    for (auto name : optimize_ast_nodes_filters) {
      std::cout << name << std::endl;
    }
    ast = peg::AstOptimizer(opt_optimize_ast_nodes, optimize_ast_nodes_filters)
              .optimize(ast);
    std::cout << peg::ast_to_s(ast);

  } else {
    if (!parser.parse_n(source.data(), source.size())) { return -1; }
  }

  return 0;
}

// vim: et ts=4 sw=4 cin cino={1s ff=unix
