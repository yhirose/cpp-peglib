use orc_jit::*;
use orc_jit::ffi::{LLVMTypeRef, LLVMValueRef};
use peglib::Ast;
use std::collections::HashMap;

use crate::symbol::token_str;

struct Compiler<'a> {
    ctx: &'a Context,
    module: &'a Module,
    builder: &'a Builder,
    tyinfo: LLVMValueRef,
    symbols: Vec<HashMap<String, LLVMValueRef>>,
    scopes: Vec<ScopeInfo>,
}

struct ScopeInfo {
    procedures: HashMap<String, ProcInfo>,
}

struct ProcInfo {
    free_vars: Vec<String>,
}

impl<'a> Compiler<'a> {
    fn push_scope(&mut self) {
        self.symbols.push(HashMap::new());
        self.scopes.push(ScopeInfo { procedures: HashMap::new() });
    }

    fn pop_scope(&mut self) {
        self.symbols.pop();
        self.scopes.pop();
    }

    fn set_symbol(&mut self, name: &str, val: LLVMValueRef) {
        self.symbols.last_mut().unwrap().insert(name.to_string(), val);
    }

    fn lookup_symbol(&self, name: &str) -> Option<LLVMValueRef> {
        for scope in self.symbols.iter().rev() {
            if let Some(&v) = scope.get(name) {
                return Some(v);
            }
        }
        None
    }

    fn lookup_proc(&self, name: &str) -> Option<&ProcInfo> {
        for scope in self.scopes.iter().rev() {
            if let Some(p) = scope.procedures.get(name) {
                return Some(p);
            }
        }
        None
    }

    fn i32_ty(&self) -> LLVMTypeRef { self.ctx.i32_type() }
    fn i64_ty(&self) -> LLVMTypeRef { self.ctx.i64_type() }
    fn void_ty(&self) -> LLVMTypeRef { self.ctx.void_type() }
    fn ptr_ty(&self) -> LLVMTypeRef { self.ctx.ptr_type() }
}

pub fn run(ast: &Ast<'static>, dump_llvm: bool) {
    let ctx = Context::new();
    let module = Module::new("pl0", &ctx);
    let builder = Builder::new(&ctx);

    let tyinfo = module.add_global(ctx.ptr_type(), "_ZTIPKc");
    set_linkage(tyinfo, ffi::LLVMLinkage::LLVMExternalLinkage);
    set_global_constant(tyinfo, true);

    let mut compiler = Compiler {
        ctx: &ctx, module: &module, builder: &builder,
        tyinfo, symbols: Vec::new(), scopes: Vec::new(),
    };

    compile_libs(&compiler);
    compile_program(&mut compiler, ast);

    if dump_llvm {
        print!("{}", module.print_to_string());
    } else {
        exec(ctx, module);
    }
}

fn exec(ctx: Context, module: Module) {
    let jit = LLJIT::new().expect("failed to create JIT");
    jit.add_module(ctx, module).expect("failed to add module");
    let addr = jit.lookup("main").expect("failed to find 'main'");
    let main_fn: extern "C" fn() = unsafe { std::mem::transmute(addr) };
    main_fn();
}

fn compile_libs(c: &Compiler) {
    // out(i32) -> void: prints value with printf
    let out_ty = c.ctx.function_type(c.void_ty(), &[c.i32_ty()], false);
    let out_fn = c.module.add_function("out", out_ty);

    let bb = c.ctx.append_bb(out_fn, "entry");
    c.builder.position_at_end(bb);

    let printf_ty = c.ctx.function_type(c.ctx.i32_type(), &[c.ptr_ty()], true);
    let printf_fn = c.module.get_or_insert_function("printf", printf_ty);

    let val = get_param(out_fn, 0);
    let fmt = c.builder.global_string_ptr("%d\n", ".printf.fmt");
    c.builder.call(printf_ty, printf_fn, &[fmt, val], "");

    c.builder.ret_void();
    verify_function(out_fn);
}

