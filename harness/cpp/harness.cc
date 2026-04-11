#include <peglib.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

// =============================================================================
// Environment variable helpers
// =============================================================================

static std::string env_or(const char *name, const char *def = "") {
  auto *v = std::getenv(name);
  return v ? std::string(v) : std::string(def);
}

static bool env_flag(const char *name) {
  auto v = env_or(name);
  return !v.empty() && v != "0";
}

// =============================================================================
// JSON test file discovery and line-number scanning
// =============================================================================

static std::vector<fs::path> discover_test_files() {
  std::vector<fs::path> files;
  for (auto &entry : fs::recursive_directory_iterator(SPEC_TESTS_DIR)) {
    if (entry.path().extension() == ".json") { files.push_back(entry.path()); }
  }
  std::sort(files.begin(), files.end());
  return files;
}

static json load_json(const fs::path &path) {
  std::ifstream ifs(path);
  if (!ifs) { throw std::runtime_error("Cannot open: " + path.string()); }
  return json::parse(ifs);
}

// Scan the raw JSON text and find the line number of each top-level
// "name": "..." occurrence. This is a pragmatic way to report source
// locations without a full JSON-with-positions parser.
static std::map<std::string, size_t> scan_group_lines(const fs::path &path) {
  std::map<std::string, size_t> result;
  std::ifstream ifs(path);
  if (!ifs) return result;
  std::string line;
  size_t lineno = 0;
  std::regex re(R"(\"name\"\s*:\s*\"([^\"]+)\")");
  while (std::getline(ifs, line)) {
    ++lineno;
    std::smatch m;
    if (std::regex_search(line, m, re)) {
      auto name = m[1].str();
      // Only record the first occurrence of each name.
      if (result.find(name) == result.end()) { result[name] = lineno; }
    }
  }
  return result;
}

// =============================================================================
// Test context and failure reporting
// =============================================================================

struct TestContext {
  fs::path file;
  std::string group_name;
  size_t group_line = 0;
  size_t case_index = 0;
  std::string input;
  std::string grammar;
};

static std::string truncate(const std::string &s, size_t max_len = 120) {
  if (s.size() <= max_len) return s;
  return s.substr(0, max_len) + "...";
}

static std::string indent_lines(const std::string &s, const char *prefix) {
  std::string result;
  std::string line;
  std::istringstream iss(s);
  while (std::getline(iss, line)) {
    result += prefix;
    result += line;
    result += "\n";
  }
  return result;
}

static std::string format_context(const TestContext &ctx) {
  std::ostringstream oss;
  oss << "\n"
      << "  File:    " << ctx.file.string();
  if (ctx.group_line > 0) { oss << ":" << ctx.group_line; }
  oss << "\n"
      << "  Group:   " << ctx.group_name << "\n"
      << "  Case:    [" << ctx.case_index << "] input=\"" << truncate(ctx.input)
      << "\"\n"
      << "  Reproduce: PEG_SPEC_FOCUS=" << ctx.group_name
      << " ./spec-harness\n";
  return oss.str();
}

// =============================================================================
// Action binding helpers
// =============================================================================

