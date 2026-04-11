# PEG Spec Test Harness

Language-specific test runners for the language-independent test suite in `spec/tests/`.

```
harness/
  cpp/       C++ harness (reference, uses cpp-peglib)
  rust/      Rust harness (future)
```

## Build and run (C++)

```bash
cd harness/cpp && cmake -B build && cmake --build build
./build/spec-harness
```

All dependencies (`googletest`, `nlohmann/json`) are fetched automatically by CMake.

## Documents

- [DEBUGGING.md](./DEBUGGING.md) — diagnosing test failures, DSL quirks, layer triage
- [`spec/schema/test-case.schema.json`](../spec/schema/test-case.schema.json) — canonical definition of JSON test-case fields
- [`cpp/harness.cc`](./cpp/harness.cc) — reference interpreter for the JSON spec

## Adding a new language harness

Create `harness/<lang>/` and implement a runner that:

1. Loads each `spec/tests/**/*.json` file
2. For each group, creates a parser from `grammar` and configures it per the group's fields
3. Binds `actions`, `vars`, `handlers`, `trace` as defined in the schema
4. Runs each `case` and verifies against the `expected_*` fields

Field semantics: see the JSON schema. Reference implementation: `cpp/harness.cc`.
