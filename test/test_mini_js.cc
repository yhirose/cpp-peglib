#include <gtest/gtest.h>
#include <peglib.h>

#include <fstream>
#include <string>

using namespace peg;

static std::string read_file(const std::string &path) {
  std::ifstream ifs(path, std::ios::in | std::ios::binary);
  if (!ifs) { throw std::runtime_error("cannot open " + path); }
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());
}

static std::string find_grammar() {
  for (auto prefix : {"", "../", "../../"}) {
    auto path = std::string(prefix) + "spec/mini-js/grammar.peg";
    std::ifstream ifs(path);
    if (ifs.good()) return path;
  }
  return "spec/mini-js/grammar.peg";
}

class MiniJsTest : public ::testing::Test {
protected:
  static parser *pg;

  static void SetUpTestSuite() {
    auto grammar = read_file(find_grammar());
    pg = new parser(grammar);
    ASSERT_TRUE(*pg) << "mini-js grammar failed to compile";
    pg->enable_packrat_parsing();
  }

  static void TearDownTestSuite() {
    delete pg;
    pg = nullptr;
  }

  bool parse(const char *input) { return pg->parse(input); }
};

parser *MiniJsTest::pg = nullptr;

// --- Statements ---

TEST_F(MiniJsTest, empty_program) { EXPECT_TRUE(parse("")); }
TEST_F(MiniJsTest, let_number) { EXPECT_TRUE(parse("let x = 42;")); }
TEST_F(MiniJsTest, let_string) { EXPECT_TRUE(parse("let s = 'hello';")); }

// --- Arithmetic / precedence ---

TEST_F(MiniJsTest, arithmetic_precedence) {
  EXPECT_TRUE(parse("let x = 2 + 3 * 4;"));
}
TEST_F(MiniJsTest, arithmetic_left_assoc) {
  EXPECT_TRUE(parse("let x = 10 - 3 - 2;"));
}
TEST_F(MiniJsTest, parentheses) { EXPECT_TRUE(parse("let x = (2 + 3) * 4;")); }
TEST_F(MiniJsTest, deep_parens) {
  EXPECT_TRUE(parse("let x = ((((1 + 2) * 3) - 4) / 5);"));
}
TEST_F(MiniJsTest, modulo) { EXPECT_TRUE(parse("let x = 10 % 3;")); }
TEST_F(MiniJsTest, ternary) { EXPECT_TRUE(parse("let x = 1 < 2 ? 10 : 20;")); }
TEST_F(MiniJsTest, logical_operators) {
  EXPECT_TRUE(parse("let x = true && false || true;"));
}
TEST_F(MiniJsTest, unary_operators) {
  EXPECT_TRUE(parse("let x = -3; let y = !false;"));
}

// --- Control flow ---

TEST_F(MiniJsTest, if_else) {
  EXPECT_TRUE(parse("if (true) { let x = 1; } else { let x = 2; }"));
}
TEST_F(MiniJsTest, while_loop) {
  EXPECT_TRUE(parse("let i = 0; while (i < 5) { i = i + 1; }"));
}

// --- Functions ---

TEST_F(MiniJsTest, function_and_call) {
  EXPECT_TRUE(parse("function add(a, b) { return a + b; } let x = add(3, 4);"));
}
TEST_F(MiniJsTest, recursive_function) {
  EXPECT_TRUE(parse("function f(n) { if (n <= 1) { return 1; } return n * f(n "
                    "- 1); } let x = f(5);"));
}
TEST_F(MiniJsTest, nested_calls) {
  EXPECT_TRUE(parse("function f(x) { return x * 2; } function g(x) { return "
                    "f(x) + 1; } let r = g(3);"));
}
TEST_F(MiniJsTest, empty_function) {
  EXPECT_TRUE(parse("function noop() {} noop();"));
}

// --- Arrays / member access ---

TEST_F(MiniJsTest, array_literal) { EXPECT_TRUE(parse("let a = [1, 2, 3];")); }
TEST_F(MiniJsTest, array_index) {
  EXPECT_TRUE(parse("let a = [10, 20, 30]; let x = a[1];"));
}
TEST_F(MiniJsTest, member_access) {
  EXPECT_TRUE(parse("let x = console.log;"));
}

