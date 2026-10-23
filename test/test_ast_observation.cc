// What predicates, leave handlers, user actions and tracers see of AST nodes,
// and the tree a parse returns: its fields, its parents and which nodes are
// the same objects. The expected outputs are those of the version that built
// every node in its rule's action, before AST records.

#include <gtest/gtest.h>
#include <peglib.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "ast_dump.h"

using namespace peg;

namespace {

// -----------------------------------------------------------------------------
// The output of many configurations, fixed by hash
// -----------------------------------------------------------------------------

struct ObservedGrammar {
  const char *name;
  const char *text;
  std::vector<const char *> inputs;
  std::vector<const char *> predicates;
  std::vector<const char *> leaves;
  // The rule given a user action that builds a node of its own over the
  // children it sees, and a rule whose AST action is removed with it, so
  // that it passes its first value up, if any.
  const char *action_rule;
  const char *unset_action_rule;
  // Whether a rule is marked no_ast_opt: without one, enable_ast(true, false)
  // collapses nothing and is enable_ast().
  bool has_no_ast_opt;
};

// Rules that collapse in chains, token rules with inner rules, ignored rules,
// a precedence rule and a macro, under backtracking.
const char *precedence_grammar = R"(
  PROGRAM     <-  STATEMENT (';' STATEMENT)*
  STATEMENT   <-  YIELD / CALL / ASSIGN / VALUE
  YIELD       <-  'yield' VALUE              { no_ast_opt }
  CALL        <-  NAME '(' LIST(VALUE, ',') ')'
  ASSIGN      <-  NAME '=' VALUE ~BANG?
  VALUE       <-  WRAP / EXPR
  WRAP        <-  INNER EXCL
  INNER       <-  EXPR
  EXPR        <-  TERM (OP TERM)* {
                    precedence
                      L + -
                      L * /
                  }
  TERM        <-  NUMBER / NAME / '(' VALUE ')' / '[' LIST(VALUE, ',') ']'
  LIST(I, D)  <-  (I (D I)*)?
  OP          <-  < [-+*/] >
  NUMBER      <-  < DIGIT+ >                 { ast_name: NUM }
  DIGIT       <-  [0-9]
  NAME        <-  < [a-z\u00e0-\u00ff]+ >
  BANG        <-  '??'
  ~EXCL       <-  '!'
  %whitespace <-  [ \t\r\n]*
)";

// The same with a left-recursive expression in place of precedence.
const char *left_recursive_grammar = R"(
  PROGRAM     <-  STATEMENT (';' STATEMENT)*
  STATEMENT   <-  YIELD / CALL / ASSIGN / VALUE
  YIELD       <-  'yield' VALUE              { no_ast_opt }
  CALL        <-  NAME '(' LIST(VALUE, ',') ')'
  ASSIGN      <-  NAME '=' VALUE ~BANG?
  VALUE       <-  WRAP / EXPR
  WRAP        <-  INNER EXCL
  INNER       <-  EXPR
  EXPR        <-  EXPR OP TERM / TERM
  TERM        <-  NUMBER / NAME / '(' VALUE ')' / '[' LIST(VALUE, ',') ']'
  LIST(I, D)  <-  (I (D I)*)?
  OP          <-  < [-+*/] >
  NUMBER      <-  < DIGIT+ >                 { ast_name: NUM }
  DIGIT       <-  [0-9]
  NAME        <-  < [a-z\u00e0-\u00ff]+ >
  BANG        <-  '??'
  ~EXCL       <-  '!'
  %whitespace <-  [ \t\r\n]*
)";

// Matches under `~`, `&` and `!`, inside token rules and in whitespace, and
// precedence rules, one of them in a macro used under `&`.
const char *lookahead_grammar = R"(
  PROGRAM     <-  &TERM STATEMENT (';' STATEMENT)* END?
  STATEMENT   <-  DECL / CALL / &CHECK &INFIX(TERM, OP) EXPR
  CHECK       <-  INFIX(TERM, OP)
  INFIX(A, O) <-  A (O A)* {
                    precedence
                      L + -
                      L * /
                  }
  DECL        <-  TYPE NAME ('=' EXPR)?
  TYPE        <-  < NAME >
  CALL        <-  NAME '(' LIST(EXPR, ',') ')'
  EXPR        <-  TERM (OP TERM)* {
                    precedence
                      L + -
                      L * /
                  }
  TERM        <-  NUMBER / NAME / '(' EXPR ')' / '[' LIST(EXPR, ',') ']'
                / &'-' SIGNED
  SIGNED      <-  '-' NUMBER
  LIST(I, D)  <-  (I (D I)*)?
  OP          <-  < '+' / '-' / '*' / '/' >
  NUMBER      <-  < DIGIT+ FRACTION? >
  FRACTION    <-  '.' DIGIT+
  DIGIT       <-  [0-9]
  NAME        <-  !KEYWORD < ALPHA (ALPHA / DIGIT)* >
  KEYWORD     <-  ('if' / 'else') !ALPHA
  ALPHA       <-  [a-z]
  ~END        <-  '.' ALPHA*
  COMMENT     <-  '#' (!EOL .)* EOL
  EOL         <-  '\r\n' / '\n'
  %whitespace <-  [ \t\r\n]* COMMENT?
)";

// The inputs, and the letters past ASCII in the grammars, are written with
// escapes, so that a checkout's line endings and a compiler's source encoding
// do not change them.
const ObservedGrammar observed_grammars[] = {
    {"prec",
     precedence_grammar,
     {"yield 1 + 2 * (x); f(1, [2, a + b!], (3)); a = [1, (2), b] ??; 42!",
      "yield (\xc3\xa9)!;\n  f(1, [\xc3\xb1 + b!],\n(3)); x = (2) ??"},
     {"STATEMENT", "INNER", "EXPR", "TERM", "OP", "NAME"},
     {"VALUE", "WRAP", "EXPR", "NUMBER", "BANG"},
     "CALL",
     nullptr,
     true},
    {"lr",
     left_recursive_grammar,
     {"yield 1 + 2 * (x); f(1, [2, a + b!], (3)); a = [1, (2), b] ??; 42!",
      "yield (\xc3\xa9)!;\n  f(1, [\xc3\xb1 + b!],\n(3)); x = (2) ??"},
     {"STATEMENT", "INNER", "EXPR", "TERM", "OP", "NAME"},
     {"VALUE", "WRAP", "EXPR", "NUMBER", "BANG"},
     "CALL",
     nullptr,
     true},
    {"look",
     lookahead_grammar,
     {"int a = 1 + # c\n2 * (b - 3.5); f(1, [2, -4], iff); elsex / 6 .end"},
     {"TYPE", "STATEMENT", "OP", "NUMBER", "CALL"},
     {"TERM", "FRACTION", "COMMENT", "KEYWORD"},
     "SIGNED",
     "TERM",
     false},
};

constexpr int kPredicates = 1;
constexpr int kLeaves = 2;
constexpr int kActions = 4;

