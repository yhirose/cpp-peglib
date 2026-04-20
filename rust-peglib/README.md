# peglib

A Rust port of [cpp-peglib](https://github.com/yhirose/cpp-peglib) — a PEG (Parsing Expression Grammars) library.

## Features

- **1:1 port of cpp-peglib** — identical grammar syntax, same behavior
- PEG grammar loading from text
- Packrat parsing with memoization
- Left-recursion support (Warth-style seed growing)
- AST construction, optimization, and `no_ast_opt` per-rule control
- Per-rule action callbacks with `SemanticValues`
- First-set filtering and keyword guard optimization
- Error recovery (`%recover`, `^label`) with logger callback
- Macro rules, captures/backreferences, dictionary (`|`)
- Precedence climbing
- `%whitespace` and `%word` directives
- Trace events (enter/leave callbacks)
- End-of-input check control

## Usage

### Parsing

```rust
use peglib::Parser;

let parser = Parser::new(r#"
    Additive       <- Multiplicative '+' Additive / Multiplicative
    Multiplicative <- Primary '*' Multiplicative / Primary
    Primary        <- '(' Additive ')' / Number
    Number         <- < [0-9]+ >
    %whitespace    <- [ \t]*
"#)?;

assert!(parser.parse("(1 + 2) * 3"));
```

### With actions

```rust
use peglib::Parser;

let mut parser = Parser::new(r#"
    Additive       <- Multiplicative '+' Additive / Multiplicative
    Multiplicative <- Primary '*' Multiplicative / Primary
    Primary        <- '(' Additive ')' / Number
    Number         <- < [0-9]+ >
    %whitespace    <- [ \t]*
"#)?;

parser.set_action("Additive", |sv| match sv.choice() {
    0 => sv.get::<i32>(0) + sv.get::<i32>(1),
    _ => sv.get::<i32>(0),
});

parser.set_action("Multiplicative", |sv| match sv.choice() {
    0 => sv.get::<i32>(0) * sv.get::<i32>(1),
    _ => sv.get::<i32>(0),
});

parser.set_action("Number", |sv| sv.token_to_number::<i32>());

assert_eq!(parser.parse_into::<i32>("(1 + 2) * 3")?, 9);
```

### AST generation

```rust
use peglib::{Parser, ast_to_s, optimize_ast};

let parser = Parser::new(r#"
    Expr   <- Term ('+' Term)*
    Term   <- < [0-9]+ >
"#)?;

let ast = parser.parse_ast("1+2+3")?;
let ast = optimize_ast(ast);
println!("{}", ast_to_s(&ast));
```

### Error handling

All parse methods return `Result<_, ParseError>` with line/column/message:

```rust
use peglib::Parser;

let parser = Parser::new(grammar_text)?;   // ParseError on invalid grammar
let ast    = parser.parse_ast(input)?;     // ParseError on invalid input
let val    = parser.parse_into::<i32>(input)?;
```

`ParseError` implements `Display` and `Error` for use with `?` and logging.

### Semantic value helpers

Inside `set_action` closures:

```rust
sv.choice()                      // which branch matched (for `A / B / C`)
sv.get::<T>(i)                   // copy of the i-th child value (T: Copy)
sv.val::<T>(i)                   // reference to the i-th child value
sv.transform::<T>()              // Vec<T> of all children (T: Clone)
sv.token()                       // matched token slice
sv.token_to_number::<i32>()      // parse token as number (panics on failure)
sv.try_token_to_number::<i32>()  // Result version
sv.token_to_string()             // matched token as String
sv.line_info()                   // (line, col) of current match
```

### Advanced

```rust
// Compile with options
Parser::compile(grammar, Some("StartRule"), /*allow_left_recursion*/ true)?;

// Logger callback (called on parse failure)
parser.set_logger(|line, col, msg| eprintln!("{line}:{col}: {msg}"));

// Trace events
parser.enable_trace(Tracer { enter: Some(...), leave: Some(...) });

// Disable end-of-input check (allow trailing content)
parser.disable_eoi_check();

// Preserve a rule from AST optimization
parser.grammar_mut()["MyRule"].no_ast_opt = true;
```

## PEG syntax

The grammar syntax is identical to cpp-peglib:

```
# Rules
Rule       <- Expression
Rule(A, B) <- A B           # Macro rule

# Operators
Sequence   <- A B C
Choice     <- A / B / C
ZeroOrMore <- A*
OneOrMore  <- A+
Optional   <- A?
AndPred    <- &A
NotPred    <- !A
Repetition <- A{3} / A{2,4}

# Terminals
Literal    <- 'hello' / "world"
CaseIgnore <- 'hello'i
Class      <- [a-zA-Z_]
AnyChar    <- .
Token      <- < [0-9]+ >
Dictionary <- 'Jan' | 'Feb' | 'Mar'

# Directives
%whitespace <- [ \t\n]*
%word       <- [a-zA-Z0-9_]

# Instructions
Rule <- A (Op A)* { precedence L + - L * / }
Rule <- Expr      { error_message "expected expression" }
```

## Performance

Benchmarked on Apple M2 Max with 1.2 MB SQL input:

| | rust-peglib | cpp-peglib |
|---|---|---|
| big.sql | **66 ms** | 72 ms |
| TPC-H Q1 | **0.037 ms** | 0.041 ms |

## Example: PL/0 compiler

See [`pl0/`](./pl0/) for a complete PL/0 language implementation with:
- PEG grammar
- Tree-walking interpreter
- LLVM ORC JIT compiler (using [orc-jit](../orc-jit/))

Three modes: interpreter (default), JIT (`--jit`), LLVM IR dump (`--llvm`).

## License

MIT