// --- Template literals ---

TEST_F(MiniJsTest, template_literal) {
  EXPECT_TRUE(parse("let s = `hello ${1 + 2}`;"));
}

// --- Unicode ---

TEST_F(MiniJsTest, unicode_identifier) {
  EXPECT_TRUE(parse(u8"let \u540D\u524D = 42;"));
}
TEST_F(MiniJsTest, unicode_string) {
  EXPECT_TRUE(parse(u8"let s = '\u3053\u3093\u306B\u3061\u306F';"));
}

// --- Comments ---

TEST_F(MiniJsTest, single_line_comment) {
  EXPECT_TRUE(parse("// comment\nlet x = 1;"));
}
TEST_F(MiniJsTest, multi_line_comment) {
  EXPECT_TRUE(parse("/* multi\nline */ let x = 1;"));
}

// --- Keyword boundary (%word) ---

TEST_F(MiniJsTest, keyword_boundary) { EXPECT_TRUE(parse("let letter = 1;")); }

// --- Error recovery ---

TEST_F(MiniJsTest, error_missing_semicolon) {
  EXPECT_FALSE(parse("let x = 1\nlet y = 2;"));
}
TEST_F(MiniJsTest, error_missing_brace) {
  EXPECT_FALSE(parse("if (true) { let x = 1;"));
}

// --- Exception handling ---

TEST_F(MiniJsTest, throw_statement) { EXPECT_TRUE(parse("throw 'oops';")); }
TEST_F(MiniJsTest, throw_expression) {
  EXPECT_TRUE(parse("throw new_error(42);"));
}
TEST_F(MiniJsTest, try_catch) {
  EXPECT_TRUE(parse("try { foo(); } catch (e) { log(e); }"));
}
TEST_F(MiniJsTest, try_catch_no_binding) {
  EXPECT_TRUE(parse("try { foo(); } catch { log('err'); }"));
}
TEST_F(MiniJsTest, try_finally) {
  EXPECT_TRUE(parse("try { foo(); } finally { cleanup(); }"));
}
TEST_F(MiniJsTest, try_catch_finally) {
  EXPECT_TRUE(
      parse("try { foo(); } catch (e) { log(e); } finally { cleanup(); }"));
}
TEST_F(MiniJsTest, nested_try) {
  EXPECT_TRUE(parse(
      "try { try { foo(); } catch (e) { throw e; } } catch (e) { log(e); }"));
}
TEST_F(MiniJsTest, throw_in_function) {
  EXPECT_TRUE(parse(
      "function bad() { throw 'err'; } try { bad(); } catch (e) { log(e); }"));
}
TEST_F(MiniJsTest, rethrow) {
  EXPECT_TRUE(parse("try { foo(); } catch (e) { throw e; }"));
}
TEST_F(MiniJsTest, error_try_no_handler) {
  EXPECT_FALSE(parse("try { foo(); }"));
}
TEST_F(MiniJsTest, error_catch_no_body) {
  EXPECT_FALSE(parse("try { foo(); } catch"));
}

// --- Immutability / mutability ---

TEST_F(MiniJsTest, let_immutable) { EXPECT_TRUE(parse("let x = 1;")); }
TEST_F(MiniJsTest, let_mutable) {
  EXPECT_TRUE(parse("let mut y = 1; y = y + 1;"));
}
TEST_F(MiniJsTest, let_with_type) { EXPECT_TRUE(parse("let x: int = 1;")); }

// --- Object literals ---

TEST_F(MiniJsTest, object_empty) { EXPECT_TRUE(parse("let o = {};")); }
TEST_F(MiniJsTest, object_literal) {
  EXPECT_TRUE(parse("let p = { x: 10, y: 20 };"));
}
TEST_F(MiniJsTest, object_string_key) {
  EXPECT_TRUE(parse("let o = { \"key\": 1 };"));
}
TEST_F(MiniJsTest, object_nested) {
  EXPECT_TRUE(parse("let o = { a: { b: 1 } };"));
}
TEST_F(MiniJsTest, object_assign_field) {
  EXPECT_TRUE(parse("let mut o = {x:1}; o.x = 2;"));
}