struct ObservedConfig {
  const ObservedGrammar *grammar;
  size_t input;
  int ast_mode; // enable_ast(), enable_ast(true), enable_ast(true, false)
  bool packrat;
  int observe;
  // Whether the nodes the callbacks see are kept until the parse is over,
  // which makes a collapsing parent copy them instead of taking them over.
  bool keep;
  // Whether the parse is asked for the tree.
  bool takes_tree;

  std::string name() const {
    static const char *modes[] = {"ast", "astc", "astc-opt0"};
    std::string observed = observe ? "" : "-";
    if (observe & kPredicates) { observed += "P"; }
    if (observe & kLeaves) { observed += "L"; }
    if (observe & kActions) { observed += "A"; }
    return std::string(grammar->name) + " in" + std::to_string(input) + " " +
           modes[ast_mode] + " pk" + std::to_string(packrat) + " " + observed +
           " keep" + std::to_string(keep) + (takes_tree ? "" : " notree");
  }
};

struct ObservedOutput {
  std::string seen;  // what the callbacks saw, as they saw it
  std::string after; // the nodes kept, after the parse
  std::string tree;  // the tree the parse returned
};

ObservedOutput observe_parse(const ObservedConfig &config, bool tracer) {
  const auto &g = *config.grammar;
  parser pg(g.text);
  EXPECT_TRUE(pg);
  if (config.packrat) { pg.enable_packrat_parsing(); }
  switch (config.ast_mode) {
  case 0: pg.enable_ast(); break;
  case 1: pg.enable_ast(true); break;
  default: pg.enable_ast(true, false); break;
  }
  if (tracer) {
    pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
  }

  NodeIds<Ast> ids;
  ObservedOutput out;
  std::vector<std::shared_ptr<Ast>> kept;
  auto see = [&](const std::any &value) {
    if (!value.has_value()) {
      out.seen += "(empty)\n";
      return;
    }
    auto node = std::any_cast<std::shared_ptr<Ast>>(value);
    dump_ast(node, ids, out.seen, false);
    if (config.keep) { kept.push_back(node); }
  };
  auto see_scope = [&](const std::string &what, const SemanticValues &vs) {
    out.seen += what + " " + std::to_string(vs.size()) + " '" +
                vs.token_to_string() + "' " +
                std::to_string(vs.choice_count()) + "/" +
                std::to_string(vs.choice()) + "\n";
    for (const auto &v : vs) {
      see(v);
    }
  };

  if (config.observe & kPredicates) {
    for (std::string name : g.predicates) {
      pg[name.c_str()].predicate = [&, name](const SemanticValues &vs,
                                             const std::any &, std::string &) {
        see_scope("P " + name, vs);
        return true;
      };
    }
  }
  if (config.observe & kLeaves) {
    for (std::string name : g.leaves) {
      pg[name.c_str()].leave = [&, name](const Context &, const char *, size_t,
                                         size_t len, std::any &value,
                                         std::any &) {
        if (!success(len)) { return; }
        out.seen += "L " + name + "\n";
        see(value);
      };
    }
  }
  if (config.observe & kActions) {
    if (g.unset_action_rule) { pg[g.unset_action_rule].action = Action(); }
    std::string name = g.action_rule;
    pg[g.action_rule] = [&, name](const SemanticValues &vs) {
      see_scope("A " + name, vs);
      auto node = std::make_shared<Ast>("", 1, 1, "USER",
                                        vs.transform<std::shared_ptr<Ast>>(), 0,
                                        vs.sv().size());
      for (const auto &child : node->nodes) {
        child->parent = node;
      }
      return node;
    };
  }

  std::string input = g.inputs[config.input];
  std::shared_ptr<Ast> tree;
  auto ok =
      config.takes_tree ? pg.parse(input, tree, "src") : pg.parse(input, "src");
  if (!ok) { out.tree += "FAILED\n"; }
  for (const auto &node : kept) {
    dump_ast(node, ids, out.after, false);
  }
  if (tree) { dump_ast(tree, ids, out.tree, true); }
  return out;
}

std::vector<ObservedConfig> observed_configs() {
  std::vector<ObservedConfig> configs;
  for (const auto &g : observed_grammars) {
    for (size_t input = 0; input < g.inputs.size(); input++) {
      for (auto ast_mode : {0, 1, 2}) {
        if (ast_mode == 2 && !g.has_no_ast_opt) { continue; }
        for (auto packrat : {false, true}) {
          for (auto observe : {0, kPredicates, kLeaves, kActions,
                               kPredicates | kLeaves | kActions}) {
            for (auto keep : {false, true}) {
              // Where nothing collapses, keeping a node changes nothing.
              if (keep && (!observe || ast_mode == 0)) { continue; }
              configs.push_back(
                  {&g, input, ast_mode, packrat, observe, keep, true});
            }
          }
          // The callbacks of a parse that is not asked for the tree.
          if (ast_mode == 1) {
            configs.push_back({&g, input, ast_mode, packrat,
                               kPredicates | kLeaves | kActions, true, false});
          }
        }
      }
    }
  }
  return configs;
}

struct SectionDigest {
  size_t lines;
  uint64_t hash;

  bool operator==(const SectionDigest &rhs) const {
    return lines == rhs.lines && hash == rhs.hash;
  }
};

SectionDigest digest(const std::string &s) {
  SectionDigest d{0, 14695981039346656037ull}; // FNV-1a
  for (unsigned char ch : s) {
    d.hash = (d.hash ^ ch) * 1099511628211ull;
    if (ch == '\n') { d.lines++; }
  }
  return d;
}

struct ObservedGolden {
  const char *config;
  SectionDigest seen, after, tree;
};

// Printed with CPPPEGLIB_TEST_PRINT_AST_GOLDEN=table (=full prints the
// outputs), by 1b59864, which built every node in its rule's action.
const ObservedGolden observed_goldens[] = {
#include "ast_observation_goldens.inc"
};

std::string hex(uint64_t v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "0x%016llx",
                static_cast<unsigned long long>(v));
  return buf;
}

std::string digest_entry(const SectionDigest &d) {
  return "{" + std::to_string(d.lines) + ", " + hex(d.hash) + "ull}";
}

} // namespace