static void bind_action(peg::parser &pg, const std::string &rule,
                        const json &action_def) {
  auto op = action_def["op"].get<std::string>();

  if (op == "token_to_number") {
    pg[rule.c_str()] = [](const peg::SemanticValues &vs) -> long {
      return vs.token_to_number<long>();
    };
  } else if (op == "token_to_string") {
    pg[rule.c_str()] = [](const peg::SemanticValues &vs) -> std::string {
      return vs.token_to_string();
    };
  } else if (op == "choice") {
    pg[rule.c_str()] = [](const peg::SemanticValues &vs) -> long {
      return static_cast<long>(vs.choice());
    };
  } else if (op == "size") {
    pg[rule.c_str()] = [](const peg::SemanticValues &vs) -> long {
      return static_cast<long>(vs.size());
    };
  } else if (op == "passthrough") {
    int index = 0;
    if (action_def.contains("index")) { index = action_def["index"]; }
    pg[rule.c_str()] = [index](const peg::SemanticValues &vs) -> std::any {
      return vs[index];
    };
  } else if (op == "join_tokens") {
    std::string sep;
    if (action_def.contains("separator")) {
      sep = action_def["separator"].get<std::string>();
    }
    pg[rule.c_str()] = [sep](const peg::SemanticValues &vs) -> std::string {
      std::string result;
      for (size_t i = 0; i < vs.tokens.size(); i++) {
        if (i > 0) result += sep;
        result += std::string(vs.token(i));
      }
      return result;
    };
  } else if (op == "choice_op") {
    // Dispatch on vs.choice() to apply a binary op or passthrough.
    // For left-recursive grammars like:
    //   expr <- expr '+' term / expr '-' term / term
    // where vs = [lhs, rhs] for cases 0/1 and vs = [x] for the base case.
    //
    //   "cases": {
    //     "0": { "binary": "add",      "left": 0, "right": 1 },
    //     "1": { "binary": "subtract", "left": 0, "right": 1 },
    //     "default": { "passthrough": 0 }
    //   }
    auto cases = action_def["cases"];
    pg[rule.c_str()] = [cases](const peg::SemanticValues &vs) -> long {
      auto apply = [&](const json &c) -> long {
        if (c.contains("binary")) {
          auto op_name = c["binary"].get<std::string>();
          auto l = std::any_cast<long>(vs[c["left"].get<int>()]);
          auto r = std::any_cast<long>(vs[c["right"].get<int>()]);
          if (op_name == "add") return l + r;
          if (op_name == "subtract") return l - r;
          if (op_name == "multiply") return l * r;
          if (op_name == "divide") return l / r;
        }
        if (c.contains("passthrough")) {
          return std::any_cast<long>(vs[c["passthrough"].get<int>()]);
        }
        return 0;
      };
      auto key = std::to_string(vs.choice());
      if (cases.contains(key)) return apply(cases[key]);
      if (cases.contains("default")) return apply(cases["default"]);
      return 0;
    };
  } else if (op == "reduce") {
    // Pattern: A (O A)*  →  vs = [A, O, A, O, A, ...]
    //   initial = vs[0]
    //   for i in 1, 3, 5, ...: apply vs[i] (operator) with vs[i+1] (value)
    //
    // The operator child's semantic value must be a std::string.
    // If the JSON specifies "operator_rule", the harness auto-binds that
    // rule to token_to_string so the user doesn't need to set it manually.
    auto operators = action_def["step"]["operators"];
    if (action_def.contains("operator_rule")) {
      auto op_rule = action_def["operator_rule"].get<std::string>();
      pg[op_rule.c_str()] = [](const peg::SemanticValues &vs) -> std::string {
        return vs.token_to_string();
      };
    }

    pg[rule.c_str()] = [operators](const peg::SemanticValues &vs) -> long {
      if (vs.empty()) return 0;
      auto result = std::any_cast<long>(vs[0]);
      for (size_t i = 1; i + 1 < vs.size(); i += 2) {
        // The operator child must have its semantic value set to a std::string
        // (typically via token_to_string action, which the harness can
        // auto-bind via the reduce action's "operator_rule" field).
        auto op_str = std::any_cast<std::string>(vs[i]);
        auto num = std::any_cast<long>(vs[i + 1]);
        for (auto &[key, val] : operators.items()) {
          if (op_str == key) {
            auto action = val.get<std::string>();
            if (action == "add")
              result += num;
            else if (action == "subtract")
              result -= num;
            else if (action == "multiply")
              result *= num;
            else if (action == "divide")
              result /= num;
            break;
          }
        }
      }
      return result;
    };
  }
}

// =============================================================================
// Variable state for handlers
// =============================================================================

struct VarState {
  std::map<std::string, std::any> vars;

  void init(const json &vars_def) {
    for (auto &[name, def] : vars_def.items()) {
      auto type = def["type"].get<std::string>();
      if (type == "bool") {
        vars[name] = def["init"].get<bool>();
      } else if (type == "int") {
        vars[name] = def["init"].get<int>();
      } else if (type == "string[]") {
        vars[name] = def["init"].get<std::vector<std::string>>();
      }
    }
  }

  bool get_bool(const std::string &name) const {
    return std::any_cast<bool>(vars.at(name));
  }

  int get_int(const std::string &name) const {
    return std::any_cast<int>(vars.at(name));
  }

  const std::vector<std::string> &
  get_string_array(const std::string &name) const {
    return std::any_cast<const std::vector<std::string> &>(vars.at(name));
  }

