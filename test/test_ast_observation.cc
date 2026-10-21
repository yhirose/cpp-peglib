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

} // namespace

// A leave handler marks a node and keeps no hold on it: the collapsing rules
// above take that very node over, mark and all. One that keeps the node makes
// them copy it, and the copy starts unmarked.
TEST(AstObservationTest, Collapse_takes_over_a_node_a_leave_handler_marked) {
  for (auto keep : {false, true}) {
    parser pg(list_grammar);
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
        << "keep=" << keep;
    for (const auto &node : kept) {
      EXPECT_EQ("N", node->original_name);
    }
  }
}

// The same when a second leave handler looks at the collapsed node on the way
// up, with or without a tracer.
TEST(AstObservationTest, Collapse_takes_over_a_node_two_leave_handlers_saw) {
  for (auto tracer : {false, true}) {
    parser pg(list_grammar);
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
          std::to_string(
              std::any_cast<std::shared_ptr<MarkedAst> &>(value).use_count()) +
          " ";
    };
    pg.enable_ast<MarkedAst>(true);
    if (tracer) {
      pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
    }
    std::shared_ptr<MarkedAst> ast;
    ASSERT_TRUE(pg.parse("1,2", ast));
    EXPECT_EQ("1 1 ", uses) << "tracer=" << tracer;
    EXPECT_EQ("N/T 42 seen\nN/T 42 seen\n", marked_children(ast, seen))
        << "tracer=" << tracer;
  }
}

// A user action that passes up the node it is given: the collapsing rule
// above takes it over unless the action keeps it too.
TEST(AstObservationTest, Collapse_takes_over_a_node_a_user_action_passed_up) {
  for (auto keep : {false, true}) {
    parser pg(list_grammar);
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
        << "keep=" << keep;
  }
}