TEST(AstObservationTest, Callbacks_and_tree_as_when_every_node_is_built) {
  std::map<std::string, const ObservedGolden *> goldens;
  for (const auto &golden : observed_goldens) {
    goldens[golden.config] = &golden;
  }

  const char *print = std::getenv("CPPPEGLIB_TEST_PRINT_AST_GOLDEN");
  const std::string printing = print ? print : "";

  size_t checked = 0;
  for (const auto &config : observed_configs()) {
    auto name = config.name();
    ObservedOutput out, traced;
    try {
      out = observe_parse(config, false);
      traced = observe_parse(config, true);
    } catch (const std::exception &e) {
      ADD_FAILURE() << name << ": " << e.what();
      continue;
    }
    SectionDigest seen = digest(out.seen), after = digest(out.after),
                  tree = digest(out.tree);

    if (printing == "table") {
      std::printf("{\"%s\", %s, %s, %s},\n", name.c_str(),
                  digest_entry(seen).c_str(), digest_entry(after).c_str(),
                  digest_entry(tree).c_str());
    } else if (printing == "full") {
      std::printf("=== %s\n--- seen\n%s--- after\n%s--- tree\n%s", name.c_str(),
                  out.seen.c_str(), out.after.c_str(), out.tree.c_str());
    }

    // A tracer sees every scope, so every node is built as its rule matches:
    // the callbacks and the tree must not tell the difference.
    EXPECT_EQ(out.seen, traced.seen) << name;
    EXPECT_EQ(out.after, traced.after) << name;
    EXPECT_EQ(out.tree, traced.tree) << name;

    auto it = goldens.find(name);
    if (it == goldens.end()) {
      ADD_FAILURE() << "no expected output for " << name;
      continue;
    }
    // To see what changed, print the outputs with
    // CPPPEGLIB_TEST_PRINT_AST_GOLDEN=full here and at the commit that made
    // the expected digests, and compare them.
    const auto &golden = *it->second;
    checked++;
    auto expect = [&](const char *section, const SectionDigest &expected,
                      const SectionDigest &actual, const std::string &output) {
      EXPECT_TRUE(expected == actual)
          << name << " " << section << ": expected " << digest_entry(expected)
          << ", got " << digest_entry(actual) << "\n"
          << output;
    };
    expect("seen", golden.seen, seen, out.seen);
    expect("after", golden.after, after, out.after);
    expect("tree", golden.tree, tree, out.tree);
  }
  EXPECT_EQ(goldens.size(), checked);
}

// -----------------------------------------------------------------------------
// One way of seeing nodes at a time
// -----------------------------------------------------------------------------

namespace {

struct Mark {
  int mark = 0;
};
using MarkedAst = AstBase<Mark>;

// The children of the tree of "S <- T (',' T)*": each one's name, its
// original name, its mark, and whether it is the node a callback saw at the
// same index. The callbacks hold what they saw by weak_ptr, which counts as
// no hold on it.
std::string marked_children(const std::shared_ptr<MarkedAst> &ast,
                            const std::vector<std::weak_ptr<MarkedAst>> &seen) {
  std::string out;
  for (size_t i = 0; i < ast->nodes.size(); i++) {
    const auto &node = ast->nodes[i];
    out += node->name + "/" + node->original_name + " " +
           std::to_string(node->mark) +
           (i < seen.size() && seen[i].lock() == node ? " seen" : "") + "\n";
  }
  return out;
}

const char *list_grammar = R"(
  S <- T (',' T)*
  T <- U
  U <- N
  N <- < [0-9]+ >
)";

// The tests below run with and without packrat parsing, which makes a parse
// record its matches even where it memoizes nothing, to cover both ways of
// building the nodes.

} // namespace

// A leave handler marks a node and keeps no hold on it: the collapsing rules
// above take that very node over, mark and all. One that keeps the node makes
// them copy it, and the copy starts unmarked.
TEST(AstObservationTest, Collapse_takes_over_a_node_a_leave_handler_marked) {
  for (auto packrat : {false, true}) {
    for (auto keep : {false, true}) {
      parser pg(list_grammar);
      if (packrat) { pg.enable_packrat_parsing(); }
      std::vector<std::weak_ptr<MarkedAst>> seen;
      std::vector<std::shared_ptr<MarkedAst>> kept;
      pg["N"].leave = [&](const Context &, const char *, size_t, size_t len,
                          std::any &value, std::any &) {
        if (!success(len)) { return; }
        auto node = std::any_cast<std::shared_ptr<MarkedAst>>(value);
        node->mark = 42;
        seen.push_back(node);
        if (keep) { kept.push_back(node); }
      };
      pg.enable_ast<MarkedAst>(true);
      std::shared_ptr<MarkedAst> ast;
      ASSERT_TRUE(pg.parse("1,2", ast));
      EXPECT_EQ(keep ? "N/T 0\nN/T 0\n" : "N/T 42 seen\nN/T 42 seen\n",
                marked_children(ast, seen))
          << "packrat=" << packrat << " keep=" << keep;
      for (const auto &node : kept) {
        EXPECT_EQ("N", node->original_name)
            << "packrat=" << packrat << " keep=" << keep;
      }
    }
  }
}

// The same when a second leave handler looks at the collapsed node on the way
// up, with or without a tracer.
TEST(AstObservationTest, Collapse_takes_over_a_node_two_leave_handlers_saw) {
  for (auto packrat : {false, true}) {
    for (auto tracer : {false, true}) {
      parser pg(list_grammar);
      if (packrat) { pg.enable_packrat_parsing(); }
      std::vector<std::weak_ptr<MarkedAst>> seen;
      std::string uses;
      pg["N"].leave = [&](const Context &, const char *, size_t, size_t len,
                          std::any &value, std::any &) {
        if (!success(len)) { return; }
        auto node = std::any_cast<std::shared_ptr<MarkedAst>>(value);
        node->mark = 42;
        seen.push_back(node);
      };
      pg["U"].leave = [&](const Context &, const char *, size_t, size_t len,
                          std::any &value, std::any &) {
        if (!success(len)) { return; }
        uses +=
            std::to_string(std::any_cast<std::shared_ptr<MarkedAst> &>(value)
                               .use_count()) +
            " ";
      };
      pg.enable_ast<MarkedAst>(true);
      if (tracer) {
        pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
      }
      std::shared_ptr<MarkedAst> ast;
      ASSERT_TRUE(pg.parse("1,2", ast));
      EXPECT_EQ("1 1 ", uses) << "packrat=" << packrat << " tracer=" << tracer;
      EXPECT_EQ("N/T 42 seen\nN/T 42 seen\n", marked_children(ast, seen))
          << "packrat=" << packrat << " tracer=" << tracer;
    }
  }
}

// A user action that passes up the node it is given: the collapsing rule
// above takes it over unless the action keeps it too.
TEST(AstObservationTest, Collapse_takes_over_a_node_a_user_action_passed_up) {
  for (auto packrat : {false, true}) {
    for (auto keep : {false, true}) {
      parser pg(list_grammar);
      if (packrat) { pg.enable_packrat_parsing(); }
      std::vector<std::weak_ptr<MarkedAst>> seen;
      std::vector<std::shared_ptr<MarkedAst>> kept;
      pg.enable_ast<MarkedAst>(true);
      pg["U"] = [&](const SemanticValues &vs) {
        auto node = std::any_cast<std::shared_ptr<MarkedAst>>(vs[0]);
        node->mark = 42;
        seen.push_back(node);
        if (keep) { kept.push_back(node); }
        return node;
      };
      std::shared_ptr<MarkedAst> ast;
      ASSERT_TRUE(pg.parse("1,2", ast));
      EXPECT_EQ(keep ? "N/T 0\nN/T 0\n" : "N/T 42 seen\nN/T 42 seen\n",
                marked_children(ast, seen))
          << "packrat=" << packrat << " keep=" << keep;
    }
  }
}