fn compile_program(c: &mut Compiler, ast: &Ast) {
    let block_ast = &ast.nodes[0];

    // __pl0_start function
    let start_ty = c.ctx.function_type(c.void_ty(), &[], false);
    let start_fn = c.module.add_function("__pl0_start", start_ty);
    {
        let bb = c.ctx.append_bb(start_fn, "entry");
        c.builder.position_at_end(bb);
        compile_block(c, block_ast);
        c.builder.ret_void();
        verify_function(start_fn);
    }

    // main function with exception handling
    let main_ty = c.ctx.function_type(c.void_ty(), &[], false);
    let main_fn = c.module.add_function("main", main_ty);
    {
        let personality_ty = c.ctx.function_type(c.ctx.i32_type(), &[], true);
        let personality_fn = c.module.add_function("__gxx_personality_v0", personality_ty);
        set_personality_fn(main_fn, personality_fn);

        let entry_bb = c.ctx.append_bb(main_fn, "entry");
        c.builder.position_at_end(entry_bb);

        let lpad_bb = c.ctx.append_bb(main_fn, "lpad");
        let end_bb = c.ctx.create_bb("end");

        c.builder.invoke(start_ty, start_fn, &[], end_bb, lpad_bb, "");

        // Landing pad
        c.builder.position_at_end(lpad_bb);

        let exc_ty = c.ctx.struct_type(&[c.ptr_ty(), c.ctx.i32_type()]);
        let exc = c.builder.landing_pad(exc_ty, 1, "exc");
        add_clause(exc, c.tyinfo);

        let ptr = c.builder.extract_value(exc, 0, "exc.ptr");
        let sel = c.builder.extract_value(exc, 1, "exc.sel");

        let typeid_ty = c.ctx.function_type(c.ctx.i32_type(), &[c.ptr_ty()], false);
        let typeid_fn = c.module.get_or_insert_function("llvm.eh.typeid.for.p0", typeid_ty);
        let id = c.builder.call(typeid_ty, typeid_fn, &[c.tyinfo], "tid.int");

        let catch_msg_bb = c.ctx.append_bb(main_fn, "catch_with_message");
        let catch_unk_bb = c.ctx.append_bb(main_fn, "catch_unknown");
        let cmp = c.builder.icmp(LLVMIntPredicate::LLVMIntEQ, sel, id, "tst.int");
        c.builder.cond_br(cmp, catch_msg_bb, catch_unk_bb);

        let begin_catch_ty = c.ctx.function_type(c.ptr_ty(), &[c.ptr_ty()], false);
        let begin_catch_fn = c.module.get_or_insert_function("__cxa_begin_catch", begin_catch_ty);
        let end_catch_ty = c.ctx.function_type(c.void_ty(), &[], false);
        let end_catch_fn = c.module.get_or_insert_function("__cxa_end_catch", end_catch_ty);
        let puts_ty = c.ctx.function_type(c.ctx.i32_type(), &[c.ptr_ty()], false);
        let puts_fn = c.module.get_or_insert_function("puts", puts_ty);

        // catch_with_message
        c.builder.position_at_end(catch_msg_bb);
        let str_val = c.builder.call(begin_catch_ty, begin_catch_fn, &[ptr], "str");
        c.builder.call(puts_ty, puts_fn, &[str_val], "");
        c.builder.call(end_catch_ty, end_catch_fn, &[], "");
        c.builder.br(end_bb);

        // catch_unknown
        c.builder.position_at_end(catch_unk_bb);
        c.builder.call(begin_catch_ty, begin_catch_fn, &[ptr], "");
        let unk_str = c.builder.global_string_ptr("unknown error...", ".str.unknown");
        c.builder.call(puts_ty, puts_fn, &[unk_str], "");
        c.builder.call(end_catch_ty, end_catch_fn, &[], "");
        c.builder.br(end_bb);

        // end
        unsafe { ffi::LLVMAppendExistingBasicBlock(main_fn, end_bb); }
        c.builder.position_at_end(end_bb);
        c.builder.ret_void();

        verify_function(main_fn);
    }
}

fn compile_block(c: &mut Compiler, ast: &Ast) {
    let block = if ast.name == "block" { ast } else { &ast.nodes[0] };
    c.push_scope();
    compile_const(c, &block.nodes[0]);
    compile_var(c, &block.nodes[1]);
    compile_procedure(c, &block.nodes[2]);
    compile_statement(c, &block.nodes[3]);
    c.pop_scope();
}