  void set_var(const std::string &name, const json &value_expr) {
    if (value_expr.is_boolean()) {
      vars[name] = value_expr.get<bool>();
    } else if (value_expr.is_number_integer()) {
      vars[name] = value_expr.get<int>();
    } else if (value_expr.is_string()) {
      // Expression like "var + N" or "var - N"
      auto expr = value_expr.get<std::string>();
      auto plus_pos = expr.find(" + ");
      auto minus_pos = expr.find(" - ");
      if (plus_pos != std::string::npos) {
        auto var_name = expr.substr(0, plus_pos);
        auto delta = std::stoi(expr.substr(plus_pos + 3));
        vars[name] = get_int(var_name) + delta;
      } else if (minus_pos != std::string::npos) {
        auto var_name = expr.substr(0, minus_pos);
        auto delta = std::stoi(expr.substr(minus_pos + 3));
        vars[name] = get_int(var_name) - delta;
      }
    }
  }
};

// =============================================================================
// Handler binding
// =============================================================================

static void bind_handlers(peg::parser &pg, const json &handlers,
                          std::shared_ptr<VarState> state) {
  for (auto &[rule, handler_def] : handlers.items()) {
    if (handler_def.contains("enter")) {
      auto enter_def = handler_def["enter"];
      pg[rule.c_str()].enter = [state, enter_def](const peg::Context &,
                                                  const char *, size_t,
                                                  std::any &) {
        if (enter_def.contains("set")) {
          for (auto &[var, val] : enter_def["set"].items()) {
            state->set_var(var, val);
          }
        }
      };
    }

    if (handler_def.contains("leave")) {
      auto leave_def = handler_def["leave"];
      pg[rule.c_str()].leave = [state, leave_def](const peg::Context &,
                                                  const char *, size_t, size_t,
                                                  std::any &, std::any &) {
        if (leave_def.contains("set")) {
          for (auto &[var, val] : leave_def["set"].items()) {
            state->set_var(var, val);
          }
        }
      };
    }

    if (handler_def.contains("predicate")) {
      auto pred_def = handler_def["predicate"];
      pg[rule.c_str()].predicate = [state, pred_def](
                                       const peg::SemanticValues &vs,
                                       const std::any &, std::string &msg) {
        // Check "when" condition
        if (pred_def.contains("when")) {
          auto when_var = pred_def["when"].get<std::string>();
          if (!state->get_bool(when_var)) { return true; }
        }

        auto check = pred_def["check"];
        auto error_msg = pred_def["error"].get<std::string>();

        for (auto &[prop, comparison] : check.items()) {
          if (prop == "sv") {
            auto sv_str = std::string(vs.sv());
            for (auto &[op, val] : comparison.items()) {
              if (op == "matches") {
                auto pattern = val.get<std::string>();
                std::regex re(pattern);
                if (!std::regex_match(sv_str, re)) {
                  msg = error_msg;
                  return false;
                }
              } else if (op == "eq") {
                std::string expected;
                if (val.is_string()) {
                  // Could be a variable name or literal
                  expected = val.get<std::string>();
                }
                if (sv_str != expected) {
                  msg = error_msg;
                  return false;
                }
              }
            }
          } else if (prop == "sv.length") {
            auto len = static_cast<int>(vs.sv().size());
            for (auto &[op, val] : comparison.items()) {
              if (op == "eq") {
                int expected;
                if (val.is_string()) {
                  expected = state->get_int(val.get<std::string>());
                } else {
                  expected = val.get<int>();
                }
                if (len != expected) {
                  msg = error_msg;
                  return false;
                }
              }
            }
          } else if (prop == "token_number") {
            auto num = vs.token_to_number<long>();
            for (auto &[op, val] : comparison.items()) {
              if (op == "eq") {
                long expected = val.get<long>();
                if (num != expected) {
                  msg = error_msg;
                  return false;
                }
              } else if (op == "between") {
                auto min_val = val[0].get<long>();
                auto max_val = val[1].get<long>();
                if (num < min_val || num > max_val) {
                  msg = error_msg;
                  return false;
                }
              }
            }
          } else if (prop == "token_string") {
            auto tok = vs.token_to_string();
            for (auto &[op, val] : comparison.items()) {
              if (op == "not_in") {
                auto var_name = val.get<std::string>();
                auto &list = state->get_string_array(var_name);
                if (std::find(list.begin(), list.end(), tok) != list.end()) {
                  msg = error_msg;
                  return false;
                }
              } else if (op == "in") {
                auto var_name = val.get<std::string>();
                auto &list = state->get_string_array(var_name);
                if (std::find(list.begin(), list.end(), tok) == list.end()) {
                  msg = error_msg;
                  return false;
                }
              }
            }
          }
        }
        return true;
      };
    }
  }
}

