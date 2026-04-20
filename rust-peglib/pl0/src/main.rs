mod parser;
mod symbol;
mod interp;
mod jit;

use peglib::{Ast, ast_to_s, optimize_ast};
use symbol::{Scope, build_symbols};

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: pl0 PATH [--ast] [--llvm] [--jit]");
        std::process::exit(1);
    }

    let path = &args[1];
    let show_ast = args.iter().any(|a| a == "--ast");
    let opt_jit = args.iter().any(|a| a == "--jit");
    let opt_llvm = args.iter().any(|a| a == "--llvm");

    let source = std::fs::read_to_string(path).unwrap_or_else(|e| {
        eprintln!("error: cannot read '{path}': {e}");
        std::process::exit(1);
    });

    let pl0_parser = parser::new();
    let ast = pl0_parser.parse_ast(&source).unwrap_or_else(|e| {
        eprintln!("parse error: {e}");
        std::process::exit(1);
    });

    if show_ast {
        print!("{}", ast_to_s(&optimize_ast(ast)));
        return;
    }

    let mut scope = Scope::new(None);
    if let Err(e) = build_symbols(&ast.nodes[0], &mut scope) {
        eprintln!("semantic error: {e}");
        std::process::exit(1);
    }

    let ast_static: Ast<'static> = unsafe { std::mem::transmute(ast) };
    if opt_llvm || opt_jit {
        jit::run(&ast_static, opt_llvm);
    } else {
        interp::run(&ast_static);
    }
}
