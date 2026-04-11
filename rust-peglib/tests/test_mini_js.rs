#[test]
fn mini_js_parse() {
    let grammar = std::fs::read_to_string("../spec/mini-js/grammar.peg").unwrap();
    let p = peglib::Parser::new(&grammar).expect("grammar compile failed");
    println!("grammar loaded OK");
    let inputs: &[(&str, &str, bool)] = &[
        ("empty_program", "", false),
        ("let_number", "let x = 42;", false),
        ("let_string", "let s = 'hello';", false),
        ("arithmetic_precedence", "let x = 2 + 3 * 4;", false),
        ("parentheses", "let x = (2 + 3) * 4;", false),
        ("function_and_call", "function add(a, b) { return a + b; } let x = add(3, 4);", false),
        ("if_else", "if (true) { let x = 1; } else { let x = 2; }", false),
        ("while_loop", "let i = 0; while (i < 5) { i = i + 1; }", false),
        ("array_literal", "let a = [1, 2, 3];", false),
        ("comment_single", "// comment\nlet x = 1;", false),
        ("comment_multi", "/* multi\nline */ let x = 1;", false),
        ("keyword_boundary", "let letter = 1;", false),
        ("template_literal", "let s = `hello ${1 + 2}`;", false),
        ("unicode_ident", "let \u{540D}\u{524D} = 42;", false),
        ("unicode_string", "let s = '\u{3053}\u{3093}\u{306B}\u{3061}\u{306F}';", false),
        ("recursive_func", "function f(n) { if (n <= 1) { return 1; } return n * f(n - 1); } let x = f(5);", false),
        ("nested_calls", "function f(x) { return x * 2; } function g(x) { return f(x) + 1; } let r = g(3);", false),
        ("ternary", "let x = 1 < 2 ? 10 : 20;", false),
        ("unary", "let x = -3; let y = !false;", false, ),
        ("modulo", "let x = 10 % 3;", false),
        ("deep_parens", "let x = ((((1 + 2) * 3) - 4) / 5);", false),
        ("empty_function", "function noop() {} noop();", false),
        ("array_index", "let a = [10, 20, 30]; let x = a[1];", false),
        ("member_access", "let x = console.log;", false),
        ("error_missing_semi", "let x = 1\nlet y = 2;", true),
        ("error_missing_brace", "if (true) { let x = 1;", true),
        ("throw_statement", "throw 'oops';", false),
        ("throw_expression", "throw new_error(42);", false),
        ("try_catch", "try { foo(); } catch (e) { log(e); }", false),
        ("try_catch_no_binding", "try { foo(); } catch { log('err'); }", false),
        ("try_finally", "try { foo(); } finally { cleanup(); }", false),
        ("try_catch_finally", "try { foo(); } catch (e) { log(e); } finally { cleanup(); }", false),
        ("nested_try", "try { try { foo(); } catch (e) { throw e; } } catch (e) { log(e); }", false),
        ("throw_in_function", "function bad() { throw 'err'; } try { bad(); } catch (e) { log(e); }", false),
        ("rethrow", "try { foo(); } catch (e) { throw e; }", false),
        ("error_try_no_handler", "try { foo(); }", true),
        ("error_catch_no_body", "try { foo(); } catch", true),

        // --- Immutability / mutability ---
        ("let_immutable", "let x = 1;", false),
        ("let_mutable", "let mut y = 1; y = y + 1;", false),
        ("let_with_type", "let x: int = 1;", false),

        // --- Object literals ---
        ("object_empty", "let o = {};", false),
        ("object_literal", "let p = { x: 10, y: 20 };", false),
        ("object_string_key", "let o = { \"key\": 1 };", false),
        ("object_nested", "let o = { a: { b: 1 } };", false),
        ("object_assign_field", "let mut o = {x:1}; o.x = 2;", false),

        // --- Arrow functions (first-class) ---
        ("arrow_noparen", "let f = x => x + 1;", false),
        ("arrow_params", "let f = (x, y) => x + y;", false),
        ("arrow_block", "let f = (x) => { return x * 2; };", false),
        ("arrow_in_call", "map(arr, x => x * 2);", false),
        ("higher_order", "let apply = (f, x) => f(x); apply(y => y + 1, 10);", false),

        // --- Closures ---
        ("closure_simple", "let make = () => { let n = 0; return () => n; };", false),
        ("closure_counter", "let c = () => { let mut n = 0; return () => { n = n + 1; return n; }; };", false),

        // --- For-of ---
        ("for_of", "for (let x of arr) { print(x); }", false),
        ("for_of_block", "for (let x of [1,2,3]) { sum = sum + x; }", false),

        // --- Break/continue ---
        ("break_stmt", "while (true) { break; }", false),
        ("continue_stmt", "while (i < 10) { continue; }", false),

        // --- Floating point ---
        ("float_literal", "let pi = 3.14;", false),
        ("float_exp", "let big = 1.5e10;", false),
        ("float_neg_exp", "let tiny = 1e-5;", false),

        // --- Enum (tagged union) ---
        ("enum_empty", "enum Color { Red, Green, Blue }", false),
        ("enum_with_data", "enum Option { Some(int), None }", false),
        ("enum_result", "enum Result { Ok(int), Err(string) }", false),
        ("enum_multi_payload", "enum Event { Click(int, int), Key(string) }", false),

        // --- Pattern matching ---
        ("match_literal", "match (x) { 0 => 'zero', 1 => 'one', _ => 'many' }", false),
        ("match_variant", "match (opt) { Some(n) => n, None => 0 }", false),
        ("match_multi_payload", "match (ev) { Click(x, y) => move(x, y), Key(k) => press(k), _ => skip() }", false),
        ("match_with_block", "match (r) { Ok(v) => { log(v); return v; }, Err(e) => { log(e); return 0; } }", false),

        // --- Combined: Result pattern ---
        ("result_pattern",
         "function divide(a, b) { if (b == 0) { return Err('div0'); } return Ok(a / b); } \
          match (divide(10, 2)) { Ok(v) => print(v), Err(e) => print(e) }",
         false),
    ];
    let mut pass = 0u32;
    let mut fail = 0u32;
    for (name, input, expect_fail) in inputs {
        let ok = p.parse(input);
        let correct = if *expect_fail { !ok } else { ok };
        if correct {
            pass += 1;
        } else {
            fail += 1;
            println!("FAIL {name} | parse={ok} expect_fail={expect_fail}");
        }
    }
    println!("{pass} passed, {fail} failed");
    assert_eq!(fail, 0, "{fail} mini-js parse tests failed");
}