// =============================================================================
// Trace binding
// =============================================================================

static void bind_trace(peg::parser &pg, const json &trace_def,
                       std::shared_ptr<std::vector<std::string>> trace_log) {
  auto rules = trace_def["rules"].get<std::vector<std::string>>();
  auto events = trace_def["events"].get<std::vector<std::string>>();

  bool trace_enter =
      std::find(events.begin(), events.end(), "enter") != events.end();
  bool trace_leave =
      std::find(events.begin(), events.end(), "leave") != events.end();

  for (const auto &rule : rules) {
    if (trace_enter) {
      auto r = rule;
      pg[rule.c_str()].enter =
          [trace_log, r](const peg::Context &, const char *, size_t,
                         std::any &) { trace_log->push_back("enter_" + r); };
    }
    if (trace_leave) {
      auto r = rule;
      pg[rule.c_str()].leave = [trace_log, r](const peg::Context &,
                                              const char *, size_t, size_t,
                                              std::any &, std::any &) {
        trace_log->push_back("leave_" + r);
      };
    }
  }
}

// =============================================================================
// PEG meta-grammar test support
// =============================================================================

static void run_peg_meta_test(const json &group, TestContext ctx) {
  if (group.contains("cases_by_rule")) {
    for (auto &[rule, cases] : group["cases_by_rule"].items()) {
      size_t idx = 0;
      for (auto &c : cases) {
        ctx.case_index = idx++;
        ctx.input = c["input"].get<std::string>();
        auto expected = c["match"].get<bool>();
        auto result =
            peg::ParserGenerator::parse_test(rule.c_str(), ctx.input.c_str());
        EXPECT_EQ(expected, result)
            << "Meta-test rule=" << rule << format_context(ctx);
      }
    }
  } else {
    auto rule = group["rule"].get<std::string>();
    size_t idx = 0;
    for (auto &c : group["cases"]) {
      ctx.case_index = idx++;
      ctx.input = c["input"].get<std::string>();
      auto expected = c["match"].get<bool>();
      auto result =
          peg::ParserGenerator::parse_test(rule.c_str(), ctx.input.c_str());
      EXPECT_EQ(expected, result)
          << "Meta-test rule=" << rule << format_context(ctx);
    }
  }
}

// =============================================================================
// Main test runner
// =============================================================================