fn compile_const(c: &mut Compiler, ast: &Ast) {
    let mut i = 0;
    while i + 1 < ast.nodes.len() {
        let name = token_str(&ast.nodes[i]);
        let val: i64 = token_str(&ast.nodes[i + 1]).parse().unwrap_or(0);
        let alloca = c.builder.alloca(c.i32_ty(), name);
        c.builder.store(c.builder.const_i32(c.ctx, val), alloca);
        c.set_symbol(name, alloca);
        i += 2;
    }
}

fn compile_var(c: &mut Compiler, ast: &Ast) {
    for node in &ast.nodes {
        let name = token_str(node);
        let alloca = c.builder.alloca(c.i32_ty(), name);
        c.set_symbol(name, alloca);
    }
}

fn compile_procedure(c: &mut Compiler, ast: &Ast) {
    let mut i = 0;
    while i + 1 < ast.nodes.len() {
        let name = token_str(&ast.nodes[i]);
        let block = &ast.nodes[i + 1];

        // Collect free variables by scanning the block
        let mut free_vars = Vec::new();
        collect_free_vars(block, &mut free_vars, c);

        // Create function with pointer params for free vars
        let param_types: Vec<LLVMTypeRef> = free_vars.iter().map(|_| c.ptr_ty()).collect();
        let fn_ty = c.ctx.function_type(c.void_ty(), &param_types, false);
        let func = c.module.add_function(name, fn_ty);

        for (j, fv) in free_vars.iter().enumerate() {
            let param = get_param(func, j as u32);
            set_value_name(param, fv);
        }

        // Register procedure info
        c.scopes.last_mut().unwrap().procedures.insert(
            name.to_string(),
            ProcInfo { free_vars: free_vars.clone() },
        );

        // Compile function body
        let prev_bb = c.builder.get_insert_block();
        let bb = c.ctx.append_bb(func, "entry");
        c.builder.position_at_end(bb);

        // Push scope with free var params as symbols
        c.push_scope();
        for (j, fv) in free_vars.iter().enumerate() {
            c.set_symbol(fv, get_param(func, j as u32));
        }

        compile_const(c, &block.nodes[0]);
        compile_var(c, &block.nodes[1]);
        compile_procedure(c, &block.nodes[2]);
        compile_statement(c, &block.nodes[3]);

        c.pop_scope();

        c.builder.ret_void();
        verify_function(func);
        c.builder.position_at_end(prev_bb);

        i += 2;
    }
}

fn collect_free_vars(ast: &Ast, free_vars: &mut Vec<String>, c: &Compiler) {
    if ast.name == "ident" {
        let name = token_str(ast);
        if c.lookup_symbol(name).is_some() && !free_vars.contains(&name.to_string()) {
            free_vars.push(name.to_string());
        }
    }
    // Don't recurse into nested procedure blocks
    if ast.name == "procedure" { return; }
    for node in &ast.nodes {
        collect_free_vars(node, free_vars, c);
    }
}

fn compile_statement(c: &mut Compiler, ast: &Ast) {
    if !ast.nodes.is_empty() {
        compile_switch(c, &ast.nodes[0]);
    }
}

fn compile_switch(c: &mut Compiler, ast: &Ast) {
    match ast.name.as_str() {
        "assignment" => compile_assignment(c, ast),
        "call" => compile_call(c, ast),
        "statements" => compile_statements(c, ast),
        "if" => compile_if(c, ast),
        "while" => compile_while(c, ast),
        "out" => compile_out(c, ast),
        _ => {
            if !ast.nodes.is_empty() {
                compile_switch(c, &ast.nodes[0]);
            }
        }
    }
}

fn compile_assignment(c: &mut Compiler, ast: &Ast) {
    let name = token_str(&ast.nodes[0]);
    let var = c.lookup_symbol(name).unwrap_or_else(|| panic!("'{name}' is not defined"));
    let val = compile_expression(c, &ast.nodes[1]);
    c.builder.store(val, var);
}