// A user action that hands up a node from below the one it is given, as a
// leave handler can: the node in between goes, and the collapsing rule above
// takes the one handed up over. With or without a tracer.
TEST(AstObservationTest, Collapse_takes_over_a_node_handed_up_from_below) {
  for (auto via_leave : {false, true}) {
    for (auto tracer : {false, true}) {
      parser pg(R"(
        S <- T (',' T)*
        T <- U
        U <- W
        W <- '(' N ')' { no_ast_opt }
        N <- < [0-9]+ >
      )");
      pg.enable_ast<MarkedAst>(true);
      if (tracer) {
        pg.enable_trace([](auto &&...) {}, [](auto &&...) {});
      }
      std::vector<std::weak_ptr<MarkedAst>> seen;
      auto unwrap = [&](const std::shared_ptr<MarkedAst> &w) {
        auto child = w->nodes[0];
        child->mark = 42;
        seen.push_back(child);
        return child;
      };
      if (via_leave) {
        pg["W"].leave = [&](const Context &, const char *, size_t, size_t len,
                            std::any &value, std::any &) {
          if (success(len)) {
            value = unwrap(std::any_cast<std::shared_ptr<MarkedAst>>(value));
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
          << "via_leave=" << via_leave << " tracer=" << tracer;
    }
  }
}

// A user calls the AST action of T from an action of their own: it takes over
// the node it is given as it does when the parse calls it, also where that
// node is one a rule below collapsed over already.
TEST(AstObservationTest, Ast_action_called_by_a_user_action) {
  for (const char *grammar :
       {"S <- T (',' T)*\n T <- N\n N <- < [0-9]+ >", list_grammar}) {
    parser pg(grammar);
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
    EXPECT_EQ("1 1 ", uses) << grammar;
    EXPECT_EQ("N/T 42 seen\nN/T 42 seen\n", marked_children(ast, seen))
        << grammar;
  }
}

// A collapsing rule renames the node it takes over when it matches: a later
// callback sees the new name on the node an earlier one saw.
TEST(AstObservationTest, Collapse_renames_a_node_when_its_rule_matches) {
  for (const char *grammar : {"S <- T X\n T <- N\n N <- < [0-9]+ >\n"
                              " X <- < 'x' >",
                              "S <- T X\n T <- U\n U <- N\n"
                              " N <- < [0-9]+ >\n X <- < 'x' >"}) {
    parser pg(grammar);
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
    ASSERT_TRUE(pg.parse("1x", ast));
    EXPECT_EQ("N/T", names) << grammar;
    EXPECT_EQ(seen.lock(), ast->nodes[0]) << grammar;
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
  for (auto keep : {false, true}) {
    parser pg(R"(
      S <- '(' Y ')'
      Y <- 'y' N { no_ast_opt }
      N <- < [0-9] >
    )");
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
        << "keep=" << keep;
  }
}

// A leave handler can put another node in place of its rule's, and the tree
// has that one. A value that is no node makes the AST rule above it throw,
// as soon as that rule matches.
TEST(AstObservationTest, Leave_handler_replaces_the_value) {
  {
    parser pg("S <- N (',' N)*\n N <- < [0-9]+ >");
    pg.enable_ast(true);
    pg["N"].leave = [](const Context &, const char *, size_t, size_t len,
                       std::any &value, std::any &) {
      if (success(len)) {
        value = std::make_shared<Ast>("", 1, 1, "R", std::string_view("r"));
      }
    };
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse("1,2", ast));
    EXPECT_EQ("+ S\n  - R (r)\n  - R (r)\n", ast_to_s(ast));
  }
  {
    parser pg("S <- P Q\n P <- N\n Q <- N\n N <- < [0-9] >");
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
    EXPECT_THROW(pg.parse("12", ast), std::bad_any_cast);
    EXPECT_EQ("P", entered);
  }
}

// A value that is no node under an AST rule makes it throw as soon as it
// matches; so does a node of another type.
TEST(AstObservationTest, Ast_rule_throws_on_a_value_that_is_no_node) {
  {
    parser pg("S <- P Q\n P <- N N\n Q <- N\n N <- < [0-9] >");
    pg.enable_ast();
    pg["N"] = [](const SemanticValues &vs) {
      return vs.token_to_number<int>();
    };
    std::string entered;
    for (auto name : {"P", "Q"}) {
      pg[name].enter = [&, name](const Context &, const char *, size_t,
                                 std::any &) { entered += name; };
    }
    std::shared_ptr<Ast> ast;
    EXPECT_THROW(pg.parse("123", ast), std::bad_any_cast);
    EXPECT_EQ("P", entered);
  }
  {
    Definition S, A;
    S <= seq(A, A);
    A <= chr('a');
    S.name = "S";
    A.name = "A";
    add_ast_action<Ast>(S);
    add_ast_action<MarkedAst>(A);
    std::shared_ptr<Ast> ast;
    EXPECT_THROW(S.parse_and_get_value("aa", ast), std::bad_any_cast);
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
  parser pg(R"(
    E  <- T (OP T)* { precedence L + }
    T  <- < [0-9] >
    OP <- < '+' >
  )");
  pg.enable_ast();
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse("1", ast));
  EXPECT_EQ("- T (1)\n", ast_to_s(ast));
  ASSERT_TRUE(pg.parse("1+2", ast));
  EXPECT_EQ("+ E\n  - T (1)\n  - OP (+)\n  - T (2)\n", ast_to_s(ast));
}

// A user action on a precedence rule sees the nodes of the operands and the
// operator it folds.
TEST(AstObservationTest, Precedence_action_sees_the_nodes_it_folds) {
  parser pg(R"(
    E  <- T (OP T)* { precedence L + - }
    T  <- < [0-9] >
    OP <- < [-+] >
  )");
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
  ASSERT_TRUE(pg.parse("1+2-3", ast));
  EXPECT_EQ("T OP T | FOLD OP T | ", seen);
  EXPECT_EQ("+ FOLD\n  + FOLD\n    - T (1)\n    - OP (+)\n    - T (2)\n"
            "  - OP (-)\n  - T (3)\n",
            ast_to_s(ast));
}

// A precedence macro parsed where nothing reads its value leaves nothing in
// the tree.
TEST(AstObservationTest, Precedence_macro_in_a_rule_whose_value_is_unread) {
  parser pg(R"(
    S  <- A R B
    ~R <- INFIX(N, OP)
    INFIX(X, O) <- X (O X)* { precedence L + }
    A  <- < 'a' >
    B  <- < 'b' >
    N  <- < [0-9] >
    OP <- < '+' >
  )");
  pg.enable_ast();
  // Without an action, R passes up the macro's value, which nothing reads.
  pg["R"].action = Action();
  for (auto src : {"a1b", "a1+2b"}) {
    std::shared_ptr<Ast> ast;
    ASSERT_TRUE(pg.parse(src, ast)) << src;
    EXPECT_EQ("+ S\n  - A (a)\n  - B (b)\n", ast_to_s(ast)) << src;
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
  parser pg(R"(
    S <- A B
    A <- 'a'
    B <- 'b' 'c'
  )");
  pg.enable_ast();
  pg["B"].action = pg["A"].action;
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(pg.parse("abc", ast));
  EXPECT_EQ("+ S\n  - A (a)\n  - A (bc)\n", ast_to_s(ast));
}

// The tree of a rule parsed directly, which no parser links again: its
// fields and its parents.
TEST(AstObservationTest, Tree_of_a_rule_parsed_directly) {
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
  add_ast_action(LIST);
  add_ast_action(ITEM, true);
  add_ast_action(NUM);
  add_ast_action(NAME);
  std::shared_ptr<Ast> ast;
  ASSERT_TRUE(LIST.parse_and_get_value("(1 ,ab\n,2)", ast, "src").ret);
  NodeIds<Ast> ids;
  std::string out;
  dump_ast(ast, ids, out, true);
  EXPECT_EQ("#1 LIST|LIST|0+10|1:1|0/0|0/0|2656610/2656610|-|src|0\n"
            "  #2 NUM|ITEM|1+2|1:2|0/0|2/0|87542/2573269|'1'|src|0\n"
            "  #3 NAME|ITEM|4+3|1:5|0/0|2/1|2875623/2573269|'ab'|src|0\n"
            "  #4 NUM|ITEM|8+1|2:2|0/0|2/0|87542/2573269|'2'|src|0\n",
            out);
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