// A user action that hands up a node from below the one it is given, as a
// leave handler can: the node in between goes, and the collapsing rule above
// takes the one handed up over. With or without a tracer, and whether a
// callback saw that node before or not.
TEST(AstObservationTest, Collapse_takes_over_a_node_handed_up_from_below) {
  for (auto packrat : {false, true}) {
    for (auto via_leave : {false, true}) {
      for (auto tracer : {false, true}) {
        for (auto seen_before : {false, true}) {
          parser pg(R"(
          S <- T (',' T)*
          T <- U
          U <- W
          W <- '(' N ')' { no_ast_opt }
          N <- < [0-9]+ >
        )");
          if (packrat) { pg.enable_packrat_parsing(); }
          pg.enable_ast<MarkedAst>(true);
          if (tracer) {
            pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
          }
          if (seen_before) {
            pg["N"].leave = [](const Context &, const char *, size_t, size_t,
                               std::any &, std::any &) {};
          }
          std::vector<std::weak_ptr<MarkedAst>> seen;
          auto unwrap = [&](const std::shared_ptr<MarkedAst> &w) {
            auto child = w->nodes[0];
            child->mark = 42;
            seen.push_back(child);
            return child;
          };
          if (via_leave) {
            pg["W"].leave = [&](const Context &, const char *, size_t,
                                size_t len, std::any &value, std::any &) {
              if (success(len)) {
                value =
                    unwrap(std::any_cast<std::shared_ptr<MarkedAst>>(value));
              }
            };
          } else {
            pg["U"] = [&](const SemanticValues &vs) {
              return unwrap(std::any_cast<std::shared_ptr<MarkedAst>>(vs[0]));
            };
          }
          std::shared_ptr<MarkedAst> ast;
          ASSERT_TRUE(pg.parse("(1),(2)", ast));
          EXPECT_EQ("N/T 42 seen\nN/T 42 seen\n", marked_children(ast, seen))
              << "packrat=" << packrat << " via_leave=" << via_leave
              << " tracer=" << tracer << " seen_before=" << seen_before;
        }
      }
    }
  }
}

// A user calls the AST action of T from an action of their own: it takes over
// the node it is given as it does when the parse calls it, also where that
// node is one a rule below collapsed over already.
TEST(AstObservationTest, Ast_action_called_by_a_user_action) {
  for (auto packrat : {false, true}) {
    for (const char *grammar :
         {"S <- T (',' T)*\n T <- N\n N <- < [0-9]+ >", list_grammar}) {
      parser pg(grammar);
      if (packrat) { pg.enable_packrat_parsing(); }
      std::vector<std::weak_ptr<MarkedAst>> seen;
      pg["N"].leave = [&](const Context &, const char *, size_t, size_t len,
                          std::any &value, std::any &) {
        if (!success(len)) { return; }
        auto node = std::any_cast<std::shared_ptr<MarkedAst>>(value);
        node->mark = 42;
        seen.push_back(node);
      };
      pg.enable_ast<MarkedAst>(true);
      std::string uses;
      auto ast_action = pg["T"].action;
      pg["T"] = [&, ast_action](SemanticValues &vs, std::any &dt) {
        uses += std::to_string(
                    std::any_cast<const std::shared_ptr<MarkedAst> &>(vs[0])
                        .use_count()) +
                " ";
        return ast_action(vs, dt, std::any());
      };
      std::shared_ptr<MarkedAst> ast;
      ASSERT_TRUE(pg.parse("1,2", ast));
      EXPECT_EQ("1 1 ", uses) << "packrat=" << packrat << " " << grammar;
      EXPECT_EQ("N/T 42 seen\nN/T 42 seen\n", marked_children(ast, seen))
          << "packrat=" << packrat << " " << grammar;
    }
  }
}

// A collapsing rule renames the node it takes over when it matches: a later
// callback sees the new name on the node an earlier one saw.
TEST(AstObservationTest, Collapse_renames_a_node_when_its_rule_matches) {
  for (auto packrat : {false, true}) {
    for (const char *grammar : {"S <- T X\n T <- N\n N <- < [0-9]+ >\n"
                                " X <- < 'x' >",
                                "S <- T X\n T <- U\n U <- N\n"
                                " N <- < [0-9]+ >\n X <- < 'x' >"}) {
      parser pg(grammar);
      if (packrat) { pg.enable_packrat_parsing(); }
      std::weak_ptr<Ast> seen;
      std::string names;
      pg["N"].leave = [&](const Context &, const char *, size_t, size_t len,
                          std::any &value, std::any &) {
        if (success(len)) { seen = std::any_cast<std::shared_ptr<Ast>>(value); }
      };
      pg["X"].leave = [&](const Context &, const char *, size_t, size_t len,
                          std::any &, std::any &) {
        if (!success(len)) { return; }
        auto node = seen.lock();
        names = node ? node->name + "/" + node->original_name : "gone";
      };
      pg.enable_ast(true);
      std::shared_ptr<Ast> ast;
      ASSERT_TRUE(pg.parse("1x", ast))
          << "packrat=" << packrat << " " << grammar;
      EXPECT_EQ("N/T", names) << "packrat=" << packrat << " " << grammar;
      EXPECT_EQ(seen.lock(), ast->nodes[0])
          << "packrat=" << packrat << " " << grammar;
    }
  }
}

// Packrat hands the same empty match of Z to A and to B. Each gets a node of
// its own: B the one the leave handler saw, A a copy, as the tree is linked
// again from the last child first.
TEST(AstObservationTest, Empty_match_under_two_parents) {
  parser pg(R"(
    S <- A B
    A <- 'a' Z
    B <- Z 'b' / Z 'c'
    Z <- 'x'*
  )");
  std::shared_ptr<Ast> seen;
  pg["Z"].leave = [&](const Context &, const char *, size_t, size_t len,
                      std::any &value, std::any &) {
    if (success(len) && !seen) {
      seen = std::any_cast<std::shared_ptr<Ast>>(value);
    }
  };
  pg.enable_packrat_parsing();
  pg.enable_ast();
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse("ab", ast));
  EXPECT_EQ("+ S\n  + A\n    - Z ()\n  + B/0\n    - Z ()\n", ast_to_s(ast));
  EXPECT_NE(seen, ast->nodes[0]->nodes[0]);
  EXPECT_EQ(seen, ast->nodes[1]->nodes[0]);
  EXPECT_EQ(ast->nodes[1], seen->parent.lock());
}

// Packrat hands the match of R to Q again after P's alternative failed. A
// predicate on Q sees the node P's did. Without one, that node is the tree's,
// unless the rules collapse: as R's memo keeps it, Q copies it.
TEST(AstObservationTest, Memoized_match_seen_again_is_the_same_node) {
  for (auto collapse : {false, true}) {
    for (auto q_sees : {false, true}) {
      parser pg(R"(
        S <- P 'x' / Q 'y'
        P <- R
        Q <- R
        R <- < [0-9]+ >
      )");
      pg.enable_packrat_parsing();
      pg.enable_ast(collapse);
      std::weak_ptr<Ast> seen_by_p;
      std::string seen_by_q;
      pg["P"].predicate = [&](const SemanticValues &vs, const std::any &,
                              std::string &) {
        seen_by_p = std::any_cast<std::shared_ptr<Ast>>(vs[0]);
        return true;
      };
      if (q_sees) {
        pg["Q"].predicate = [&](const SemanticValues &vs, const std::any &,
                                std::string &) {
          auto node = std::any_cast<std::shared_ptr<Ast>>(vs[0]);
          seen_by_q = node == seen_by_p.lock() ? "same" : "other";
          return true;
        };
      }
      std::shared_ptr<Ast> ast;
      ASSERT_TRUE(pg.parse("1y", ast))
          << "collapse=" << collapse << " q_sees=" << q_sees;
      if (q_sees) {
        EXPECT_EQ("same", seen_by_q) << "collapse=" << collapse;
      } else {
        auto r = collapse ? ast : ast->nodes[0]->nodes[0];
        EXPECT_EQ(!collapse, r == seen_by_p.lock()) << "collapse=" << collapse;
      }
    }
  }
}