fn compile_call(c: &mut Compiler, ast: &Ast) {
    let name = token_str(&ast.nodes[0]);
    let proc_info = c.lookup_proc(name).expect("undefined procedure");
    let free_vars = proc_info.free_vars.clone();

    let mut args = Vec::new();
    for fv in &free_vars {
        let var = c.lookup_symbol(fv).unwrap_or_else(|| panic!("'{fv}' is not defined"));
        args.push(var);
    }

    let func = c.module.get_function(name).expect("function not found");
    let param_types: Vec<LLVMTypeRef> = free_vars.iter().map(|_| c.ptr_ty()).collect();
    let fn_ty = c.ctx.function_type(c.void_ty(), &param_types, false);
    c.builder.call(fn_ty, func, &args, "");
}

fn compile_statements(c: &mut Compiler, ast: &Ast) {
    for node in &ast.nodes {
        compile_statement(c, node);
    }
}

fn compile_if(c: &mut Compiler, ast: &Ast) {
    let cond = compile_condition(c, &ast.nodes[0]);
    let func = c.builder.current_function();
    let then_bb = c.ctx.append_bb(func, "if.then");
    let end_bb = c.ctx.create_bb("if.end");

    c.builder.cond_br(cond, then_bb, end_bb);

    c.builder.position_at_end(then_bb);
    compile_statement(c, &ast.nodes[1]);
    c.builder.br(end_bb);

    unsafe { ffi::LLVMAppendExistingBasicBlock(func, end_bb); }
    c.builder.position_at_end(end_bb);
}

fn compile_while(c: &mut Compiler, ast: &Ast) {
    let func = c.builder.current_function();
    let cond_bb = c.ctx.create_bb("while.cond");

    c.builder.br(cond_bb);

    unsafe { ffi::LLVMAppendExistingBasicBlock(func, cond_bb); }
    c.builder.position_at_end(cond_bb);

    let cond = compile_condition(c, &ast.nodes[0]);

    let body_bb = c.ctx.append_bb(func, "while.body");
    let end_bb = c.ctx.create_bb("while.end");

    c.builder.cond_br(cond, body_bb, end_bb);

    c.builder.position_at_end(body_bb);
    compile_statement(c, &ast.nodes[1]);
    c.builder.br(cond_bb);

    unsafe { ffi::LLVMAppendExistingBasicBlock(func, end_bb); }
    c.builder.position_at_end(end_bb);
}

fn compile_out(c: &mut Compiler, ast: &Ast) {
    let val = compile_expression(c, &ast.nodes[0]);
    let out_fn = c.module.get_function("out").unwrap();
    let out_ty = c.ctx.function_type(c.void_ty(), &[c.i32_ty()], false);
    c.builder.call(out_ty, out_fn, &[val], "");
}

fn compile_condition(c: &mut Compiler, ast: &Ast) -> LLVMValueRef {
    compile_switch_value(c, &ast.nodes[0])
}

fn compile_switch_value(c: &mut Compiler, ast: &Ast) -> LLVMValueRef {
    match ast.name.as_str() {
        "odd" => compile_odd(c, ast),
        "compare" => compile_compare(c, ast),
        "expression" => compile_expression(c, ast),
        "ident" => compile_ident(c, ast),
        "number" => compile_number(c, ast),
        _ => {
            if !ast.nodes.is_empty() {
                compile_switch_value(c, &ast.nodes[0])
            } else {
                c.builder.const_i32(c.ctx, 0)
            }
        }
    }
}

fn compile_odd(c: &mut Compiler, ast: &Ast) -> LLVMValueRef {
    let val = compile_expression(c, &ast.nodes[0]);
    c.builder.icmp(LLVMIntPredicate::LLVMIntNE, val, c.builder.const_i32(c.ctx, 0), "icmpne")
}

