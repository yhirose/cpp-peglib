use peglib::Ast;
use std::collections::HashMap;
use std::io::{self, BufRead, Write};

use crate::symbol::token_str;

struct Env {
    constants: HashMap<String, i64>,
    variables: HashMap<String, i64>,
    procedures: HashMap<String, Ast<'static>>,
    outer: Option<Box<Env>>,
}

impl Env {
    fn new(outer: Option<Box<Env>>) -> Self {
        Self { constants: HashMap::new(), variables: HashMap::new(), procedures: HashMap::new(), outer }
    }

    fn get_value(&self, name: &str) -> Option<i64> {
        if let Some(&v) = self.constants.get(name) { return Some(v); }
        if let Some(&v) = self.variables.get(name) { return Some(v); }
        self.outer.as_ref()?.get_value(name)
    }

    fn set_variable(&mut self, name: &str, val: i64) {
        if self.variables.contains_key(name) {
            self.variables.insert(name.to_string(), val);
        } else if let Some(ref mut outer) = self.outer {
            outer.set_variable(name, val);
        }
    }

    fn get_procedure(&self, name: &str) -> Option<&Ast<'static>> {
        if let Some(p) = self.procedures.get(name) { return Some(p); }
        self.outer.as_ref()?.get_procedure(name)
    }
}

pub fn run(ast: &Ast<'static>) {
    let mut env = Env::new(None);
    exec(&ast.nodes[0], &mut env);
}

fn exec(ast: &Ast<'static>, env: &mut Env) {
    match ast.name.as_str() {
        "block" => {
            let mut inner = Env::new(Some(Box::new(std::mem::replace(env, Env::new(None)))));
            for node in &ast.nodes {
                exec_in(node, &mut inner);
            }
            *env = *inner.outer.unwrap();
        }
        "statement" => {
            if !ast.nodes.is_empty() {
                exec(&ast.nodes[0], env);
            }
        }
        "statements" => {
            for node in &ast.nodes {
                exec(node, env);
            }
        }
        "assignment" => {
            let name = token_str(&ast.nodes[0]);
            let val = eval_expression(&ast.nodes[1], env);
            env.set_variable(name, val);
        }
        "call" => {
            let name = token_str(&ast.nodes[0]);
            let proc_ast = env.get_procedure(name).unwrap().clone();
            exec(&proc_ast, env);
        }
        "if" => {
            if eval_condition(&ast.nodes[0], env) {
                exec(&ast.nodes[1], env);
            }
        }
        "while" => {
            while eval_condition(&ast.nodes[0], env) {
                exec(&ast.nodes[1], env);
            }
        }
        "out" => {
            let val = eval_expression(&ast.nodes[0], env);
            println!("{val}");
        }
        "in" => {
            let name = token_str(&ast.nodes[0]);
            print!("? ");
            io::stdout().flush().unwrap();
            let mut line = String::new();
            io::stdin().lock().read_line(&mut line).unwrap();
            let val: i64 = line.trim().parse().unwrap_or(0);
            env.set_variable(name, val);
        }
        _ => {
            for node in &ast.nodes {
                exec(node, env);
            }
        }
    }
}

fn exec_in(ast: &Ast<'static>, env: &mut Env) {
    match ast.name.as_str() {
        "const" => {
            let mut i = 0;
            while i + 1 < ast.nodes.len() {
                let name = token_str(&ast.nodes[i]).to_string();
                let val: i64 = token_str(&ast.nodes[i + 1]).parse().unwrap();
                env.constants.insert(name, val);
                i += 2;
            }
        }
        "var" => {
            for node in &ast.nodes {
                env.variables.insert(token_str(node).to_string(), 0);
            }
        }
        "procedure" => {
            let mut i = 0;
            while i + 1 < ast.nodes.len() {
                let name = token_str(&ast.nodes[i]).to_string();
                let block = ast.nodes[i + 1].clone();
                env.procedures.insert(name, block);
                i += 2;
            }
        }
        "statement" => exec(ast, env),
        _ => exec(ast, env),
    }
}

fn eval_expression(ast: &Ast, env: &Env) -> i64 {
    if ast.name == "expression" {
        let sign = token_str(&ast.nodes[0]);
        let mut val = eval_term(&ast.nodes[1], env);
        if sign == "-" { val = -val; }
        let mut i = 2;
        while i + 1 < ast.nodes.len() {
            let op = token_str(&ast.nodes[i]);
            let rhs = eval_term(&ast.nodes[i + 1], env);
            val = match op { "+" => val + rhs, "-" => val - rhs, _ => val };
            i += 2;
        }
        val
    } else {
        eval_term(ast, env)
    }
}

fn eval_term(ast: &Ast, env: &Env) -> i64 {
    if ast.name == "term" {
        let mut val = eval_factor(&ast.nodes[0], env);
        let mut i = 1;
        while i + 1 < ast.nodes.len() {
            let op = token_str(&ast.nodes[i]);
            let rhs = eval_factor(&ast.nodes[i + 1], env);
            val = match op { "*" => val * rhs, "/" => if rhs != 0 { val / rhs } else { 0 }, _ => val };
            i += 2;
        }
        val
    } else {
        eval_factor(ast, env)
    }
}

fn eval_factor(ast: &Ast, env: &Env) -> i64 {
    match ast.name.as_str() {
        "factor" => eval_factor(&ast.nodes[0], env),
        "ident" => env.get_value(token_str(ast)).unwrap_or(0),
        "number" => token_str(ast).parse().unwrap_or(0),
        "expression" => eval_expression(ast, env),
        _ => {
            if ast.is_token {
                ast.tokens.first().or(Some(&ast.sv)).unwrap().parse().unwrap_or(0)
            } else if !ast.nodes.is_empty() {
                eval_factor(&ast.nodes[0], env)
            } else {
                0
            }
        }
    }
}

fn eval_condition(ast: &Ast, env: &Env) -> bool {
    match ast.name.as_str() {
        "condition" => eval_condition(&ast.nodes[0], env),
        "odd" => eval_expression(&ast.nodes[0], env) % 2 != 0,
        "compare" => {
            let lhs = eval_expression(&ast.nodes[0], env);
            let op = token_str(&ast.nodes[1]);
            let rhs = eval_expression(&ast.nodes[2], env);
            match op {
                "=" => lhs == rhs, "#" => lhs != rhs,
                "<" => lhs < rhs, "<=" => lhs <= rhs,
                ">" => lhs > rhs, ">=" => lhs >= rhs,
                _ => false,
            }
        }
        _ => false,
    }
}
