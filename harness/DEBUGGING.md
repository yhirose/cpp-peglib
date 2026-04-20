# Harness Debugging — AI Instructions

Concise facts for diagnosing failures in `harness/cpp/spec-harness`. This file is written for AI assistants, not humans; it omits narrative and tutorials.

## Three layers

A failing test points to exactly one of:

- `spec/tests/**/*.json` — test data
- `harness/cpp/harness.cc` — JSON interpreter (schema: `spec/schema/test-case.schema.json`)
- `peglib.h` — PEG parser (reference implementation)

## Failure output format

The harness prints failures in this form (parse it to extract `group_name`):

```
<assertion> mismatch
  File:    <json_path>:<group_line>
  Group:   <group_name>
  Case:    [<index>] input="<input>"
  Reproduce: PEG_SPEC_FOCUS=<group_name> ./spec-harness
```

`expected_ast` failures additionally print `Expected AST:` and `Actual AST:` blocks, indented 4 spaces.

## Isolation commands

| Goal | Command |
|---|---|
| Run one group across all files | `PEG_SPEC_FOCUS=<name> ./spec-harness` |
| Narrow to one file | add `--gtest_filter='*<file_stem>*'` |
| Trace execution | `PEG_SPEC_VERBOSE=1 ./spec-harness` |

## Symptom → root cause

| Symptom | First suspect | Fix location |
|---|---|---|
| `bad any cast` in `reduce` | missing `operator_rule` field | JSON actions |
| `bad any cast` in `choice_op` | wrong `left`/`right` index | JSON actions |
| `bad any cast` in `passthrough` | index points at a literal slot | JSON actions |
| Infinite hang during parse | left-recursive grammar with no base case | change JSON to `grammar_compiles: true` and drop `cases` |
| AST dump mismatch | `ast_to_s` format changed or JSON stale | JSON `expected_ast` (regenerate with `peglint --ast --opt`) |
| Error message text mismatch | `peglib.h` wording drift | JSON (keep `line`/`col`, drop `message`) or `peglib.h` |
| Same scenario fails in gtest | parser regression | `peglib.h` |
| gtest passes, harness fails | spec or harness bug | JSON or `harness.cc` |
| `grammar_error: true` no longer triggers | parser became permissive | JSON or `peglib.h` |

## DSL quirks (non-obvious, from harness.cc)

- `reduce` requires `"operator_rule": "<RULE>"`; harness auto-binds that rule to return `std::string`. Without it, `vs[i]` for the operator child is empty `std::any`.
- `choice_op` cases: `"0"`, `"1"`, ..., `"default"`. `left`/`right` are indices into `vs` as seen by the parent rule. Literal characters in the grammar (`'+' '-'`) do **not** occupy `vs` slots, so for `expr <- expr '+' term`, indices are `0` (expr) and `1` (term).
- `passthrough` returns `vs[index]` as `std::any`; it cannot synthesize a value from a literal slot.
- `expected_value` is compared as `long`.
- `expected_ast` is the verbatim output of `peg::ast_to_s(ast)`, trailing newline included. Any whitespace difference fails the match.
- `grammar_compiles: true` disables `cases`; use for left-recursive grammars that would hang if parsed.
- `peg_meta_test: true` switches to `peg::ParserGenerator::parse_test`; uses `rule` or `cases_by_rule`, not `grammar`.

## Action DSL vocabulary

Implemented in `harness/cpp/harness.cc` `bind_action()`:

`token_to_number`, `token_to_string`, `choice`, `size`, `join_tokens`, `passthrough`, `reduce`, `choice_op`

Handler DSL (`bind_handlers()`): `enter.set`, `leave.set`, `predicate.when`/`check`/`error`. Check properties: `sv`, `sv.length`, `token_string`, `token_number`. Comparisons: `eq`, `matches`, `between`, `in`, `not_in`. Variable types: `bool`, `int`, `string[]`.

## Editing map

| Change | Files to edit |
|---|---|
| Fix expected value only | `spec/tests/**/*.json` |
| Add a new JSON field | `spec/schema/test-case.schema.json` + `harness/cpp/harness.cc` + JSON |
| Add a new DSL action | `harness/cpp/harness.cc` `bind_action()` + schema `Action` oneOf |
| Improve failure output | `harness/cpp/harness.cc` `format_context()` |
| Fix parser behavior | `peglib.h` + add regression test in `test/*.cc` |

## Out of scope for spec tests

These belong in `test/*.cc` (gtest), **not** in `spec/tests/`:

- C++ parser-combinator API (`seq`, `cho`, `Definition`)
- `peglib.h` internal state checks (`parent.expired()`, `AstOptimizer` node internals)
- Arbitrary C++ logic in semantic actions
- Patterns that require extending the DSL only for one test
