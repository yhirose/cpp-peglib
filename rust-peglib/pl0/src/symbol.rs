use peglib::Ast;
use std::collections::{HashMap, HashSet};

pub fn token_str<'a>(ast: &'a Ast) -> &'a str {
    if !ast.tokens.is_empty() { ast.tokens[0] } else { ast.sv }
}

#[derive(Clone)]
pub struct Scope {
    constants: HashMap<String, i64>,
    variables: HashSet<String>,
    procedures: HashSet<String>,
    outer: Option<Box<Scope>>,
}

impl Scope {
    pub fn new(outer: Option<Box<Scope>>) -> Self {
        Self { constants: HashMap::new(), variables: HashSet::new(), procedures: HashSet::new(), outer }
    }

    pub fn lookup_constant(&self, name: &str) -> Option<i64> {
        self.constants.get(name).copied().or_else(|| self.outer.as_ref()?.lookup_constant(name))
    }

    pub fn has_variable(&self, name: &str) -> bool {
        self.variables.contains(name) || self.outer.as_ref().map_or(false, |o| o.has_variable(name))
    }

    pub fn has_procedure(&self, name: &str) -> bool {
        self.procedures.contains(name) || self.outer.as_ref().map_or(false, |o| o.has_procedure(name))
    }

    fn is_local(&self, name: &str) -> bool {
        self.constants.contains_key(name) || self.variables.contains(name) || self.procedures.contains(name)
    }
}

pub fn build_symbols(ast: &Ast, scope: &mut Scope) -> Result<(), String> {
    match ast.name.as_str() {
        "block" => {
            for node in &ast.nodes {
                build_symbols(node, scope)?;
            }
        }
        "const" => {
            let mut i = 0;
            while i + 1 < ast.nodes.len() {
                let name = token_str(&ast.nodes[i]);
                let val: i64 = token_str(&ast.nodes[i + 1]).parse().map_err(|e| format!("{e}"))?;
                if scope.is_local(name) {
                    return Err(format!("'{name}' is already defined"));
                }
                scope.constants.insert(name.to_string(), val);
                i += 2;
            }
        }
        "var" => {
            for node in &ast.nodes {
                let name = token_str(node);
                if scope.is_local(name) {
                    return Err(format!("'{name}' is already defined"));
                }
                scope.variables.insert(name.to_string());
            }
        }
        "procedure" => {
            let mut i = 0;
            while i + 1 < ast.nodes.len() {
                let name = token_str(&ast.nodes[i]);
                if scope.is_local(name) {
                    return Err(format!("'{name}' is already defined"));
                }
                scope.procedures.insert(name.to_string());
                let mut inner = Scope::new(Some(Box::new(scope.clone())));
                build_symbols(&ast.nodes[i + 1], &mut inner)?;
                i += 2;
            }
        }
        "assignment" => {
            let name = token_str(&ast.nodes[0]);
            if scope.lookup_constant(name).is_some() {
                return Err(format!("cannot assign to constant '{name}'"));
            }
            if !scope.has_variable(name) {
                return Err(format!("undefined variable '{name}'"));
            }
            for node in &ast.nodes[1..] {
                check_idents(node, scope)?;
            }
        }
        "call" => {
            let name = token_str(&ast.nodes[0]);
            if !scope.has_procedure(name) {
                return Err(format!("undefined procedure '{name}'"));
            }
        }
        "ident" => {
            let name = token_str(ast);
            if scope.lookup_constant(name).is_none() && !scope.has_variable(name) {
                return Err(format!("undefined identifier '{name}'"));
            }
        }
        _ => {
            for node in &ast.nodes {
                build_symbols(node, scope)?;
            }
        }
    }
    Ok(())
}

fn check_idents(ast: &Ast, scope: &Scope) -> Result<(), String> {
    if ast.name == "ident" {
        let name = token_str(ast);
        if scope.lookup_constant(name).is_none() && !scope.has_variable(name) {
            return Err(format!("undefined identifier '{name}'"));
        }
    }
    for node in &ast.nodes {
        check_idents(node, scope)?;
    }
    Ok(())
}