// R hands A's node up as its value, with its action disabled or with none:
// packrat reuses R's match, after the first alternative failed, with R's own
// length.
TEST(AstObservationTest, Memo_of_a_rule_that_hands_a_node_up) {
  for (auto disabled : {true, false}) {
    parser pg(R"(
      S <- R 'x' / R 'y'
      R <- A ';'
      A <- < [a-z]+ >
    )");
    pg.enable_packrat_parsing();
    if (disabled) {
      pg.enable_ast();
      pg["R"].disable_action = true;
    } else {
      add_ast_action(pg["S"]);
      add_ast_action(pg["A"]);
    }
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("ab;y", ast)) << "disabled=" << disabled;
    EXPECT_EQ("+ S/1\n  - A (ab)\n", ast_to_s(ast)) << "disabled=" << disabled;
  }
}

// A collapses over the node of B that a leave handler saw, so packrat keeps
// that node for A's match. The second alternative records E's match before it
// reuses A's: were A's memo not to keep its record, E's would take its place.
TEST(AstObservationTest, Memo_of_a_node_a_collapsing_rule_took_over) {
  parser pg(R"(
    S <- A 'x' / D
    D <- E A 'y'
    E <- Z
    Z <- ''
    A <- B
    B <- < [0-9]+ >
  )");
  pg.enable_packrat_parsing();
  pg.enable_ast(true);
  pg["B"].leave = [](const Context &, const char *, size_t, size_t, std::any &,
                     std::any &) {};
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse("12y", ast));
  EXPECT_EQ("+ S/1[D]\n  - E[Z] ()\n  - A[B] (12)\n", ast_to_s(ast));
}

// An empty match of Z under T, which collapses over it, and under B. The two
// stay apart: T's node is renamed T and B's is not. With packrat, Z's memo
// keeps its node, so T copies it.
TEST(AstObservationTest, Empty_match_under_a_collapsing_parent) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S <- A B
      A <- W T
      B <- Z W 'b' / Z W 'c'
      T <- Z
      W <- < 'w' >
      Z <- 'x'*
    )");
    if (packrat) { pg.enable_packrat_parsing(); }
    pg.enable_ast(true);
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("wwb", ast));
    EXPECT_EQ("+ S\n  + A\n    - W (w)\n    - T[Z] ()\n  + B/0\n    - Z ()\n"
              "    - W (w)\n",
              ast_to_s(ast))
        << "packrat=" << packrat;
    EXPECT_NE(ast->nodes[0]->nodes[1], ast->nodes[1]->nodes[0])
        << "packrat=" << packrat;
  }
}

// A collapsing rule over the node of a no_ast_opt rule leaves that node's
// position and length as they are, whether it takes the node over or, the
// node being kept, copies it.
TEST(AstObservationTest, Collapse_keeps_the_span_of_a_no_ast_opt_node) {
  for (auto packrat : {false, true}) {
    for (auto keep : {false, true}) {
      parser pg(R"(
      S <- '(' Y ')'
      Y <- 'y' N { no_ast_opt }
      N <- < [0-9] >
    )");
      if (packrat) { pg.enable_packrat_parsing(); }
      std::vector<std::shared_ptr<Ast>> kept;
      pg["Y"].leave = [&](const Context &, const char *, size_t, size_t len,
                          std::any &value, std::any &) {
        if (success(len) && keep) {
          kept.push_back(std::any_cast<std::shared_ptr<Ast>>(value));
        }
      };
      pg.enable_ast(true);
      std::shared_ptr<Ast> ast;
      ASSERT_TRUE(pg.parse("(y1)", ast));
      EXPECT_EQ("Y/S 1+2", ast->name + "/" + ast->original_name + " " +
                               std::to_string(ast->position) + "+" +
                               std::to_string(ast->length))
          << "packrat=" << packrat << " keep=" << keep;
    }
  }
}

// A leave handler can put another node in place of its rule's, and the tree
// has that one. A value that is no node makes the AST rule above it throw,
// as soon as that rule matches.
TEST(AstObservationTest, Leave_handler_replaces_the_value) {
  for (auto packrat : {false, true}) {
    {
      parser pg("S <- N (',' N)*\n N <- < [0-9]+ >");
      if (packrat) { pg.enable_packrat_parsing(); }
      pg.enable_ast(true);
      pg["N"].leave = [](const Context &, const char *, size_t, size_t len,
                         std::any &value, std::any &) {
        if (success(len)) {
          value = std::make_shared<Ast>("", 1, 1, "R", std::string_view("r"));
        }
      };
      std::shared_ptr<Ast> ast;
      ASSERT_TRUE(pg.parse("1,2", ast));
      EXPECT_EQ("+ S\n  - R (r)\n  - R (r)\n", ast_to_s(ast))
          << "packrat=" << packrat;
    }
    {
      parser pg("S <- P Q\n P <- N\n Q <- N\n N <- < [0-9] >");
      if (packrat) { pg.enable_packrat_parsing(); }
      pg.enable_ast(true);
      std::string entered;
      pg["N"].leave = [](const Context &, const char *, size_t, size_t len,
                         std::any &value, std::any &) {
        if (success(len)) { value = 1; }
      };
      for (auto name : {"P", "Q"}) {
        pg[name].enter = [&, name](const Context &, const char *, size_t,
                                   std::any &) { entered += name; };
      }
      std::shared_ptr<Ast> ast;
      EXPECT_THROW(pg.parse("12", ast), std::bad_any_cast)
          << "packrat=" << packrat;
      EXPECT_EQ("P", entered) << "packrat=" << packrat;
    }
  }
}