static void run_test_group(const json &group, TestContext ctx) {
  // Focus filter: skip groups that don't match PEG_SPEC_FOCUS
  static const std::string focus = env_or("PEG_SPEC_FOCUS");
  if (!focus.empty() && ctx.group_name != focus) { return; }

  static const bool verbose = env_flag("PEG_SPEC_VERBOSE");
  if (verbose) {
    std::cerr << "[harness] running: " << ctx.group_name << " ("
              << ctx.file.filename().string();
    if (ctx.group_line > 0) std::cerr << ":" << ctx.group_line;
    std::cerr << ")\n";
  }

  // PEG meta-grammar tests
  if (group.contains("peg_meta_test") && group["peg_meta_test"].get<bool>()) {
    run_peg_meta_test(group, ctx);
    return;
  }

  ctx.grammar = group["grammar"].get<std::string>();

  // Check left_recursion setting
  bool lr_enabled = true;
  if (group.contains("left_recursion")) {
    lr_enabled = group["left_recursion"].get<bool>();
  }

  // Start rule override
  std::string start_rule;
  if (group.contains("start_rule")) {
    start_rule = group["start_rule"].get<std::string>();
  }

  // If group has "grammar_compiles" flag, just verify grammar loads
  if (group.contains("grammar_compiles") &&
      group["grammar_compiles"].get<bool>()) {
    peg::parser pg;
    if (!lr_enabled) { pg.enable_left_recursion(false); }
    bool ok;
    if (start_rule.empty()) {
      ok = pg.load_grammar(ctx.grammar);
    } else {
      ok = pg.load_grammar(ctx.grammar, start_rule.c_str());
    }
    EXPECT_TRUE(ok) << "Grammar should compile" << format_context(ctx)
                    << "  Grammar:\n"
                    << indent_lines(ctx.grammar, "    ");
    return;
  }

  if (!group.contains("cases")) { return; }

  size_t case_idx = 0;
  for (auto &c : group["cases"]) {
    ctx.case_index = case_idx++;
    ctx.input = c["input"].get<std::string>();
    auto input = ctx.input;

    // Grammar error test
    if (c.contains("grammar_error") && c["grammar_error"].get<bool>()) {
      peg::parser pg;
      if (!lr_enabled) { pg.enable_left_recursion(false); }
      bool ok;
      if (start_rule.empty()) {
        ok = pg.load_grammar(ctx.grammar);
      } else {
        ok = pg.load_grammar(ctx.grammar, start_rule.c_str());
      }
      EXPECT_FALSE(ok) << "Grammar should fail to compile"
                       << format_context(ctx) << "  Grammar:\n"
                       << indent_lines(ctx.grammar, "    ");
      continue;
    }

    // Create parser
    peg::parser pg;
    if (!lr_enabled) { pg.enable_left_recursion(false); }

    bool grammar_ok;
    if (start_rule.empty()) {
      grammar_ok = pg.load_grammar(ctx.grammar);
    } else {
      grammar_ok = pg.load_grammar(ctx.grammar, start_rule.c_str());
    }

    if (!grammar_ok) {
      ADD_FAILURE() << "Grammar failed to compile unexpectedly"
                    << format_context(ctx) << "  Grammar:\n"
                    << indent_lines(ctx.grammar, "    ");
      continue;
    }

    // Enable packrat if specified
    if (group.contains("packrat") && group["packrat"].get<bool>()) {
      pg.enable_packrat_parsing();
    }

    // State variables
    auto state = std::make_shared<VarState>();
    if (group.contains("vars")) { state->init(group["vars"]); }

    // Bind actions
    if (group.contains("actions")) {
      for (auto &[rule, action_def] : group["actions"].items()) {
        bind_action(pg, rule, action_def);
      }
    }

    // Bind handlers
    if (group.contains("handlers")) {
      bind_handlers(pg, group["handlers"], state);
    }

    // Trace
    auto trace_log = std::make_shared<std::vector<std::string>>();
    if (group.contains("trace")) { bind_trace(pg, group["trace"], trace_log); }

    // AST mode
    bool ast_mode = group.contains("ast");
    bool ast_optimize = false;
    if (ast_mode && group["ast"].contains("optimize")) {
      ast_optimize = group["ast"]["optimize"].get<bool>();
    }
    if (ast_mode) { pg.enable_ast(); }

    // Error collection
    std::vector<std::tuple<size_t, size_t, std::string>> errors;
    pg.set_logger([&](size_t ln, size_t col, const std::string &msg) {
      errors.emplace_back(ln, col, msg);
    });

    // Parse
    bool match_result;
    std::shared_ptr<peg::Ast> ast;
    long value_result = 0;
    bool has_value = false;

    if (ast_mode) {
      match_result = pg.parse(input.c_str(), ast);
      if (match_result && ast_optimize) { ast = pg.optimize_ast(ast); }
    } else if (c.contains("expected_value")) {
      match_result = pg.parse(input.c_str(), value_result);
      has_value = match_result;
    } else {
      match_result = pg.parse(input.c_str());
    }

    // Verify match
    if (c.contains("match")) {
      auto expected_match = c["match"].get<bool>();
      EXPECT_EQ(expected_match, match_result)
          << "match mismatch" << format_context(ctx);
    }

    // Verify expected value
    if (c.contains("expected_value") && has_value) {
      auto &ev = c["expected_value"];
      if (ev.is_number_integer()) {
        EXPECT_EQ(ev.get<long>(), value_result)
            << "expected_value mismatch" << format_context(ctx);
      }
    }

    // Verify expected AST
    if (c.contains("expected_ast") && ast) {
      auto expected_ast = c["expected_ast"].get<std::string>();
      auto actual_ast = peg::ast_to_s(ast);
      EXPECT_EQ(expected_ast, actual_ast)
          << "expected_ast mismatch" << format_context(ctx)
          << "  Expected AST:\n"
          << indent_lines(expected_ast, "    ") << "  Actual AST:\n"
          << indent_lines(actual_ast, "    ");
    }

    // Verify expected error
    if (c.contains("expected_error")) {
      ASSERT_FALSE(errors.empty())
          << "expected error but none occurred" << format_context(ctx);
      auto &ee = c["expected_error"];
      auto &[ln, col, msg] = errors[0];
      if (ee.contains("line")) {
        EXPECT_EQ(ee["line"].get<size_t>(), ln)
            << "error line mismatch" << format_context(ctx);
      }
      if (ee.contains("col")) {
        EXPECT_EQ(ee["col"].get<size_t>(), col)
            << "error col mismatch" << format_context(ctx);
      }
      if (ee.contains("message")) {
        EXPECT_EQ(ee["message"].get<std::string>(), msg)
            << "error message mismatch" << format_context(ctx);
      }
    }

    // Verify expected errors (multiple)
    if (c.contains("expected_errors")) {
      auto &ees = c["expected_errors"];
      ASSERT_EQ(ees.size(), errors.size())
          << "error count mismatch" << format_context(ctx);
      for (size_t i = 0; i < ees.size(); i++) {
        auto &ee = ees[i];
        auto &[ln, col, msg] = errors[i];
        if (ee.contains("line")) {
          EXPECT_EQ(ee["line"].get<size_t>(), ln)
              << "errors[" << i << "].line" << format_context(ctx);
        }
        if (ee.contains("col")) {
          EXPECT_EQ(ee["col"].get<size_t>(), col)
              << "errors[" << i << "].col" << format_context(ctx);
        }
        if (ee.contains("message")) {
          EXPECT_EQ(ee["message"].get<std::string>(), msg)
              << "errors[" << i << "].message" << format_context(ctx);
        }
      }
    }

    // Verify trace
    if (c.contains("expected_trace")) {
      auto expected = c["expected_trace"].get<std::vector<std::string>>();
      EXPECT_EQ(expected, *trace_log)
          << "trace mismatch" << format_context(ctx);
    }

    if (c.contains("expected_trace_prefix")) {
      auto prefix = c["expected_trace_prefix"].get<std::vector<std::string>>();
      ASSERT_GE(trace_log->size(), prefix.size())
          << "trace too short for prefix check" << format_context(ctx);
      for (size_t i = 0; i < prefix.size(); i++) {
        EXPECT_EQ(prefix[i], (*trace_log)[i])
            << "trace[" << i << "] mismatch" << format_context(ctx);
      }
    }

    // Verify expected state
    if (c.contains("expected_state")) {
      for (auto &[var, val] : c["expected_state"].items()) {
        if (val.is_boolean()) {
          EXPECT_EQ(val.get<bool>(), state->get_bool(var))
              << "state." << var << " mismatch" << format_context(ctx);
        } else if (val.is_number_integer()) {
          EXPECT_EQ(val.get<int>(), state->get_int(var))
              << "state." << var << " mismatch" << format_context(ctx);
        }
      }
    }
  }
}