fn compile_compare(c: &mut Compiler, ast: &Ast) -> LLVMValueRef {
    let lhs = compile_expression(c, &ast.nodes[0]);
    let rhs = compile_expression(c, &ast.nodes[2]);
    let op = token_str(&ast.nodes[1]);
    let pred = match op {
        "=" => LLVMIntPredicate::LLVMIntEQ,
        "#" => LLVMIntPredicate::LLVMIntNE,
        "<" => LLVMIntPredicate::LLVMIntSLT,
        "<=" => LLVMIntPredicate::LLVMIntSLE,
        ">" => LLVMIntPredicate::LLVMIntSGT,
        ">=" => LLVMIntPredicate::LLVMIntSGE,
        _ => LLVMIntPredicate::LLVMIntEQ,
    };
    c.builder.icmp(pred, lhs, rhs, "cmp")
}

fn compile_expression(c: &mut Compiler, ast: &Ast) -> LLVMValueRef {
    let nodes = &ast.nodes;
    let sign = token_str(&nodes[0]);
    let negative = !(sign.is_empty() || sign == "+");

    let mut val = compile_term(c, &nodes[1]);
    if negative { val = c.builder.neg(val, "negative"); }

    let mut i = 2;
    while i + 1 < nodes.len() {
        let op = token_str(&nodes[i]);
        let rhs = compile_term(c, &nodes[i + 1]);
        val = match op {
            "+" => c.builder.add(val, rhs, "add"),
            "-" => c.builder.sub(val, rhs, "sub"),
            _ => val,
        };
        i += 2;
    }
    val
}

fn compile_term(c: &mut Compiler, ast: &Ast) -> LLVMValueRef {
    let nodes = &ast.nodes;
    let mut val = compile_switch_value(c, &nodes[0]);

    let mut i = 1;
    while i + 1 < nodes.len() {
        let op = token_str(&nodes[i]);
        let rhs = compile_switch_value(c, &nodes[i + 1]);
        val = match op {
            "*" => c.builder.mul(val, rhs, "mul"),
            "/" => compile_div(c, val, rhs),
            _ => val,
        };
        i += 2;
    }
    val
}

fn compile_div(c: &mut Compiler, lhs: LLVMValueRef, rhs: LLVMValueRef) -> LLVMValueRef {
    let cond = c.builder.icmp(LLVMIntPredicate::LLVMIntEQ, rhs, c.builder.const_i32(c.ctx, 0), "icmpeq");

    let func = c.builder.current_function();
    let if_zero_bb = c.ctx.append_bb(func, "zdiv.zero");
    let if_nonzero_bb = c.ctx.create_bb("zdiv.non_zero");
    c.builder.cond_br(cond, if_zero_bb, if_nonzero_bb);

    // Zero: throw exception
    c.builder.position_at_end(if_zero_bb);
    {
        let alloc_ty = c.ctx.function_type(c.ptr_ty(), &[c.i64_ty()], false);
        let alloc_fn = c.module.get_or_insert_function("__cxa_allocate_exception", alloc_ty);
        let eh = c.builder.call(alloc_ty, alloc_fn, &[c.builder.const_i64(c.ctx, 8)], "eh");

        let msg = c.builder.global_string_ptr("divide by 0", ".str.zero_divide");
        c.builder.store(msg, eh);

        let throw_ty = c.ctx.function_type(c.void_ty(), &[c.ptr_ty(), c.ptr_ty(), c.ptr_ty()], false);
        let throw_fn = c.module.get_or_insert_function("__cxa_throw", throw_ty);
        c.builder.call(throw_ty, throw_fn, &[eh, c.tyinfo, const_null_ptr(c.ctx)], "");
        c.builder.unreachable();
    }

    // Non-zero: divide
    unsafe { ffi::LLVMAppendExistingBasicBlock(func, if_nonzero_bb); }
    c.builder.position_at_end(if_nonzero_bb);
    c.builder.sdiv(lhs, rhs, "div")
}

fn compile_ident(c: &Compiler, ast: &Ast) -> LLVMValueRef {
    let name = token_str(ast);
    let var = c.lookup_symbol(name).unwrap_or_else(|| panic!("'{name}' is not defined"));
    c.builder.load(c.i32_ty(), var, name)
}

fn compile_number(c: &Compiler, ast: &Ast) -> LLVMValueRef {
    let val: i64 = token_str(ast).parse().unwrap_or(0);
    c.builder.const_i32(c.ctx, val)
}
