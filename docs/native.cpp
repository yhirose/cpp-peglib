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

std::function<void(size_t, size_t, const std::string &)>
makeJSONFormatter(std::string &json, bool &init) {
  init = true;
  return [&](size_t ln, size_t col, const std::string &msg) mutable {
    if (!init) { json += ","; }
    json += "{";
    json += R"("ln":)" + std::to_string(ln) + ",";
    json += R"("col":)" + std::to_string(col) + ",";
    json += R"("msg":")" + escape_json(msg) + R"(")";
    json += "}";

    init = false;
  };
}

bool parse_grammar(const std::string &text, peg::parser &peg,
                   std::string &json) {
  bool init;
  peg.log = makeJSONFormatter(json, init);
  json += "[";
  auto ret = peg.load_grammar(text.data(), text.size());
  json += "]";
  return ret;
}

bool parse_code(const std::string &text, peg::parser &peg, std::string &json,
                std::shared_ptr<peg::Ast> &ast) {
  peg.enable_ast();
  bool init;
  peg.log = makeJSONFormatter(json, init);
  json += "[";
  auto ret = peg.parse_n(text.data(), text.size(), ast);
  json += "]";
  return ret;
}

std::string lint(const std::string &grammarText, const std::string &codeText,
		bool opt_mode, bool opt_profile, bool opt_packrat) {
  std::string grammarResult;
  std::string codeResult;
  std::string astResult;
  std::string astResultOptimized;

  peg::parser peg;
  auto is_grammar_valid = parse_grammar(grammarText, peg, grammarResult);
  auto is_source_valid = false;

  struct StatsItem {
    std::string name;
    size_t success;
    size_t fail;
  };
  std::vector<StatsItem> stats;
  std::map<std::string, size_t> stats_index;
  size_t stats_item_total = 0;

  if (is_grammar_valid && peg) {
    std::shared_ptr<peg::Ast> ast;

  if (opt_packrat) { peg.enable_packrat_parsing(); }
  if (opt_profile) {
    peg.enable_trace(
        [&](auto &ope, auto, auto, auto &, auto &, auto &) {
          auto holder = dynamic_cast<const peg::Holder*>(&ope);
          if (holder) {
            auto &name = holder->name();
            if (stats_index.find(name) == stats_index.end()) {
              stats_index[name] = stats_index.size();
              stats.push_back({name, 0, 0});
            }
            stats_item_total++;
          }
        },
        [&](auto &ope, auto, auto, auto &, auto &, auto &, auto len) {
          auto holder = dynamic_cast<const peg::Holder*>(&ope);
          if (holder) {
            auto &name = holder->name();
            auto index = stats_index[name];
            auto &stat = stats[index];
            if (len != static_cast<size_t>(-1)) {
              stat.success++;
            } else {
              stat.fail++;
            }
          }
        },
        true);
  }

    is_source_valid = parse_code(codeText, peg, codeResult, ast);
    if (ast) {
      astResult = escape_json(peg::ast_to_s(ast));
      astResultOptimized = escape_json(
          peg::ast_to_s(peg.optimize_ast(ast, opt_mode)));
    }
  }

  std::string json;
  json += "{";
  json += std::string("\"grammar_valid\":") + (is_grammar_valid ? "true" : "false");
  json += ",\"grammar\":" + grammarResult;
  json += std::string(",\"source_valid\":") + (is_source_valid ? "true" : "false");
  if (!codeResult.empty()) {
    json += ",\"code\":" + codeResult;
    json += ",\"ast\":\"" + astResult + "\"";
    json += ",\"astOptimized\":\"" + astResultOptimized + "\"";
    if (opt_profile) {
      size_t id = 0;
      std::string profile_buf = "<pre>";
      profile_buf += "  id       total      %     success        fail  "
                   "definition<br/>";
      size_t total_total, total_success = 0, total_fail = 0;
      char buff[BUFSIZ];
      for (auto &[name, success, fail] : stats) {
        total_success += success;
        total_fail += fail;
      }
      total_total = total_success + total_fail;
      sprintf(buff, "%4s  %10lu  %5s  %10lu  %10lu  %s<br/>", "",
              total_total, "", total_success, total_fail,
              "Total counters");
      profile_buf += buff;
      sprintf(buff, "%4s  %10s  %5s  %10.2f  %10.2f  %s<br/>", "", "",
              "", total_success*100.0/total_total,
              total_fail*100.0/total_total, "% success/fail");
      profile_buf += buff;
      profile_buf += "<br/>";
      for (auto &[name, success, fail] : stats) {
        auto total = success + fail;
        auto ratio = total * 100.0 / stats_item_total;
        sprintf(buff, "%4zu  %10lu  %5.2f  %10lu  %10lu  %s<br/>", id, total,
                ratio, success, fail, name.c_str());
        profile_buf += buff;
        id++;
      }
      profile_buf += "</pre>";

      json += ",\"profile\":\"" + escape_json(profile_buf) + "\"";
    }
  }
  json += "}";

  return json;
}

EMSCRIPTEN_BINDINGS(native) { emscripten::function("lint", &lint); }