// --- Arrow functions (first-class) ---

TEST_F(MiniJsTest, arrow_noparen) { EXPECT_TRUE(parse("let f = x => x + 1;")); }
TEST_F(MiniJsTest, arrow_params) {
  EXPECT_TRUE(parse("let f = (x, y) => x + y;"));
}
TEST_F(MiniJsTest, arrow_block) {
  EXPECT_TRUE(parse("let f = (x) => { return x * 2; };"));
}
TEST_F(MiniJsTest, arrow_in_call) {
  EXPECT_TRUE(parse("map(arr, x => x * 2);"));
}
TEST_F(MiniJsTest, higher_order) {
  EXPECT_TRUE(parse("let apply = (f, x) => f(x); apply(y => y + 1, 10);"));
}

// --- Closures ---

TEST_F(MiniJsTest, closure_simple) {
  EXPECT_TRUE(parse("let make = () => { let n = 0; return () => n; };"));
}
TEST_F(MiniJsTest, closure_counter) {
  EXPECT_TRUE(parse("let c = () => { let mut n = 0; return () => { n = n + 1; "
                    "return n; }; };"));
}

// --- For-of ---

TEST_F(MiniJsTest, for_of) {
  EXPECT_TRUE(parse("for (let x of arr) { print(x); }"));
}
TEST_F(MiniJsTest, for_of_block) {
  EXPECT_TRUE(parse("for (let x of [1,2,3]) { sum = sum + x; }"));
}

// --- Break/continue ---

TEST_F(MiniJsTest, break_stmt) {
  EXPECT_TRUE(parse("while (true) { break; }"));
}
TEST_F(MiniJsTest, continue_stmt) {
  EXPECT_TRUE(parse("while (i < 10) { continue; }"));
}

// --- Floating point ---

TEST_F(MiniJsTest, float_literal) { EXPECT_TRUE(parse("let pi = 3.14;")); }
TEST_F(MiniJsTest, float_exp) { EXPECT_TRUE(parse("let big = 1.5e10;")); }
TEST_F(MiniJsTest, float_neg_exp) { EXPECT_TRUE(parse("let tiny = 1e-5;")); }

// --- Enum (tagged union) ---

TEST_F(MiniJsTest, enum_empty) {
  EXPECT_TRUE(parse("enum Color { Red, Green, Blue }"));
}
TEST_F(MiniJsTest, enum_with_data) {
  EXPECT_TRUE(parse("enum Option { Some(int), None }"));
}
TEST_F(MiniJsTest, enum_result) {
  EXPECT_TRUE(parse("enum Result { Ok(int), Err(string) }"));
}
TEST_F(MiniJsTest, enum_multi_payload) {
  EXPECT_TRUE(parse("enum Event { Click(int, int), Key(string) }"));
}

// --- Pattern matching ---

TEST_F(MiniJsTest, match_literal) {
  EXPECT_TRUE(parse("match (x) { 0 => 'zero', 1 => 'one', _ => 'many' }"));
}
TEST_F(MiniJsTest, match_variant) {
  EXPECT_TRUE(parse("match (opt) { Some(n) => n, None => 0 }"));
}
TEST_F(MiniJsTest, match_multi_payload) {
  EXPECT_TRUE(parse("match (ev) { Click(x, y) => move(x, y), Key(k) => "
                    "press(k), _ => skip() }"));
}
TEST_F(MiniJsTest, match_with_block) {
  EXPECT_TRUE(parse("match (r) { Ok(v) => { log(v); return v; }, Err(e) => { "
                    "log(e); return 0; } }"));
}

// --- Combined: Result pattern (error handling without exceptions) ---

TEST_F(MiniJsTest, result_pattern) {
  EXPECT_TRUE(
      parse("function divide(a, b) { if (b == 0) { return Err('div0'); } "
            "return Ok(a / b); } "
            "match (divide(10, 2)) { Ok(v) => print(v), Err(e) => print(e) }"));
}
