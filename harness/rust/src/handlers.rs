//! Harness-side `peglib::Handlers` driving the spec's `vars`,
//! `handlers.{enter,leave,predicate}`, and `trace` blocks.

use peglib::Handlers;
use regex::Regex;
use serde_json::Value as Json;
use std::collections::{HashMap, HashSet};

#[derive(Debug, Clone)]
pub enum VarValue {
    Bool(bool),
    Int(i64),
    StrList(Vec<String>),
}

#[derive(Default)]
pub struct VarState {
    vars: HashMap<String, VarValue>,
}

impl VarState {
    pub fn init(vars_def: &Json) -> Self {
        let mut state = Self::default();
        if let Some(obj) = vars_def.as_object() {
            for (name, def) in obj {
                let ty = def.get("type").and_then(|v| v.as_str()).unwrap_or("");
                let init = def.get("init");
                let val = match ty {
                    "bool" => VarValue::Bool(init.and_then(|v| v.as_bool()).unwrap_or(false)),
                    "int" => VarValue::Int(init.and_then(|v| v.as_i64()).unwrap_or(0)),
                    "string[]" => {
                        let list = init
                            .and_then(|v| v.as_array())
                            .map(|a| {
                                a.iter()
                                    .filter_map(|e| e.as_str().map(String::from))
                                    .collect()
                            })
                            .unwrap_or_default();
                        VarValue::StrList(list)
                    }
                    _ => VarValue::Int(0),
                };
                state.vars.insert(name.clone(), val);
            }
        }
        state
    }

    fn get_bool(&self, name: &str) -> bool {
        matches!(self.vars.get(name), Some(VarValue::Bool(true)))
    }

    fn get_int(&self, name: &str) -> Option<i64> {
        if let Some(VarValue::Int(n)) = self.vars.get(name) {
            Some(*n)
        } else {
            None
        }
    }

    fn get_string_list(&self, name: &str) -> Option<&[String]> {
        if let Some(VarValue::StrList(v)) = self.vars.get(name) {
            Some(v.as_slice())
        } else {
            None
        }
    }

    /// Literal bool / int or a simple `"var + N"` / `"var - N"` expression.
    fn set(&mut self, name: &str, value_expr: &Json) {
        if let Some(b) = value_expr.as_bool() {
            self.vars.insert(name.to_string(), VarValue::Bool(b));
            return;
        }
        if let Some(n) = value_expr.as_i64() {
            self.vars.insert(name.to_string(), VarValue::Int(n));
            return;
        }
        if let Some(expr) = value_expr.as_str() {
            if let Some(rest) = expr.split_once(" + ") {
                let base = self.get_int(rest.0).unwrap_or(0);
                let delta: i64 = rest.1.parse().unwrap_or(0);
                self.vars.insert(name.to_string(), VarValue::Int(base + delta));
                return;
            }
            if let Some(rest) = expr.split_once(" - ") {
                let base = self.get_int(rest.0).unwrap_or(0);
                let delta: i64 = rest.1.parse().unwrap_or(0);
                self.vars.insert(name.to_string(), VarValue::Int(base - delta));
                return;
            }
        }
    }

    pub fn matches(&self, expected: &Json) -> bool {
        let Some(obj) = expected.as_object() else {
            return true;
        };
        for (name, ev) in obj {
            match self.vars.get(name) {
                Some(VarValue::Bool(b)) => {
                    if ev.as_bool() != Some(*b) {
                        return false;
                    }
                }
                Some(VarValue::Int(n)) => {
                    if ev.as_i64() != Some(*n) {
                        return false;
                    }
                }
                Some(VarValue::StrList(_)) | None => return false,
            }
        }
        true
    }
}

pub struct SpecHandlers {
    pub state: VarState,
    pub trace: Vec<String>,
    handlers_json: Json,
    trace_rules: HashSet<String>,
    trace_events: HashSet<String>,
}

impl SpecHandlers {
    pub fn new(
        vars_def: Option<&Json>,
        handlers_json: Option<&Json>,
        trace_def: Option<&Json>,
    ) -> Self {
        let state = vars_def.map(VarState::init).unwrap_or_default();
        let handlers_json = handlers_json.cloned().unwrap_or(Json::Null);
        let (trace_rules, trace_events) = match trace_def {
            Some(t) => (
                collect_string_set(t.get("rules")),
                collect_string_set(t.get("events")),
            ),
            None => (HashSet::new(), HashSet::new()),
        };
        Self {
            state,
            trace: Vec::new(),
            handlers_json,
            trace_rules,
            trace_events,
        }
    }

    fn rule_block(&self, rule: &str) -> Option<&Json> {
        self.handlers_json.get(rule)
    }

    fn record(&mut self, event: &str, rule: &str) {
        if self.trace_rules.contains(rule) && self.trace_events.contains(event) {
            self.trace.push(format!("{event}_{rule}"));
        }
    }
}