// A value that is no node under an AST rule, or the nothing that N hands on
// with its action disabled, makes it throw as soon as it matches, whether the
// parse is asked for the tree or not; so does a node of another type.
TEST(AstObservationTest, Ast_rule_throws_on_a_value_that_is_no_node) {
  for (auto packrat : {false, true}) {
    for (auto disabled : {false, true}) {
      for (auto takes_tree : {false, true}) {
        parser pg("S <- P Q\n P <- N N\n Q <- N\n N <- < [0-9] >");
        if (packrat) { pg.enable_packrat_parsing(); }
        pg.enable_ast();
        if (disabled) {
          pg["N"].disable_action = true;
        } else {
          pg["N"] = [](const SemanticValues &vs) {
            return vs.token_to_number<int>();
          };
        }
        std::string entered;
        for (auto name : {"P", "Q"}) {
          pg[name].enter = [&, name](const Context &, const char *, size_t,
                                     std::any &) { entered += name; };
        }
        std::shared_ptr<Ast> ast;
        EXPECT_THROW(takes_tree ? pg.parse("123", ast) : pg.parse("123"),
                     std::bad_any_cast)
            << "packrat=" << packrat << " disabled=" << disabled
            << " takes_tree=" << takes_tree;
        EXPECT_EQ("P", entered)
            << "packrat=" << packrat << " disabled=" << disabled
            << " takes_tree=" << takes_tree;
      }
    }
    {
      Definition S, A;
      S <= seq(A, A);
      A <= chr('a');
      S.name = "S";
      A.name = "A";
      S.enablePackratParsing = packrat;
      add_ast_action<Ast>(S);
      add_ast_action<MarkedAst>(A);
      std::shared_ptr<Ast> ast;
      EXPECT_THROW(S.parse_and_get_value("aa", ast), std::bad_any_cast)
          << "packrat=" << packrat;
    }
  }
}

// A rule with its action disabled hands on its first value, or nothing where
// it has none: in a lookahead, where nothing reads the AST rule over it, that
// rule throws on it all the same, as it does with a tracer. Only a precedence
// rule of the grammar, which always has an operand, is known to hand on one.
TEST(AstObservationTest, Ast_rule_throws_on_nothing_handed_on_in_a_lookahead) {
  struct {
    const char *grammar;
    const char *input;
  } cases[] = {
      // F has no values.
      {"S <- &E C C ';'\n E <- F\n F <- < [a-z]+ >\n C <- < [a-z] >", "ab;"},
      // F has a value on one alternative only.
      {"S <- &P C C C\n P <- F F\n F <- N / [a-z]\n N <- < [0-9] >\n"
       " C <- < . >",
       "ab1"},
  };
  for (const auto &[grammar, input] : cases) {
    for (auto packrat : {false, true}) {
      for (auto tracer : {false, true}) {
        parser pg(grammar);
        ASSERT_TRUE(pg) << grammar;
        if (packrat) { pg.enable_packrat_parsing(); }
        pg.enable_ast();
        pg["F"].disable_action = true;
        if (tracer) {
          pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
        }
        std::shared_ptr<Ast> ast;
        EXPECT_THROW(pg.parse(input, ast), std::bad_any_cast)
            << grammar << " packrat=" << packrat << " tracer=" << tracer;
      }
    }
  }
  // A precedence that is not the rule's body, or whose operand is no rule,
  // can hand on nothing.
  for (auto shape : {"precedence in a choice", "operand that is no rule"}) {
    for (auto tracer : {false, true}) {
      Definition S, X, E, A, N;
      PrecedenceClimbing::BinOpeInfo info{{"+", {1, 'L'}}};
      S <= seq(apd(X), N, chr(';'));
      X <= seq(E);
      if (std::string(shape) == "precedence in a choice") {
        E <= cho(pre(A, tok(chr('+')), info, E), chr('x'));
      } else {
        E <= pre(opt(A), tok(chr('+')), info, E);
      }
      A <= tok(oom(cls("0-9")));
      N <= tok(oom(cls("a-z")));
      S.name = "S";
      X.name = "X";
      E.name = "E";
      A.name = "A";
      N.name = "N";
      for (auto *rule : {&S, &X, &E, &A, &N}) {
        add_ast_action(*rule);
      }
      E.disable_action = true;
      if (tracer) {
        S.tracer_enter = [](auto &&...) {};
        S.tracer_leave = [](auto &&...) {};
      }
      EXPECT_THROW(S.parse("x;"), std::bad_any_cast)
          << shape << " tracer=" << tracer;
    }
  }
}

// After a recovery no value is built: callbacks see none where nodes would
// be, and the parse leaves the tree it was given as it was, as one that fails
// does.
TEST(AstObservationTest, Callbacks_see_no_values_after_a_recovery) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S    <- ITEM+
      ITEM <- N ';'^semi
      N    <- < [0-9]+ >
      semi <- (!';' .)* ';'?
    )");
    if (packrat) { pg.enable_packrat_parsing(); }
    std::string log;
    pg["ITEM"].predicate = [&](const SemanticValues &vs, const std::any &,
                               std::string &) {
      log += "P" + std::to_string(vs.size()) +
             (vs.size() && vs[0].has_value() ? "+ " : "- ");
      return true;
    };
    for (auto name : {"ITEM", "N", "S"}) {
      pg[name].leave = [&, name](const Context &, const char *, size_t,
                                 size_t len, std::any &value, std::any &) {
        if (success(len)) {
          log += std::string(name) + (value.has_value() ? "+ " : "- ");
        }
      };
    }
    pg.enable_ast();
    auto given = std::make_shared<Ast>("", 1, 1, "GIVEN", std::string_view());
    auto ast = given;
    EXPECT_FALSE(pg.parse("1;2 3;4;", ast)) << "packrat=" << packrat;
    EXPECT_EQ(given, ast) << "packrat=" << packrat;
    EXPECT_EQ("N+ P1+ ITEM+ N+ P1+ ITEM- N- P1- ITEM- S- ", log)
        << "packrat=" << packrat;
    EXPECT_FALSE(pg.parse("1;x", ast)) << "packrat=" << packrat;
    EXPECT_EQ(given, ast) << "packrat=" << packrat;
  }
}

// A precedence rule with no operator to fold passes its operand's node up as
// its own value.
TEST(AstObservationTest, Precedence_rule_passes_up_a_lone_operand) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      E  <- T (OP T)* { precedence L + }
      T  <- < [0-9] >
      OP <- < '+' >
    )");
    if (packrat) { pg.enable_packrat_parsing(); }
    pg.enable_ast();
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("1", ast)) << "packrat=" << packrat;
    EXPECT_EQ("- T (1)\n", ast_to_s(ast)) << "packrat=" << packrat;
    ASSERT_TRUE(pg.parse("1+2", ast)) << "packrat=" << packrat;
    EXPECT_EQ("+ E\n  - T (1)\n  - OP (+)\n  - T (2)\n", ast_to_s(ast))
        << "packrat=" << packrat;
  }
}

// A user action on a precedence rule sees the nodes of the operands and the
// operator it folds.
TEST(AstObservationTest, Precedence_action_sees_the_nodes_it_folds) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      E  <- T (OP T)* { precedence L + - }
      T  <- < [0-9] >
      OP <- < [-+] >
    )");
    if (packrat) { pg.enable_packrat_parsing(); }
    pg.enable_ast();
    std::string seen;
    pg["E"] = [&](const SemanticValues &vs) {
      auto nodes = vs.transform<std::shared_ptr<Ast>>();
      for (const auto &node : nodes) {
        seen += node->original_name + " ";
      }
      seen += "| ";
      return std::make_shared<Ast>("", 1, 1, "FOLD", nodes, 0, vs.sv().size());
    };
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("1+2-3", ast)) << "packrat=" << packrat;
    EXPECT_EQ("T OP T | FOLD OP T | ", seen) << "packrat=" << packrat;
    EXPECT_EQ("+ FOLD\n  + FOLD\n    - T (1)\n    - OP (+)\n    - T (2)\n"
              "  - OP (-)\n  - T (3)\n",
              ast_to_s(ast))
        << "packrat=" << packrat;
  }
}

