//
//  peglint.cc
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#include <fstream>
#include <iostream>
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

  {
    using namespace peg;
    auto &grammar = parser.get_grammar();
    const char *source_start = syntax.data();

    // Get the first Reference name from an Ope tree
    struct GetFirstRef : public Ope::Visitor {
      using Ope::Visitor::visit;
      string name; // empty if not found
      void visit(Reference &ope) override {
        if (name.empty() && ope.rule_ && !ope.is_macro_) { name = ope.name_; }
      }
      void visit(Sequence &ope) override {
        if (name.empty() && !ope.opes_.empty()) { ope.opes_[0]->accept(*this); }
      }
      void visit(Repetition &ope) override {
        if (name.empty()) { ope.ope_->accept(*this); }
      }
      void visit(CaptureScope &ope) override {
        if (name.empty()) { ope.ope_->accept(*this); }
      }
      void visit(Capture &ope) override {
        if (name.empty()) { ope.ope_->accept(*this); }
      }
      void visit(TokenBoundary &ope) override {
        if (name.empty()) { ope.ope_->accept(*this); }
      }
      void visit(Ignore &ope) override {
        if (name.empty()) { ope.ope_->accept(*this); }
      }
    };

    // Build first-reference chain: all rules reachable by following
    // the first Reference of each definition recursively.
    map<string, set<string>> chain_cache;
    auto build_chain = [&](const string &start) -> const set<string> & {
      auto cached = chain_cache.find(start);
      if (cached != chain_cache.end()) { return cached->second; }
      auto &chain = chain_cache[start];
      string cur = start;
      while (!cur.empty() && !chain.count(cur)) {
        chain.insert(cur);
        auto it = grammar.find(cur);
        if (it == grammar.end()) break;
        GetFirstRef vis;
        it->second.get_core_operator()->accept(vis);
        cur = vis.name;
      }
      return chain;
    };

    auto warn = [&](const Definition &def, const string &msg) {
      auto pos = line_info(source_start, def.s_);
      cerr << syntax_path << ":" << pos.first << ":" << pos.second << ": "
           << msg << endl;
    };

    for (auto &[rule_name, def] : grammar) {
      auto core = def.get_core_operator();
      auto *choice = dynamic_cast<PrioritizedChoice *>(core.get());
      if (!choice) continue;

      // Collect first-ref info for each alternative
      struct AltInfo {
        string direct_ref;
        const set<string> *chain = nullptr;
      };
      vector<AltInfo> alts;
      for (auto &ope : choice->opes_) {
        GetFirstRef vis;
        ope->accept(vis);
        AltInfo info;
        if (!vis.name.empty()) {
          info.direct_ref = vis.name;
          info.chain = &build_chain(vis.name);
        }
        alts.push_back(std::move(info));
      }

      // Direct common prefix
      map<string, vector<size_t>> direct;
      for (size_t i = 0; i < alts.size(); i++) {
        if (!alts[i].direct_ref.empty()) {
          direct[alts[i].direct_ref].push_back(i);
        }
      }
      for (auto &[prefix, idx] : direct) {
        if (idx.size() < 2) continue;

        warn(def, "'" + rule_name + "' has " + to_string(idx.size()) +
                      " alternatives starting with '" + prefix +
                      "'; consider left-factoring.");
      }

      // Indirect common prefix
      map<string, vector<size_t>> indirect;
      for (size_t i = 0; i < alts.size(); i++) {
        if (!alts[i].chain) continue;
        for (auto &ref : *alts[i].chain) {
          indirect[ref].push_back(i);
        }
      }
      for (auto &[ref, idx] : indirect) {
        if (idx.size() < 2) continue;
        if (direct.count(ref) && direct[ref].size() >= 2) continue;
        vector<string> names;
        for (auto i : idx) {
          if (!alts[i].direct_ref.empty()) {
            names.push_back(alts[i].direct_ref);
          }
        }
        sort(names.begin(), names.end());
        names.erase(unique(names.begin(), names.end()), names.end());
        if (names.size() < 2) continue;

        string affected;
        for (auto &n : names) {
          if (!affected.empty()) affected += ", ";
          affected += n;
        }
        warn(def, "'" + rule_name + "' alternatives {" + affected +
                      "} share indirect prefix '" + ref +
                      "'; consider left-factoring.");
      }
    }
  }

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
