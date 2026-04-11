# PEG Parser Test Specification

A language-independent test suite for PEG parser implementations that support the cpp-peglib extended PEG grammar syntax.

## Purpose

This specification defines a common set of test cases in JSON format that any conforming PEG parser implementation can use to verify compatibility. The same `.peg` grammar files and test inputs should produce identical results across C++, Rust, Go, or any other language implementation.

## Directory Structure

```
spec/
  schema/
    test-case.schema.json       # JSON Schema for test case files
  tests/
    core/                       # Standard PEG operators
      sequence.json
      choice.json
      repetition.json
      literal.json
      character_class.json
      lookahead.json
      token_boundary.json
      ignore.json
      dot.json
    extensions/                 # cpp-peglib extended syntax
      whitespace.json
      word.json
      cut.json
      dictionary.json
      macro.json
      precedence.json
      capture_backreference.json
      left_recursion.json
      case_insensitive.json
      hex_unicode.json
    error/                      # Error reporting and recovery
      error_report.json
      error_recovery.json
      error_message.json
      infinite_loop.json
    ast/                        # AST generation and optimization
      ast_generation.json
      ast_optimization.json
    semantic/                   # Semantic actions
      token_conversion.json
      choice_detection.json
      arithmetic.json
      enter_leave.json
      predicate.json
      state_variables.json
    packrat/                    # Packrat parsing
      packrat.json
    unicode/                    # Unicode support
      unicode.json
```

## Test Case Format

Each JSON file contains an array of test cases. A minimal test case specifies a grammar, input, and expected result:

```json
[
  {
    "name": "basic_sequence",
    "grammar": "S <- 'hello' ' ' 'world'",
    "cases": [
      { "input": "hello world", "match": true },
      { "input": "hello", "match": false }
    ]
  }
]
```

### Grammar

The `grammar` field contains PEG grammar text, exactly as it would appear in a `.peg` file. Multi-line grammars use `\n` for line breaks.

### Cases

Each entry in `cases` specifies:

| Field | Type | Description |
|---|---|---|
| `input` | string | Input text to parse |
| `match` | boolean | Whether parsing should succeed |
| `match_length` | integer | (optional) Expected number of characters consumed |
| `expected_value` | any | (optional) Expected semantic value from the start rule |
| `expected_ast` | string | (optional) Expected AST text dump |
| `expected_trace` | string[] | (optional) Expected enter/leave event sequence |
| `expected_error` | object | (optional) Expected error `{ "line": N, "col": N, "message": "..." }` |

### Semantic Actions

The `actions` field defines semantic actions declaratively:

```json
{
  "grammar": "Expr <- Term (('+' / '-') Term)*\nTerm <- < [0-9]+ >",
  "actions": {
    "Term": { "op": "token_to_number", "type": "int" },
    "Expr": {
      "op": "reduce",
      "initial": { "child": 0 },
      "step": {
        "operator_child": 1,
        "value_child": 2,
        "operators": { "+": "add", "-": "subtract" }
      }
    }
  },
  "cases": [
    { "input": "1+2-3", "expected_value": 0 }
  ]
}
```

#### Available Action Operations

| Operation | Description |
|---|---|
| `token_to_number` | Convert matched token to a number |
| `token_to_string` | Return matched token as a string |
| `choice` | Return the matched choice index (0-based) |
| `size` | Return the number of child semantic values |
| `join_tokens` | Join token values with a separator |
| `reduce` | Fold child values with binary operators |
| `passthrough` | Return the Nth child value (default: 0) |

### State Variables

The `vars` field declares mutable state accessible from handlers:

```json
{
  "vars": {
    "indent": { "type": "int", "init": 0 },
    "require_upper": { "type": "bool", "init": false },
    "banned": { "type": "string[]", "init": ["bad", "evil"] }
  }
}
```

Supported variable types: `bool`, `int`, `string[]`.

### Handlers (enter/leave/predicate)

The `handlers` field defines enter, leave, and predicate callbacks:

```json
{
  "handlers": {
    "Block": {
      "enter": { "set": { "indent": "indent + 2" } },
      "leave": { "set": { "indent": "indent - 2" } }
    },
    "Samedent": {
      "predicate": {
        "check": { "sv.length": { "eq": "indent" } },
        "error": "different indent..."
      }
    }
  }
}
```

#### Enter/Leave Actions

| Field | Description |
|---|---|
| `set` | Object mapping variable names to new values. Values can be constants or simple expressions like `"var + N"` or `"var - N"` |

#### Predicate

| Field | Description |
|---|---|
| `when` | (optional) Variable name; predicate only applies when the variable is truthy |
| `check` | Condition to evaluate (see below) |
| `error` | Error message on failure |

#### Check Conditions

The left side of a check is a property of the parse result:

| Property | Description |
|---|---|
| `sv` | Matched string |
| `sv.length` | Length of matched string |
| `token_string` | Token text |
| `token_number` | Token converted to number |

The right side is a comparison:

| Comparison | Description |
|---|---|
| `eq` | Equal to a value or variable |
| `matches` | Regex match |
| `between` | `[min, max]` inclusive range |
| `in` | Contained in a string[] variable |
| `not_in` | Not contained in a string[] variable |

### Trace Verification

To verify enter/leave ordering:

```json
{
  "trace": {
    "rules": ["A", "B"],
    "events": ["enter", "leave"]
  },
  "cases": [
    {
      "input": "ab",
      "match": true,
      "expected_trace": ["enter_A", "leave_A", "enter_B", "leave_B"]
    }
  ]
}
```

The test harness hooks enter/leave on the specified rules and records events as `"enter_RULE"` / `"leave_RULE"` strings.

### AST Verification

To verify AST output:

```json
{
  "ast": { "optimize": true },
  "cases": [
    {
      "input": "1+2*3",
      "expected_ast": "+ Expr/0\n  - Term[Number] (1)\n  + Expr/0[Term]\n    - Number (2)\n    - Term[Number] (3)"
    }
  ]
}
```

The AST text format uses:
- `+ RuleName` — non-terminal node (has children)
- `- RuleName` — terminal node (token)
- `/N` — choice index (0-based)
- `[OriginalRule]` — original rule name before AST optimization
- `(token)` — token content
- Indentation: 2 spaces per depth level

## Implementing a Test Harness

Each language implementation provides a test harness that:

1. Reads JSON test files
2. Creates a parser from the `grammar` field
3. Binds `actions` to native semantic action functions
4. Initializes `vars` as mutable state
5. Binds `handlers` (enter/leave/predicate) to native callbacks
6. Sets up `trace` hooks if specified
7. Enables AST mode if `ast` is specified
8. Runs each case and verifies against expected results

The harness is typically a few hundred lines of code. Once written, adding new test cases requires no harness changes.

## Scope

This specification covers the **PEG grammar format and its parsing behavior**. It does not cover:

- Language-specific parser combinator APIs (e.g., C++ `Definition`, `seq`, `cho`)
- Language-specific binding details beyond what the action DSL expresses
- Performance characteristics

These aspects are tested by each implementation's own native test suite.