// A precedence macro parsed where nothing reads its value leaves nothing in
// the tree.
TEST(AstObservationTest, Precedence_macro_in_a_rule_whose_value_is_unread) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S  <- A R B
      ~R <- INFIX(N, OP)
      INFIX(X, O) <- X (O X)* { precedence L + }
      A  <- < 'a' >
      B  <- < 'b' >
      N  <- < [0-9] >
      OP <- < '+' >
    )");
    if (packrat) { pg.enable_packrat_parsing(); }
    pg.enable_ast();
    // Without an action, R passes up the macro's value, which nothing reads.
    pg["R"].action = Action();
    for (auto src : {"a1b", "a1+2b"}) {
      std::shared_ptr<Ast> ast;
      ASSERT_TRUE(pg.parse(src, ast)) << "packrat=" << packrat << " " << src;
      EXPECT_EQ("+ S\n  - A (a)\n  - B (b)\n", ast_to_s(ast))
          << "packrat=" << packrat << " " << src;
    }
  }
}

// A user operator can put a node of its own among the children of an AST
// rule.
TEST(AstObservationTest, User_operator_puts_a_node_among_the_children) {
  Definition S, A, B;
  S <= seq(A,
           usr([](const char *, size_t, SemanticValues &vs,
                  std::any &) -> size_t {
             vs.emplace_back(std::make_shared<Ast>("", 1, 1, "USER",
                                                   std::string_view("u")));
             vs.tags.emplace_back(0);
             return 0;
           }),
           B);
  A <= chr('a');
  B <= chr('b');
  S.name = "S";
  A.name = "A";
  B.name = "B";
  add_ast_action(S);
  add_ast_action(A);
  add_ast_action(B);
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(S.parse_and_get_value("ab", ast).ret);
  EXPECT_EQ("+ S\n  - A (a)\n  - USER (u)\n  - B (b)\n", ast_to_s(ast));
}

// A tracer sees nodes, never anything else, among the values of the scopes.
TEST(AstObservationTest, Tracer_sees_nodes_in_the_scopes) {
  parser pg(list_grammar);
  pg.enable_ast(true);
  size_t nodes = 0;
  auto count = [&](const SemanticValues &vs) {
    for (const auto &v : vs) {
      EXPECT_TRUE(std::any_cast<std::shared_ptr<Ast>>(&v) != nullptr);
      nodes++;
    }
  };
  pg.enable_trace([&](const Ope &, const char *, size_t,
                      const SemanticValues &vs, const Context &,
                      const std::any &, std::any &) { count(vs); },
                  [&](const Ope &, const char *, size_t,
                      const SemanticValues &vs, const Context &,
                      const std::any &, size_t, std::any &) { count(vs); });
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse("1,2", ast));
  EXPECT_LT(0u, nodes);
}

// An AST action copied to another rule names its nodes after the rule it was
// made for.
TEST(AstObservationTest, Ast_action_copied_to_another_rule) {
  for (auto packrat : {false, true}) {
    parser pg(R"(
      S <- A B
      A <- 'a'
      B <- 'b' 'c'
    )");
    if (packrat) { pg.enable_packrat_parsing(); }
    pg.enable_ast();
    pg["B"].action = pg["A"].action;
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("abc", ast)) << "packrat=" << packrat;
    EXPECT_EQ("+ S\n  - A (a)\n  - A (bc)\n", ast_to_s(ast))
        << "packrat=" << packrat;
  }
}

// The tree of a rule parsed directly, which no parser links again: its
// fields and its parents.
TEST(AstObservationTest, Tree_of_a_rule_parsed_directly) {
  for (auto packrat : {false, true}) {
    Definition LIST, ITEM, NUM, NAME, SP;
    LIST <= seq(chr('('), ITEM, zom(seq(chr(','), ITEM)), chr(')'));
    ITEM <= cho(NUM, NAME);
    NUM <= seq(tok(oom(cls("0-9"))), SP);
    NAME <= seq(tok(oom(cls("a-z"))), SP);
    SP <= zom(cls(" \n"));
    LIST.name = "LIST";
    ITEM.name = "ITEM";
    NUM.name = "NUM";
    NAME.name = "NAME";
    SP.name = "SP";
    ~SP;
    LIST.enablePackratParsing = packrat;
    add_ast_action(LIST);
    add_ast_action(ITEM, true);
    add_ast_action(NUM);
    add_ast_action(NAME);
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(LIST.parse_and_get_value("(1 ,ab\n,2)", ast, "src").ret)
        << "packrat=" << packrat;
    NodeIds<Ast> ids;
    std::string out;
    dump_ast(ast, ids, out, true);
    EXPECT_EQ("#1 LIST|LIST|0+10|1:1|0/0|0/0|2656610/2656610|-|src|0\n"
              "  #2 NUM|ITEM|1+2|1:2|0/0|2/0|87542/2573269|'1'|src|0\n"
              "  #3 NAME|ITEM|4+3|1:5|0/0|2/1|2875623/2573269|'ab'|src|0\n"
              "  #4 NUM|ITEM|8+1|2:2|0/0|2/0|87542/2573269|'2'|src|0\n",
              out)
        << "packrat=" << packrat;
  }
}

// A parser that has parsed without callbacks gives the ones set afterwards
// what a parser that has them from the start does, also on rules whose
// matches nothing observed before (the ignored BANG and EXCL).
TEST(AstObservationTest, Callbacks_set_after_a_parse) {
  const char *src = "f(1, [2, a + b!], (3)); x = 2 ??; 42!";
  auto observe = [src](parser &pg) {
    NodeIds<Ast> ids;
    std::string out;
    auto see = [&](const std::any &value) {
      if (value.has_value()) {
        dump_ast(std::any_cast<std::shared_ptr<Ast>>(value), ids, out, false);
      }
    };
    pg["INNER"].predicate = [&](const SemanticValues &vs, const std::any &,
                                std::string &) {
      out += "P INNER\n";
      for (const auto &v : vs) {
        see(v);
      }
      return true;
    };
    for (std::string name : {"NUMBER", "BANG", "EXCL"}) {
      pg[name.c_str()].leave = [&, name](const Context &, const char *, size_t,
                                         size_t len, std::any &value,
                                         std::any &) {
        if (!success(len)) { return; }
        out += "L " + name + "\n";
        see(value);
      };
    }
    std::shared_ptr<Ast> ast;
    EXPECT_TRUE(pg.parse(src, ast));
    dump_ast(ast, ids, out, true);
    // The callbacks refer to this call's locals.
    pg["INNER"].predicate = Predicate();
    for (auto name : {"NUMBER", "BANG", "EXCL"}) {
      pg[name].leave = nullptr;
    }
    return out;
  };
  for (auto packrat : {false, true}) {
    parser fresh(precedence_grammar);
    parser reused(precedence_grammar);
    for (auto pg : {&fresh, &reused}) {
      if (packrat) { pg->enable_packrat_parsing(); }
      pg->enable_ast(true);
    }
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(reused.parse(src, ast));
    EXPECT_EQ(observe(fresh), observe(reused)) << "packrat=" << packrat;
  }
}

