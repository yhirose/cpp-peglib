#include "../peglib.h"
#include <cstdio>
#include <emscripten/bind.h>
#include <functional>
#include <iomanip>
#include <sstream>

// https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c/33799784#33799784
std::string escape_json(const std::string &s) {
  std::ostringstream o;
  for (auto c : s) {
    if (c == '"' || c == '\\' || ('\x00' <= c && c <= '\x1f')) {
      o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
    } else {
      o << c;
    }
  }
  return o.str();
}

std::function<void(size_t, size_t, const std::string &, const std::string &)>
makeJSONFormatter(peg::parser &peg, std::string &json, bool &init) {
  init = true;
  return [&](size_t ln, size_t col, const std::string &msg,
             const std::string &rule) mutable {
    if (!init) { json += ","; }
    json += "{";
    json += R"("ln":)" + std::to_string(ln) + ",";
    json += R"("col":)" + std::to_string(col) + ",";
    json += R"("msg":")" + escape_json(msg) + R"(")";
    if (!rule.empty()) {
      auto it = peg.get_grammar().find(rule);
      if (it != peg.get_grammar().end()) {
        auto [gln, gcol] = it->second.line_;
        json += ",";
        json += R"("gln":)" + std::to_string(gln) + ",";
        json += R"("gcol":)" + std::to_string(gcol);
      }
    }
    json += "}";

    init = false;
  };
}

bool parse_grammar(const std::string &text, peg::parser &peg,
                   const std::string &startRule, std::string &json) {
  bool init;
  peg.set_logger(makeJSONFormatter(peg, json, init));
  json += "[";
  auto ret = peg.load_grammar(text.data(), text.size(), startRule);
  json += "]";
  return ret;
}

bool parse_code(const std::string &text, peg::parser &peg, std::string &json,
                std::shared_ptr<peg::Ast> &ast) {
  peg.enable_ast();
  bool init;
  peg.set_logger(makeJSONFormatter(peg, json, init));
  json += "[";
  auto ret = peg.parse_n(text.data(), text.size(), ast);
  json += "]";
  return ret;
}

std::string lint(const std::string &grammarText, const std::string &codeText,
                 bool opt_mode, bool packrat, const std::string &startRule) {
  std::string grammarResult;
  std::string codeResult;
  std::string astResult;
  std::string astResultOptimized;
  std::string profileResult;

  peg::parser peg;
  auto is_grammar_valid =
      parse_grammar(grammarText, peg, startRule, grammarResult);
  auto is_source_valid = false;

  if (is_grammar_valid && peg) {
    std::stringstream ss;
    peg::enable_profiling(peg, ss);

    if (packrat) { peg.enable_packrat_parsing(); }

    std::shared_ptr<peg::Ast> ast;
    is_source_valid = parse_code(codeText, peg, codeResult, ast);

    profileResult = escape_json(ss.str());

    if (ast) {
      astResult = escape_json(peg::ast_to_s(ast));
      astResultOptimized =
          escape_json(peg::ast_to_s(peg.optimize_ast(ast, opt_mode)));
    }
  }

  std::string json;
  json += "{";
  json +=
      std::string("\"grammar_valid\":") + (is_grammar_valid ? "true" : "false");
  json += ",\"grammar\":" + grammarResult;
  json +=
      std::string(",\"source_valid\":") + (is_source_valid ? "true" : "false");
  if (!codeResult.empty()) {
    json += ",\"code\":" + codeResult;
    json += ",\"ast\":\"" + astResult + "\"";
    json += ",\"astOptimized\":\"" + astResultOptimized + "\"";
    json += ",\"profile\":\"" + profileResult + "\"";
  }
  json += "}";

  return json;
}

EMSCRIPTEN_BINDINGS(native) { emscripten::function("lint", &lint); }