fn collect_string_set(val: Option<&Json>) -> HashSet<String> {
    val.and_then(|v| v.as_array())
        .map(|a| {
            a.iter()
                .filter_map(|e| e.as_str().map(String::from))
                .collect()
        })
        .unwrap_or_default()
}

impl Handlers for SpecHandlers {
    fn on_enter(&mut self, rule: &str) {
        self.record("enter", rule);
        apply_set_for_event(&self.handlers_json, &mut self.state, rule, "enter");
    }

    fn on_leave(&mut self, rule: &str, _ok: bool) {
        // cpp-peglib fires `leave` on both success and failure so paired
        // counters stay balanced across backtracking.
        self.record("leave", rule);
        apply_set_for_event(&self.handlers_json, &mut self.state, rule, "leave");
    }

    fn predicate(
        &mut self,
        rule: &str,
        sv: &str,
        tokens: &[&str],
    ) -> Result<(), String> {
        let Some(block) = self.rule_block(rule) else {
            return Ok(());
        };
        let Some(pred) = block.get("predicate") else {
            return Ok(());
        };

        // `when`: skip the predicate unless the named bool var is true.
        if let Some(when) = pred.get("when").and_then(|v| v.as_str()) {
            if !self.state.get_bool(when) {
                return Ok(());
            }
        }

        let error_msg = pred
            .get("error")
            .and_then(|v| v.as_str())
            .unwrap_or("predicate failed")
            .to_string();
        let check = pred.get("check").and_then(|c| c.as_object());
        let Some(check) = check else {
            return Ok(());
        };

        let token0 = tokens.first().copied().unwrap_or(sv);

        for (prop, comparison) in check {
            let passed = match prop.as_str() {
                "sv" => check_sv(sv, comparison, &self.state),
                "sv.length" => check_int(sv.chars().count() as i64, comparison, &self.state),
                "token_number" => {
                    let n: i64 = token0.parse().unwrap_or(0);
                    check_int(n, comparison, &self.state)
                }
                "token_string" => check_token_string(token0, comparison, &self.state),
                _ => true,
            };
            if !passed {
                return Err(error_msg);
            }
        }
        Ok(())
    }
}

fn apply_set_for_event(handlers_json: &Json, state: &mut VarState, rule: &str, event: &str) {
    if let Some(set) = handlers_json
        .get(rule)
        .and_then(|b| b.get(event))
        .and_then(|e| e.get("set"))
        .and_then(|s| s.as_object())
    {
        for (var, val) in set {
            state.set(var, val);
        }
    }
}

fn check_sv(sv: &str, comparison: &Json, state: &VarState) -> bool {
    let Some(obj) = comparison.as_object() else {
        return true;
    };
    for (op, val) in obj {
        match op.as_str() {
            "matches" => {
                let pattern = val.as_str().unwrap_or("");
                match Regex::new(pattern) {
                    Ok(re) => {
                        if !re.is_match(sv) {
                            return false;
                        }
                    }
                    Err(_) => return false,
                }
            }
            "eq" => {
                let expected = resolve_string(val, state);
                if sv != expected {
                    return false;
                }
            }
            _ => {}
        }
    }
    true
}

fn check_int(n: i64, comparison: &Json, state: &VarState) -> bool {
    let Some(obj) = comparison.as_object() else {
        return true;
    };
    for (op, val) in obj {
        match op.as_str() {
            "eq" => {
                let expected = resolve_int(val, state);
                if n != expected {
                    return false;
                }
            }
            "between" => {
                if let Some(range) = val.as_array() {
                    if range.len() == 2 {
                        let lo = range[0].as_i64().unwrap_or(i64::MIN);
                        let hi = range[1].as_i64().unwrap_or(i64::MAX);
                        if n < lo || n > hi {
                            return false;
                        }
                    }
                }
            }
            _ => {}
        }
    }
    true
}

fn check_token_string(tok: &str, comparison: &Json, state: &VarState) -> bool {
    let Some(obj) = comparison.as_object() else {
        return true;
    };
    for (op, val) in obj {
        match op.as_str() {
            "eq" => {
                let expected = resolve_string(val, state);
                if tok != expected {
                    return false;
                }
            }
            "not_in" => {
                if let Some(var_name) = val.as_str() {
                    if let Some(list) = state.get_string_list(var_name) {
                        if list.iter().any(|s| s == tok) {
                            return false;
                        }
                    }
                }
            }
            _ => {}
        }
    }
    true
}

fn resolve_string(val: &Json, _state: &VarState) -> String {
    val.as_str().unwrap_or("").to_string()
}

fn resolve_int(val: &Json, state: &VarState) -> i64 {
    if let Some(n) = val.as_i64() {
        return n;
    }
    if let Some(var_name) = val.as_str() {
        return state.get_int(var_name).unwrap_or(0);
    }
    0
}