// A parse that backtracks drops the records of the matches it drops by a
// choice, a repetition or an option, and so ends with the records of its tree
// alone, as long as packrat memoizes none of the dropped matches. So does a
// parse where a leave handler sees the nodes of N: each N in its tree then has
// a record that holds its node among its parent's children, besides N's own
// record, or in place of it where N's memo holds the node.
TEST(AstObservationTest, Parse_keeps_no_records_of_dropped_matches) {
  std::function<size_t(const std::shared_ptr<Ast> &, const char *)> nodes =
      [&](const std::shared_ptr<Ast> &ast, const char *name) {
        size_t n = !name || ast->name == name;
        for (const auto &child : ast->nodes) {
          n += nodes(child, name);
        }
        return n;
      };
  const std::string nested = "((((1+2)*3)+4)*5)";
  struct {
    const char *grammar;
    std::string input;
    bool memoizes_n;
  } cases[] = {
      {"E <- M '+' E / M\n M <- T '*' M / T\n T <- '(' E ')' / N\n"
       " N <- < [0-9]+ >",
       nested, false},
      {"E <- (M '+')? M\n M <- (T '*')? T\n T <- '(' E ')' / N\n"
       " N <- < [0-9]+ >",
       nested, false},
      {"L <- (I ',')* I\n I <- '[' L ']' / N\n N <- < [0-9]+ >",
       "[1,[2,[3,[4,5]]]]", false},
      // With packrat, the second M of the first case reuses the match of the
      // first, so its choices drop nothing: here P's match is dropped, while
      // Q reuses N's.
      {"S <- P 'x' / Q 'y'\n P <- N N\n Q <- N N\n N <- < [0-9] >", "12y",
       true},
  };
  for (const auto &[grammar, input, memoizes_n] : cases) {
    for (auto seen : {false, true}) {
      parser pg(grammar);
      ASSERT_TRUE(pg) << grammar;
      pg.enable_packrat_parsing();
      pg.enable_ast();
      if (seen) {
        pg["N"].leave = [](const Context &, const char *, size_t, size_t,
                           std::any &, std::any &) {};
      }
      std::shared_ptr<Ast> ast;
      ASSERT_TRUE(pg.parse(input, ast)) << grammar;
      auto seen_nodes = seen ? nodes(ast, "N") : 0;
      EXPECT_EQ(nodes(ast, nullptr) + (memoizes_n ? 0 : seen_nodes),
                ast_records_at_end().records)
          << grammar << " seen=" << seen;
      EXPECT_EQ(seen_nodes, ast_records_at_end().externals)
          << grammar << " seen=" << seen;
    }
  }
}

// A parse records the matches of AST rules where its start rule has packrat
// parsing on or reaches a left-recursive rule, also where that rule is parsed
// directly. Any other parse builds each node as its rule matches.
TEST(AstObservationTest,
     Ast_matches_are_recorded_with_packrat_or_left_recursion) {
  auto records = [](parser &pg, const char *input) {
    std::shared_ptr<Ast> ast;
    EXPECT_TRUE(pg.parse(input, ast)) << input;
    return ast_records_at_end().records;
  };
  {
    parser pg("S <- A (',' A)*\n A <- < [a-z]+ >");
    pg.enable_ast();
    EXPECT_EQ(0u, records(pg, "a,b"));
    pg.enable_packrat_parsing();
    EXPECT_EQ(3u, records(pg, "a,b"));
    pg["S"].enablePackratParsing = false;
    EXPECT_EQ(0u, records(pg, "a,b"));

    // A rule parsed directly goes by its own flag.
    std::shared_ptr<Ast> ast;
    pg.enable_packrat_parsing();
    ASSERT_TRUE(pg["A"].parse_and_get_value("a", ast).ret);
    EXPECT_EQ(0u, ast_records_at_end().records);
    pg["A"].enablePackratParsing = true;
    ASSERT_TRUE(pg["A"].parse_and_get_value("a", ast).ret);
    EXPECT_EQ(1u, ast_records_at_end().records);
  }
  // Left recursion below the start rule, and in a macro alone.
  for (auto grammar : {"S <- E ';'\n E <- E '+' N / N\n N <- < [0-9] >",
                       "S <- L(N) ';'\n L(X) <- L(X) '+' X / X\n"
                       " N <- < [0-9] >"}) {
    parser pg(grammar);
    ASSERT_TRUE(pg) << grammar;
    pg.enable_ast();
    EXPECT_LT(0u, records(pg, "1+2;")) << grammar;
  }
}

// A chain of collapsing matches leaves one record for the collapsed levels,
// and the tree is the one a parse without records builds, also where a memo
// holds a level of the chain and a second parent reuses it.
TEST(AstObservationTest, Ast_collapsed_chain_is_recorded_once) {
  const char *chain = "S <- A\n A <- B\n B <- C\n C <- D\n D <- N\n"
                      " N <- < [0-9] >";
  // The collapsed levels below M are held by its memo.
  const char *shared = "S <- P 'x' / Q 'y'\n P <- M N\n Q <- M N\n M <- B\n"
                       " B <- C\n C <- < [0-9] >\n N <- < [a-z] >";
  auto tree = [](const char *grammar, const char *input, bool packrat) {
    parser pg(grammar);
    EXPECT_TRUE(pg) << grammar;
    if (packrat) { pg.enable_packrat_parsing(); }
    pg.enable_ast(true);
    std::shared_ptr<Ast> ast;
    EXPECT_TRUE(pg.parse(input, ast)) << grammar;
    return std::make_pair(ast_to_s(ast), ast_records_at_end().records);
  };
  EXPECT_EQ(2u, tree(chain, "1", true).second);
  for (auto [grammar, input] : {std::pair{chain, "1"}, std::pair{shared, "1ax"},
                                std::pair{shared, "1ay"}}) {
    EXPECT_EQ(tree(grammar, input, false).first,
              tree(grammar, input, true).first)
        << grammar << " " << input;
  }
}

// The array the records are kept in keeps its entries where they are as it
// grows, and keeps their values as it shrinks across a chunk's edge and
// grows again.
TEST(AstObservationTest, Chunked_array_keeps_its_entries) {
  constexpr size_t chunk = ChunkedArray<size_t>::kChunkSize;
  ChunkedArray<size_t> a;
  a.push_back(0);
  const auto *first = &a[0];
  for (size_t i = 1; i < 3 * chunk; i++) {
    a.push_back(i);
  }
  EXPECT_EQ(first, &a[0]);
  a.truncate(chunk - 1);
  for (size_t i = chunk - 1; i < chunk + 2; i++) {
    a.push_back(i);
  }
  ASSERT_EQ(chunk + 2, a.size());
  for (size_t i = 0; i < a.size(); i++) {
    EXPECT_EQ(i, a[i]) << i;
  }
}
