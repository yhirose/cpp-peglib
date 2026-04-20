# orc-jit

Minimal safe Rust wrapper around LLVM's ORC JIT, focused on embedding JIT compilation in custom compilers and DSLs.

## Why this crate?

- **inkwell** does not support ORC JIT (only the legacy MCJIT `ExecutionEngine`, which has [known exception-handling bugs](https://github.com/llvm/llvm-project/issues/49036))
- **llvm-sys** exposes the full LLVM C API but with raw FFI only (no safe wrappers)
- **orc-jit** provides a focused, safe layer over just what you need to build a working JIT compiler

Scope is deliberately narrow: IR building + ORC `LLJIT` execution. Roughly 70 wrapped functions, around 400 lines of code.

## Features

- Context, Module, Builder with RAII cleanup
- Type constructors (i1/i32/i64/void/pointer/function/struct)
- IR instructions: arithmetic, comparison, branch, call, invoke, alloca, load/store
- Exception handling: `landingpad`, `invoke`, `extractvalue`, personality functions
- Global constants, strings, external linkage
- Function verification
- ORC JIT: `LLJIT`, module loading, symbol lookup, dynamic library search

## Requirements

- LLVM 22 (installed via `brew install llvm` on macOS)
- Other versions may work; see the GitHub Actions matrix for the tested set

The build script discovers LLVM via `llvm-config` on `PATH`, or via `brew --prefix llvm` on macOS. Override with `LLVM_CONFIG=/path/to/llvm-config`.

## Quick example

```rust
use orc_jit::*;

let ctx = Context::new();
let module = Module::new("hello", &ctx);
let builder = Builder::new(&ctx);

// int add(int a, int b) { return a + b; }
let i32_ty = ctx.i32_type();
let fn_ty = ctx.function_type(i32_ty, &[i32_ty, i32_ty], false);
let func = module.add_function("add", fn_ty);

let bb = ctx.append_bb(func, "entry");
builder.position_at_end(bb);

let a = get_param(func, 0);
let b = get_param(func, 1);
let sum = builder.add(a, b, "sum");
// builder.ret(sum);  // see full example

// JIT compile & execute
let jit = LLJIT::new()?;
jit.add_module(ctx, module)?;
let addr = jit.lookup("add")?;
let add: extern "C" fn(i32, i32) -> i32 = unsafe { std::mem::transmute(addr) };
assert_eq!(add(1, 2), 3);
```

## Complete example: PL/0 compiler

See [pl0/](../pl0/) in the parent repository for a full implementation of the PL/0 language with ORC JIT. Demonstrates:

- Arithmetic and comparison
- Control flow (`if`/`while`)
- Procedure calls with closures (free variable passing)
- C++-style exception handling (divide-by-zero → throw → catch in `main`)
- Integration with a PEG parser (rust-peglib)

## Why ORC JIT over MCJIT?

- MCJIT is deprecated in modern LLVM
- MCJIT has [unresolved bugs](https://github.com/llvm/llvm-project/issues/49036) with C++ exception handling
- ORC JIT (`LLJIT`) is the recommended replacement and is actively developed

If your language has any form of exception/unwinding, you need ORC.

## License

MIT