// =============================================================================
// Test registration: one gtest per JSON file
// =============================================================================

class SpecTest : public ::testing::TestWithParam<fs::path> {};

TEST_P(SpecTest, RunFile) {
  auto path = GetParam();
  auto data = load_json(path);
  auto lines = scan_group_lines(path);

  for (auto &group : data) {
    TestContext ctx;
    ctx.file = path;
    if (group.contains("name")) {
      ctx.group_name = group["name"].get<std::string>();
      auto it = lines.find(ctx.group_name);
      if (it != lines.end()) { ctx.group_line = it->second; }
    }
    run_test_group(group, ctx);
  }
}

INSTANTIATE_TEST_SUITE_P(PegSpec, SpecTest,
                         ::testing::ValuesIn(discover_test_files()),
                         [](const ::testing::TestParamInfo<fs::path> &info) {
                           // Generate test name from relative path
                           auto rel = fs::relative(info.param, SPEC_TESTS_DIR);
                           std::string name;
                           for (auto part : rel) {
                             if (!name.empty()) name += "_";
                             auto s = part.string();
                             // Remove .json extension
                             if (s.size() > 5 &&
                                 s.substr(s.size() - 5) == ".json") {
                               s = s.substr(0, s.size() - 5);
                             }
                             name += s;
                           }
                           return name;
                         });
