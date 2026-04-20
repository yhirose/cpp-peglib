//! Post-parse evaluation of the spec's JSON action DSL. Since the port
//! does not yet run semantic actions natively, the harness walks the
//! AST produced by `parse_ast` and applies each rule's `actions.rule.op`.

use peglib::Ast;
use serde_json::Value as Json;
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub enum Value {
    None,
    Int(i64),
    Float(f64),
    Str(String),
}

impl Value {
    fn as_int(&self) -> Option<i64> {
        match self {
            Value::Int(n) => Some(*n),
            Value::Float(f) => Some(*f as i64),
            _ => None,
        }
    }

    fn as_str(&self) -> Option<&str> {
        match self {
            Value::Str(s) => Some(s),
            _ => None,
        }
    }
}

pub fn value_matches(actual: &Value, expected: &Json) -> bool {
    match (actual, expected) {
        (Value::Int(n), Json::Number(e)) => {
            if let Some(ei) = e.as_i64() {
                *n == ei
            } else if let Some(ef) = e.as_f64() {
                (*n as f64 - ef).abs() < f64::EPSILON
            } else {
                false
            }
        }
        (Value::Float(f), Json::Number(e)) => {
            if let Some(ef) = e.as_f64() {
                (f - ef).abs() < 1e-9
            } else {
                false
            }
        }
        (Value::Str(s), Json::String(e)) => s == e,
        _ => false,
    }
}

pub struct Evaluator<'a> {
    actions: HashMap<String, &'a Json>,
}

impl<'a> Evaluator<'a> {
    pub fn new(actions_json: &'a Json) -> Self {
        let mut actions: HashMap<String, &'a Json> = HashMap::new();
        if let Some(obj) = actions_json.as_object() {
            for (k, v) in obj {
                actions.insert(k.clone(), v);
            }
        }
        // Auto-bind `operator_rule` to `token_to_string` for `reduce`,
        // matching the C++ harness.
        let mut implicit: Vec<(String, &'a Json)> = Vec::new();
        for v in actions.values() {
            if v.get("op").and_then(|x| x.as_str()) == Some("reduce") {
                if let Some(op_rule) = v.get("operator_rule").and_then(|x| x.as_str()) {
                    if !actions.contains_key(op_rule) {
                        implicit.push((op_rule.to_string(), &*TOKEN_TO_STRING));
                    }
                }
            }
        }
        for (k, v) in implicit {
            actions.insert(k, v);
        }
        Self { actions }
    }

    pub fn eval(&self, ast: &Ast) -> Value {
        let children: Vec<Value> = ast.nodes.iter().map(|n| self.eval(n)).collect();

        let Some(action) = self.actions.get(ast.name.as_str()).copied() else {
            // Default: passthrough the first child, or fall back to sv.
            if let Some(first) = children.into_iter().next() {
                return first;
            }
            return Value::Str(ast.sv.to_string());
        };

        let op = action.get("op").and_then(|v| v.as_str()).unwrap_or("");
        match op {
            "token_to_number" => {
                let text = token0(ast);
                let ty = action
                    .get("type")
                    .and_then(|v| v.as_str())
                    .unwrap_or("long");
                if ty == "float" || ty == "double" {
                    Value::Float(text.parse::<f64>().unwrap_or(0.0))
                } else {
                    Value::Int(text.parse::<i64>().unwrap_or(0))
                }
            }
            "token_to_string" => Value::Str(token0(ast).to_string()),
            "choice" => Value::Int(ast.choice as i64),
            "size" => Value::Int(children.len() as i64),
            "passthrough" => {
                let idx = action
                    .get("index")
                    .and_then(|v| v.as_u64())
                    .unwrap_or(0) as usize;
                children.get(idx).cloned().unwrap_or(Value::None)
            }
            "choice_op" => apply_choice_op(action, ast.choice, &children),
            "reduce" => apply_reduce(action, &children),
            _ => Value::None,
        }
    }
}

fn token0<'i>(ast: &Ast<'i>) -> &'i str {
    ast.tokens.first().copied().unwrap_or(ast.sv)
}

fn apply_choice_op(action: &Json, choice: usize, children: &[Value]) -> Value {
    let cases = match action.get("cases") {
        Some(c) => c,
        None => return Value::None,
    };
    let key = choice.to_string();
    let case = cases.get(&key).or_else(|| cases.get("default"));
    let Some(case) = case else {
        return Value::None;
    };
    if let Some(binary) = case.get("binary").and_then(|v| v.as_str()) {
        let l = case
            .get("left")
            .and_then(|v| v.as_u64())
            .and_then(|i| children.get(i as usize))
            .and_then(|v| v.as_int())
            .unwrap_or(0);
        let r = case
            .get("right")
            .and_then(|v| v.as_u64())
            .and_then(|i| children.get(i as usize))
            .and_then(|v| v.as_int())
            .unwrap_or(0);
        return Value::Int(apply_binary(binary, l, r));
    }
    if let Some(idx) = case.get("passthrough").and_then(|v| v.as_u64()) {
        return children
            .get(idx as usize)
            .cloned()
            .unwrap_or(Value::None);
    }
    Value::None
}

fn apply_reduce(action: &Json, children: &[Value]) -> Value {
    if children.is_empty() {
        return Value::None;
    }
    let mut acc = children[0].as_int().unwrap_or(0);
    let step = action.get("step");
    let operators = step.and_then(|s| s.get("operators")).and_then(|o| o.as_object());
    let mut i = 1;
    while i + 1 < children.len() {
        let op_str = children[i].as_str().unwrap_or("");
        let val = children[i + 1].as_int().unwrap_or(0);
        if let Some(ops) = operators {
            if let Some(name) = ops.get(op_str).and_then(|v| v.as_str()) {
                acc = apply_binary(name, acc, val);
            }
        }
        i += 2;
    }
    Value::Int(acc)
}

fn apply_binary(op: &str, l: i64, r: i64) -> i64 {
    match op {
        "add" => l + r,
        "subtract" => l - r,
        "multiply" => l * r,
        "divide" => {
            if r == 0 {
                0
            } else {
                l / r
            }
        }
        _ => 0,
    }
}

use std::sync::LazyLock;
static TOKEN_TO_STRING: LazyLock<Json> = LazyLock::new(|| {
    serde_json::json!({ "op": "token_to_string" })
});
