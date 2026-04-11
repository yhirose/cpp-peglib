use peglib::{Parser, ast_to_s, optimize_ast};

fn parse_ast<'a>(parser: &'a Parser, input: &'a str) -> peglib::Ast<'a> {
    let result = parser.parse_with_errors(input, true, None);
    assert!(result.ok, "parse failed for: {input}");
    result.ast.expect("no AST produced")
}

#[test]
fn backtracking_with_ast() {
    // test_core.cc: Backtracking_with_AST
    let parser = Parser::new(r#"
        S <- A? B (A B)* A
        A <- 'a'
        B <- 'b'
    "#).unwrap();
    let ast = parse_ast(&parser, "ba");
    assert_eq!(ast.nodes.len(), 2);
}

#[test]
fn literal_token_on_ast_test1() {
    // test_core.cc: Literal_token_on_AST_test1
    // Single token rule — entire match is a token
    let parser = Parser::new(r#"
        STRING_LITERAL <- '"' (('\\"' / '\\t' / '\\n') / (!["] .))* '"'
    "#).unwrap();
    let ast = parse_ast(&parser, r#""a\tb""#);
    assert!(ast.is_token);
    assert!(ast.nodes.is_empty());
}

#[test]
fn literal_token_on_ast_test2() {
    // test_core.cc: Literal_token_on_AST_test2
    // Non-token rule with child nodes
    let parser = Parser::new(r#"
        STRING_LITERAL <- '"' (ESC / CHAR)* '"'
        ESC            <- ('\\"' / '\\t' / '\\n')
        CHAR           <- (!["] .)
    "#).unwrap();
    let ast = parse_ast(&parser, r#""a\tb""#);
    assert!(!ast.is_token);
    assert_eq!(ast.nodes.len(), 3);
}

#[test]
fn literal_token_on_ast_test3() {
    // test_core.cc: Literal_token_on_AST_test3
    // Token boundary wraps entire rule
    let parser = Parser::new(r#"
        STRING_LITERAL <- < '"' (ESC / CHAR)* '"' >
        ESC            <- ('\\"' / '\\t' / '\\n')
        CHAR           <- (!["] .)
    "#).unwrap();
    let ast = parse_ast(&parser, r#""a\tb""#);
    assert!(ast.is_token);
    assert!(ast.nodes.is_empty());
}

#[test]
fn literal_token_on_ast_test4() {
    // test_core.cc: Literal_token_on_AST_test4
    // Nested token boundary — inner token is captured
    let parser = Parser::new(r#"
        STRING_LITERAL <- < '"' < (ESC / CHAR)* > '"' >
        ESC            <- ('\\"' / '\\t' / '\\n')
        CHAR           <- (!["] .)
    "#).unwrap();
    let ast = parse_ast(&parser, r#""a\tb""#);
    assert!(ast.is_token);
    assert!(ast.nodes.is_empty());
    // Inner token captures content without quotes
    assert_eq!(ast.tokens[0], r#"a\tb"#);
}

#[test]
fn single_child_optimization() {
    // test_integration.cc: Single_child_optimization
    let parser = Parser::new(r#"
        A <- B
        B <- C
        C <- < [a-z]+ >
    "#).unwrap();
    let ast = parse_ast(&parser, "hello");
    let opt = optimize_ast(ast);
    assert!(opt.is_token);
    assert_eq!(opt.tokens[0], "hello");
}

#[test]
fn optimization_with_multiple_children() {
    // test_integration.cc: Optimization_with_multiple_children
    let parser = Parser::new(r#"
        S <- A B C
        A <- 'a'
        B <- 'b'
        C <- 'c'
    "#).unwrap();
    let ast = parse_ast(&parser, "abc");
    let opt = optimize_ast(ast);
    assert_eq!(opt.nodes.len(), 3);
}

#[test]
fn ast_with_whitespace_and_token() {
    // test_integration.cc: AST_with_whitespace_and_token
    let parser = Parser::new(r#"
        S           <- ITEM (',' ITEM)*
        ITEM        <- < [a-z]+ >
        %whitespace <- [ \t]*
    "#).unwrap();
    let ast = parse_ast(&parser, "hello , world , foo");
    assert_eq!(ast.nodes.len(), 3);
    assert!(ast.nodes[0].is_token);
    assert_eq!(ast.nodes[0].tokens[0], "hello");
    assert!(ast.nodes[1].is_token);
    assert_eq!(ast.nodes[1].tokens[0], "world");
    assert!(ast.nodes[2].is_token);
    assert_eq!(ast.nodes[2].tokens[0], "foo");
}

#[test]
fn optimization_preserves_structure() {
    // test_integration.cc: Optimization_preserves_structure
    let parser = Parser::new(r#"
        EXPR   <- TERM ('+' TERM)*
        TERM   <- FACTOR ('*' FACTOR)*
        FACTOR <- < [0-9]+ >
    "#).unwrap();
    let ast = parse_ast(&parser, "1+2*3");
    let opt = optimize_ast(ast);
    assert!(!opt.nodes.is_empty());
}

#[test]
fn json_like_parser_with_ast() {
    // test_integration.cc: JSON_like_parser_with_AST
    let parser = Parser::new(r#"
        VALUE   <- OBJECT / ARRAY / STRING / NUMBER / BOOL / NULL
        OBJECT  <- '{' (PAIR (',' PAIR)*)? '}'
        PAIR    <- STRING ':' VALUE
        ARRAY   <- '[' (VALUE (',' VALUE)*)? ']'
        STRING  <- '"' < (!'"' .)* > '"'
        NUMBER  <- < '-'? [0-9]+ ('.' [0-9]+)? >
        BOOL    <- 'true' / 'false'
        NULL    <- 'null'
        %whitespace <- [ \t\r\n]*
    "#).unwrap();
    let ast = parse_ast(&parser, r#"{"a": 1, "b": [2, 3]}"#);
    assert!(!ast.nodes.is_empty());
}

#[test]
fn left_recursion_with_ast() {
    // test_integration.cc: Left_recursion_with_AST
    let parser = Parser::new(r#"
        EXPR <- EXPR '+' TERM / TERM
        TERM <- < [0-9]+ >
        %whitespace <- [ \t]*
    "#).unwrap();
    let ast = parse_ast(&parser, "1 + 2 + 3");
    assert!(!ast.nodes.is_empty());
}

#[test]
fn ast_to_s_format() {
    // Verify ast_to_s output format matches cpp-peglib
    let parser = Parser::new(r#"
        EXPR <- TERM ('+' TERM)*
        TERM <- < [0-9]+ >
    "#).unwrap();
    let ast = parse_ast(&parser, "1+2");
    let s = ast_to_s(&ast);
    assert!(s.contains("EXPR"));
    assert!(s.contains("TERM"));
    // Token nodes use "- name (value)" format
    assert!(s.contains("- "));
    // Non-token nodes use "+ name" format
    assert!(s.contains("+ "));
}

#[test]
fn optimize_ast_collapses_chain() {
    // A -> B -> C -> token should collapse to token with original_name = A
    let parser = Parser::new(r#"
        A <- B
        B <- C
        C <- < [a-z]+ >
    "#).unwrap();
    let ast = parse_ast(&parser, "test");
    let opt = optimize_ast(ast);
    assert_eq!(opt.original_name, "A");
    assert_eq!(opt.name, "C");
    assert!(opt.is_token);
}

#[test]
fn no_ast_opt_preserves_node() {
    // no_ast_opt prevents a node from collapsing its single child
    // ROOT -> WRAP -> ITEM(token)

    // Without no_ast_opt: entire chain collapses to ITEM
    let parser1 = Parser::new(r#"
        ROOT <- WRAP
        WRAP <- ITEM
        ITEM <- < [a-z]+ >
    "#).unwrap();
    let ast1 = parse_ast(&parser1, "hello");
    let opt1 = parser1.optimize_ast(ast1);
    assert_eq!(opt1.name, "ITEM");
    assert_eq!(opt1.original_name, "ROOT");
    assert!(opt1.is_token);
    assert!(opt1.nodes.is_empty());

    // With no_ast_opt on WRAP: WRAP won't collapse its child
    let mut parser2 = Parser::new(r#"
        ROOT <- WRAP
        WRAP <- ITEM
        ITEM <- < [a-z]+ >
    "#).unwrap();
    parser2.grammar_mut()["WRAP"].no_ast_opt = true;
    let ast2 = parse_ast(&parser2, "hello");
    let opt2 = parser2.optimize_ast(ast2);
    // ROOT collapses into WRAP, but WRAP keeps ITEM as child
    assert_eq!(opt2.name, "WRAP");
    assert_eq!(opt2.original_name, "ROOT");
    assert_eq!(opt2.nodes.len(), 1);
    assert_eq!(opt2.nodes[0].name, "ITEM");
    assert!(opt2.nodes[0].is_token);
}
