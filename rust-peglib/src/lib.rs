pub mod ope;
pub mod meta;

use std::any::Any;
use std::rc::Rc;
use ope::*;

pub type Action = Box<dyn Fn(&mut SemanticValues, &mut dyn Any) -> Box<dyn Any>>;
pub type Enter = Box<dyn Fn(&str, usize, &mut dyn Any)>;
pub type Leave = Box<dyn Fn(&str, usize, usize, &dyn Any, &mut dyn Any)>;

pub struct Definition {
    pub name: String,
    pub id: usize,
    pub(crate) holder: Rc<dyn Ope>,
    pub action: Option<Action>,
    pub enter: Option<Enter>,
    pub leave: Option<Leave>,
    pub ignore_semantic_value: bool,
    pub(crate) is_macro: bool,
    pub(crate) params: Vec<String>,
    pub(crate) error_message: String,
    pub no_ast_opt: bool,
    pub(crate) is_left_recursive: bool,
    pub(crate) can_be_empty: bool,
    pub disable_action: bool,
    pub(crate) is_token: bool,
    pub eoi_check: bool,
    pub packrat_hits: std::cell::Cell<usize>,
    pub packrat_misses: std::cell::Cell<usize>,
}

impl Definition {
    pub fn new(name: &str) -> Self {
        Self {
            name: name.to_string(),
            id: usize::MAX,
            holder: Rc::new(Holder { ope: None, outer: usize::MAX, is_lr: false, is_macro: false, is_token: false, ignore: false }),
            action: None,
            enter: None,
            leave: None,
            ignore_semantic_value: false,
            is_macro: false,
            params: Vec::new(),
            error_message: String::new(),
            no_ast_opt: false,
            is_left_recursive: false,
            can_be_empty: false,
            disable_action: false,
            is_token: false,
            eoi_check: true,
            packrat_hits: std::cell::Cell::new(0),
            packrat_misses: std::cell::Cell::new(0),
        }
    }

    pub fn set_body(&mut self, ope: Rc<dyn Ope>) {
        // Downcast holder to set its inner ope
        // Since we can't downcast Rc<dyn Ope>, we store the body on a separate field
        // and Holder reads it at parse time.
        // For now: replace holder entirely
        self.holder = Rc::new(Holder { ope: Some(ope), outer: self.id, is_lr: false, is_macro: false, is_token: false, ignore: false });
    }
}

pub struct Grammar {
    pub rules: Vec<Definition>,
    pub rule_index: std::collections::HashMap<String, usize>,
    pub start: usize,
    pub whitespace_ope: Option<Rc<dyn Ope>>,
    pub word_ope: Option<Rc<dyn Ope>>,
}

impl Grammar {
    pub fn set_action<F, R>(&mut self, name: &str, f: F)
    where F: Fn(&SemanticValues) -> R + 'static, R: Any
    {
        if let Some(&id) = self.rule_index.get(name) {
            self.rules[id].action = Some(Box::new(move |sv: &mut SemanticValues, _dt: &mut dyn Any| Box::new(f(sv)) as Box<dyn Any>));
        }
    }

    pub fn new() -> Self {
        Self {
            rules: Vec::new(),
            rule_index: std::collections::HashMap::new(),
            start: 0,
            whitespace_ope: None,
            word_ope: None,
        }
    }

    pub fn finalize(&mut self) {
        for i in 0..self.rules.len() {
            self.rules[i].id = i;
        }
        compute_can_be_empty(&mut self.rules);
        detect_left_recursion(&mut self.rules);
        if let Some(word) = &self.word_ope {
            mark_keyword_literals(&self.rules, word.clone());
        }
        setup_is_token(&mut self.rules);
        setup_first_sets(&self.rules);
        // Precompute Holder flags to avoid per-call pointer indirection
        for i in 0..self.rules.len() {
            let d = &self.rules[i];
            let holder = Rc::new(Holder {
                ope: d.holder.as_any().downcast_ref::<Holder>().and_then(|h| h.ope.clone()),
                outer: i,
                is_lr: d.is_left_recursive,
                is_macro: d.is_macro,
                is_token: d.is_token,
                ignore: d.ignore_semantic_value,
            });
            self.rules[i].holder = holder;
        }
    }

    pub fn add_rule(&mut self, name: &str) -> usize {
        if let Some(&id) = self.rule_index.get(name) {
            return id;
        }
        let id = self.rules.len();
        let mut def = Definition::new(name);
        def.id = id;
        self.rules.push(def);
        self.rule_index.insert(name.to_string(), id);
        id
    }

    pub fn parse(&self, input: &str) -> bool {
        let mut ctx = Context::new(input, &self.rules, self.rules.len(), true);
        ctx.whitespace_ope = self.whitespace_ope.clone();
        ctx.word_ope = self.word_ope.clone();
        let ws = ctx.skip_whitespace(0);
        let mut vs = SemanticValues::new();
        let start_def = &self.rules[self.start];
        let len = start_def.holder.parse_core(ws, &mut vs, &mut ctx);
        let eoi = start_def.eoi_check;
        success(len) && (!eoi || ws + len == input.len()) && !ctx.recovered
    }

    pub fn parse_with_value<T: 'static>(&self, input: &str, val: &mut T) -> bool {
        let mut ctx = Context::new(input, &self.rules, self.rules.len(), true);
        ctx.whitespace_ope = self.whitespace_ope.clone();
        ctx.word_ope = self.word_ope.clone();
        // Actions need SV scopes
        let has_any_action = self.rules.iter().any(|r| r.action.is_some());
        if has_any_action { ctx.enable_ast = false; }
        let ws = ctx.skip_whitespace(0);
        let mut vs = SemanticValues::new();
        let start_def = &self.rules[self.start];
        let len = start_def.holder.parse_core(ws, &mut vs, &mut ctx);
        let eoi = start_def.eoi_check;
        let ok = success(len) && (!eoi || ws + len == input.len()) && !ctx.recovered;
        if ok {
            if let Some(boxed) = ctx.root_ast.take() {
                if let Ok(v) = boxed.downcast::<T>() {
                    *val = *v;
                }
            }
        }
        ok
    }
}

impl Drop for Grammar {
    fn drop(&mut self) {
        // Break Rc cycles: Holder → Ope tree → Reference(index) is not a cycle,
        // but clear bodies to be safe.
        for rule in &mut self.rules {
            rule.holder = Rc::new(Holder { ope: None, outer: usize::MAX, is_lr: false, is_macro: false, is_token: false, ignore: false });
        }
    }
}

// Public API
pub type Log = Box<dyn Fn(usize, usize, &str)>;

pub struct Parser {
    grammar: Grammar,
    tracer: Option<Tracer>,
    logger: Option<Log>,
}

impl Parser {
    pub fn new(grammar_text: &str) -> Result<Self, ParseError> {
        Self::compile(grammar_text, None, true)
    }

    pub fn compile(
        grammar_text: &str,
        start_rule: Option<&str>,
        allow_left_recursion: bool,
    ) -> Result<Self, ParseError> {
        let g = meta::parse(grammar_text, start_rule).map_err(|msg| {
            ParseError { errors: vec![ErrorReport { line: 0, col: 0, message: msg }] }
        })?;
        if !allow_left_recursion {
            if let Some(r) = g.rules.iter().find(|r| r.is_left_recursive) {
                return Err(ParseError { errors: vec![ErrorReport { line: 0, col: 0,
                    message: format!("'{}' is left recursive but left recursion is disabled", r.name) }] });
            }
        }
        Ok(Self { grammar: g, tracer: None, logger: None })
    }

    pub fn enable_packrat_parsing(&mut self) -> &mut Self { self }

    pub fn set_action<F, R>(&mut self, name: &str, f: F)
    where F: Fn(&SemanticValues) -> R + 'static, R: Any
    {
        self.grammar.set_action(name, f);
    }

    pub fn disable_eoi_check(&mut self) {
        self.grammar.rules[self.grammar.start].eoi_check = false;
    }

    pub fn set_logger<F>(&mut self, f: F) where F: Fn(usize, usize, &str) + 'static {
        self.logger = Some(Box::new(f));
    }

    pub fn enable_trace(&mut self, tracer: Tracer) {
        self.tracer = Some(tracer);
    }

    pub fn parse(&self, input: &str) -> bool {
        let ok = self.grammar.parse(input);
        if !ok { self.log_errors(input); }
        ok
    }

    pub fn parse_with_value<T: 'static>(&self, input: &str, val: &mut T) -> bool {
        let ok = self.grammar.parse_with_value(input, val);
        if !ok { self.log_errors(input); }
        ok
    }

    pub fn parse_into<T: 'static + Default>(&self, input: &str) -> Result<T, ParseError> {
        let mut val = T::default();
        if self.grammar.parse_with_value(input, &mut val) {
            Ok(val)
        } else {
            let result = self.parse_with_errors(input, false, None);
            Err(ParseError { errors: result.errors })
        }
    }

    pub fn grammar(&self) -> &Grammar { &self.grammar }
    pub fn grammar_mut(&mut self) -> &mut Grammar { &mut self.grammar }

    pub fn optimize_ast<'a>(&self, ast: Ast<'a>) -> Ast<'a> {
        let no_opt: Vec<&str> = self.grammar.rules.iter()
            .filter(|r| r.no_ast_opt)
            .map(|r| r.name.as_str())
            .collect();
        optimize_ast_with(ast, &no_opt)
    }

    pub fn parse_with_errors<'input>(
        &'input self,
        input: &'input str,
        enable_ast: bool,
        mut handlers: Option<&mut dyn Handlers>,
    ) -> ParseResult<'input> {
        let mut ctx = Context::new(input, &self.grammar.rules, self.grammar.rules.len(), true);
        ctx.whitespace_ope = self.grammar.whitespace_ope.clone();
        ctx.word_ope = self.grammar.word_ope.clone();
        ctx.enable_ast = enable_ast;
        if let Some(ref mut h) = handlers {
            // Safety: handler pointer is valid for the duration of this parse call
            let ptr: *mut dyn Handlers = *h as *mut dyn Handlers;
            ctx.handlers = Some(unsafe { std::mem::transmute(ptr) });
        }
        let ws = ctx.skip_whitespace(0);
        let mut vs = SemanticValues::new();
        let start_def = &self.grammar.rules[self.grammar.start];
        let len = start_def.holder.parse_core(ws, &mut vs, &mut ctx);
        let eoi = self.grammar.rules[self.grammar.start].eoi_check;
        let structural_ok = success(len) && (!eoi || ws + len == input.len());
        let ok = structural_ok && !ctx.recovered;
        let mut errors: Vec<ErrorReport> = ctx.predicate_errors.iter().map(|(pos, m)| {
            let (line, col) = line_col(input, *pos);
            ErrorReport { line, col, message: m.clone() }
        }).collect();

        let fail_pos = if structural_ok {
            if ok { None } else { Some(ctx.error_info.error_pos) }
        } else if success(len) {
            Some(ws + len)
        } else {
            Some(ctx.error_info.error_pos)
        };
        let (error_line, error_col, last_error) = if ok {
            (None, None, None)
        } else if let Some((_, m)) = ctx.predicate_errors.last().cloned() {
            let pos = fail_pos.unwrap_or(0);
            let (l, c) = line_col(input, pos);
            (Some(l), Some(c), Some(m))
        } else if !ctx.error_info.message.is_empty() {
            let pos = ctx.error_info.message_pos;
            let (l, c) = line_col(input, pos);
            if errors.is_empty() {
                errors.push(ErrorReport { line: l, col: c, message: ctx.error_info.message.clone() });
            }
            (Some(l), Some(c), Some(ctx.error_info.message.clone()))
        } else {
            let pos = fail_pos.unwrap_or(0);
            let (l, c) = line_col(input, pos);
            let msg = format_syntax_error(input, ctx.error_info.error_pos, &ctx.error_info.expected_tokens, &self.grammar.rules);
            if errors.is_empty() {
                errors.push(ErrorReport { line: l, col: c, message: msg.clone() });
            }
            (Some(l), Some(c), Some(msg))
        };
        let mut ast: Option<Ast> = if ok {
            ctx.root_ast.take().and_then(|b| b.downcast::<Ast>().ok().map(|b| *b))
        } else { None };
        if let Some(ref mut a) = ast { set_parents(a); }
        ParseResult { ok, ast, errors, last_error, error_line, error_col }
    }

    pub fn parse_ast<'input>(&'input self, input: &'input str) -> Result<Ast<'input>, ParseError> {
        let result = self.parse_with_errors(input, true, None);
        if result.ok {
            Ok(result.ast.unwrap())
        } else {
            Err(ParseError { errors: result.errors })
        }
    }

    fn log_errors(&self, input: &str) {
        if let Some(ref log) = self.logger {
            let result = self.parse_with_errors(input, false, None);
            for e in &result.errors {
                log(e.line, e.col, &e.message);
            }
        }
    }
}

// Stub types for harness compatibility
#[derive(Debug)]
pub struct Ast<'input> {
    pub name: String,
    pub original_name: String,
    pub sv: &'input str,
    pub tokens: Vec<&'input str>,
    pub nodes: Vec<Ast<'input>>,
    pub parent: *const Ast<'input>,
    pub is_token: bool,
    pub choice: usize,
    pub original_choice: usize,
    pub choice_count: usize,
    pub original_choice_count: usize,
    pub tag: u32,
    pub original_tag: u32,
}

impl<'input> Clone for Ast<'input> {
    fn clone(&self) -> Self {
        let mut ast = Ast {
            name: self.name.clone(),
            original_name: self.original_name.clone(),
            sv: self.sv,
            tokens: self.tokens.clone(),
            nodes: self.nodes.clone(),
            parent: std::ptr::null(),
            is_token: self.is_token,
            choice: self.choice,
            original_choice: self.original_choice,
            choice_count: self.choice_count,
            original_choice_count: self.original_choice_count,
            tag: self.tag,
            original_tag: self.original_tag,
        };
        set_parents(&mut ast);
        ast
    }
}

pub fn str2tag(s: &str) -> u32 {
    let mut h: u32 = 0;
    for b in s.bytes() { h = h.wrapping_mul(33) ^ (b as u32); }
    h
}

#[derive(Debug, Clone)]
pub struct ErrorReport {
    pub line: usize,
    pub col: usize,
    pub message: String,
}

#[derive(Debug, Clone)]
pub struct ParseResult<'input> {
    pub ok: bool,
    pub ast: Option<Ast<'input>>,
    pub errors: Vec<ErrorReport>,
    pub last_error: Option<String>,
    pub error_line: Option<usize>,
    pub error_col: Option<usize>,
}

#[derive(Debug, Clone)]
pub struct ParseError {
    pub errors: Vec<ErrorReport>,
}

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if let Some(e) = self.errors.first() {
            write!(f, "{}:{}: {}", e.line, e.col, e.message)
        } else {
            write!(f, "parse error")
        }
    }
}

impl std::error::Error for ParseError {}

pub trait Handlers {
    fn on_enter(&mut self, _rule: &str) {}
    fn on_leave(&mut self, _rule: &str, _ok: bool) {}
    fn predicate(&mut self, _rule: &str, _sv: &str, _tokens: &[&str]) -> Result<(), String> { Ok(()) }
}

pub type TracerEnter = Box<dyn Fn(&str, usize)>;
pub type TracerLeave = Box<dyn Fn(&str, usize, bool)>;

pub struct Tracer {
    pub enter: Option<TracerEnter>,
    pub leave: Option<TracerLeave>,
}

fn mark_keyword_literals(rules: &[Definition], word: Rc<dyn Ope>) {
    for rule in rules {
        mark_kw(rule.holder.as_ref(), word.as_ref());
    }
}

fn mark_kw(ope: &dyn Ope, word: &dyn Ope) {
    let any = ope.as_any();
    if let Some(lit) = any.downcast_ref::<LiteralString>() {
        if lit.lit.is_empty() { return; }
        let text = std::str::from_utf8(&lit.lit).unwrap_or("");
        let mut vs = SemanticValues::new();
        // Create a minimal context just for word check
        let empty_rules: Vec<Definition> = Vec::new();
        let mut ctx = Context::new(text, &empty_rules, 0, false);
        let consumed = word.parse_core(0, &mut vs, &mut ctx);
        if success(consumed) && consumed >= lit.lit.len() {
            lit.is_word.set(true);
        }
        return;
    }
    if let Some(h) = any.downcast_ref::<Holder>() {
        if let Some(o) = &h.ope { mark_kw(o.as_ref(), word); }
        return;
    }
    if let Some(seq) = any.downcast_ref::<Sequence>() { for o in &seq.opes { mark_kw(o.as_ref(), word); } return; }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() { for o in &ch.opes { mark_kw(o.as_ref(), word); } return; }
    if let Some(rep) = any.downcast_ref::<Repetition>() { mark_kw(rep.ope.as_ref(), word); return; }
    if let Some(p) = any.downcast_ref::<AndPredicate>() { mark_kw(p.ope.as_ref(), word); return; }
    if let Some(p) = any.downcast_ref::<NotPredicate>() { mark_kw(p.ope.as_ref(), word); return; }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { mark_kw(tb.ope.as_ref(), word); return; }
    if let Some(cap) = any.downcast_ref::<Capture>() { mark_kw(cap.ope.as_ref(), word); return; }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { mark_kw(cs.ope.as_ref(), word); return; }
    if let Some(ig) = any.downcast_ref::<Ignore>() { mark_kw(ig.ope.as_ref(), word); return; }
    if let Some(rec) = any.downcast_ref::<Recovery>() { mark_kw(rec.ope.as_ref(), word); }
}

fn heuristic_error_token(input: &str, pos: usize) -> String {
    let bytes = input.as_bytes();
    if pos >= bytes.len() { return String::new(); }
    let first = bytes[pos];
    let mut end = pos + 1;
    let is_word = |b: u8| !b.is_ascii_whitespace() && !b.is_ascii_punctuation();
    if is_word(first) {
        while end < bytes.len() && is_word(bytes[end]) { end += 1; }
    }
    while end < bytes.len() && (bytes[end] & 0xC0) == 0x80 { end += 1; }
    input[pos..end].to_string()
}

fn format_syntax_error(input: &str, pos: usize, expected: &[(Option<*const str>, Option<usize>)], rules: &[Definition]) -> String {
    if expected.is_empty() { return "syntax error.".to_string(); }
    let mut msg = String::from("syntax error");
    let token = heuristic_error_token(input, pos);
    if !token.is_empty() {
        msg.push_str(", unexpected '");
        msg.push_str(&token);
        msg.push('\'');
    }
    let mut first = true;
    let mut seen = std::collections::HashSet::new();
    for (lit, rule_id) in expected {
        if let Some(id) = rule_id {
            if let Some(r) = rules.get(*id) {
                if r.name.starts_with('_') { continue; }
            }
        }
        let key = match (lit, rule_id) {
            (Some(l), _) => format!("'{}'", unsafe { &**l }),
            (None, Some(id)) => {
                if let Some(r) = rules.get(*id) { format!("<{}>", r.name) } else { continue; }
            }
            (None, None) => continue,
        };
        if !seen.insert(key.clone()) { continue; }
        msg.push_str(if first { ", expecting " } else { ", " });
        msg.push_str(&key);
        first = false;
    }
    msg.push('.');
    msg
}

pub fn line_col(input: &str, pos: usize) -> (usize, usize) {
    let prefix = &input[..pos.min(input.len())];
    let line = prefix.bytes().filter(|&b| b == b'\n').count() + 1;
    let col = prefix.rfind('\n').map_or(pos + 1, |n| pos - n);
    (line, col)
}

fn setup_first_sets(rules: &[Definition]) {
    for rule in rules {
        fill_first_sets(rule.holder.as_ref(), rules);
    }
}

fn fill_first_sets(ope: &dyn Ope, rules: &[Definition]) {
    let any = ope.as_any();
    if let Some(seq) = any.downcast_ref::<Sequence>() {
        setup_keyword_guard(seq, rules);
        for o in &seq.opes { fill_first_sets(o.as_ref(), rules); }
        return;
    }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() {
        let mut sets = Vec::with_capacity(ch.opes.len());
        for alt in &ch.opes {
            let mut fs = FirstSet::default();
            compute_first_set(alt.as_ref(), &mut fs, &mut std::collections::HashSet::new(), rules);
            let (lit, rule) = first_hint(alt.as_ref(), rules);
            fs.first_literal = lit;
            fs.first_rule = rule;
            sets.push(fs);
        }
        unsafe { *ch.first_sets.get() = sets; }
        for alt in &ch.opes { fill_first_sets(alt.as_ref(), rules); }
        return;
    }
    if let Some(h) = any.downcast_ref::<Holder>() { if let Some(o) = &h.ope { fill_first_sets(o.as_ref(), rules); } return; }
    if let Some(rep) = any.downcast_ref::<Repetition>() { fill_first_sets(rep.ope.as_ref(), rules); return; }
    if let Some(p) = any.downcast_ref::<AndPredicate>() { fill_first_sets(p.ope.as_ref(), rules); return; }
    if let Some(p) = any.downcast_ref::<NotPredicate>() { fill_first_sets(p.ope.as_ref(), rules); return; }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { fill_first_sets(tb.ope.as_ref(), rules); return; }
    if let Some(ws) = any.downcast_ref::<Whitespace>() { fill_first_sets(ws.ope.as_ref(), rules); return; }
    if let Some(ig) = any.downcast_ref::<Ignore>() { fill_first_sets(ig.ope.as_ref(), rules); return; }
    if let Some(cap) = any.downcast_ref::<Capture>() { fill_first_sets(cap.ope.as_ref(), rules); return; }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { fill_first_sets(cs.ope.as_ref(), rules); return; }
    if let Some(rec) = any.downcast_ref::<Recovery>() { fill_first_sets(rec.ope.as_ref(), rules); }
}

fn compute_first_set(ope: &dyn Ope, out: &mut FirstSet, seen: &mut std::collections::HashSet<usize>, rules: &[Definition]) {
    let any = ope.as_any();
    if let Some(lit) = any.downcast_ref::<LiteralString>() {
        if let Some(&b) = lit.lit.first() {
            if b < 0x80 { out.set(b); if lit.ignore_case { out.set(b.to_ascii_uppercase()); out.set(b.to_ascii_lowercase()); } }
            else { out.any_char = true; }
        } else { out.can_be_empty = true; }
        return;
    }
    if let Some(cls) = any.downcast_ref::<CharacterClass>() {
        if cls.negated { out.any_char = true; return; }
        for &(lo, hi) in &cls.ranges {
            if (lo as u32) > 0x7F || (hi as u32) > 0x7F { out.any_char = true; return; }
        }
        for &(lo, hi) in &cls.ranges {
            for cp in (lo as u32)..=(hi as u32) {
                let b = cp as u8;
                out.set(b);
                if cls.ignore_case { out.set(b.to_ascii_uppercase()); out.set(b.to_ascii_lowercase()); }
            }
        }
        return;
    }
    if any.downcast_ref::<AnyCharacter>().is_some() || any.downcast_ref::<Character>().is_some() { out.any_char = true; return; }
    if let Some(seq) = any.downcast_ref::<Sequence>() {
        for c in &seq.opes {
            let mut elem = FirstSet::default();
            compute_first_set(c.as_ref(), &mut elem, seen, rules);
            out.merge(&elem);
            if !elem.can_be_empty { return; }
        }
        out.can_be_empty = true;
        return;
    }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() {
        for c in &ch.opes { compute_first_set(c.as_ref(), out, seen, rules); }
        return;
    }
    if let Some(rep) = any.downcast_ref::<Repetition>() {
        compute_first_set(rep.ope.as_ref(), out, seen, rules);
        if rep.min == 0 { out.can_be_empty = true; }
        return;
    }
    if any.downcast_ref::<AndPredicate>().is_some() || any.downcast_ref::<NotPredicate>().is_some() || any.downcast_ref::<Cut>().is_some() { out.can_be_empty = true; return; }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { compute_first_set(tb.ope.as_ref(), out, seen, rules); return; }
    if let Some(ws) = any.downcast_ref::<Whitespace>() { compute_first_set(ws.ope.as_ref(), out, seen, rules); return; }
    if let Some(ig) = any.downcast_ref::<Ignore>() { compute_first_set(ig.ope.as_ref(), out, seen, rules); return; }
    if let Some(cap) = any.downcast_ref::<Capture>() { compute_first_set(cap.ope.as_ref(), out, seen, rules); return; }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { compute_first_set(cs.ope.as_ref(), out, seen, rules); return; }
    if let Some(rec) = any.downcast_ref::<Recovery>() { compute_first_set(rec.ope.as_ref(), out, seen, rules); return; }
    if let Some(h) = any.downcast_ref::<Holder>() {
        if let Some(o) = &h.ope { compute_first_set(o.as_ref(), out, seen, rules); }
        return;
    }
    if let Some(r) = any.downcast_ref::<Reference>() {
        if let Some(id) = r.rule_id {
            if !seen.insert(id) { return; }
            if let Some(def) = rules.get(id) { compute_first_set(def.holder.as_ref(), out, seen, rules); }
            seen.remove(&id);
        } else {
            out.any_char = true;
        }
        return;
    }
    out.any_char = true;
}

fn setup_keyword_guard(seq: &Sequence, rules: &[Definition]) {
    unsafe { *seq.kw_guard.get() = None; }
    if seq.opes.len() != 2 { return; }
    // Child 0: NotPredicate(Reference → PrioritizedChoice of case-insensitive literals)
    let not_pred = match seq.opes[0].as_any().downcast_ref::<NotPredicate>() {
        Some(np) => np, _ => return,
    };
    let refr = match not_pred.ope.as_any().downcast_ref::<Reference>() {
        Some(r) => r, _ => return,
    };
    let rule_id = match refr.rule_id { Some(id) => id, _ => return };
    let holder = match rules.get(rule_id) { Some(d) => &d.holder, _ => return };
    let holder_inner = match holder.as_any().downcast_ref::<Holder>() {
        Some(h) => h, _ => return,
    };
    let choice = match holder_inner.ope.as_ref().and_then(|o| o.as_any().downcast_ref::<PrioritizedChoice>()) {
        Some(c) => c, _ => return,
    };
    let mut exact_keywords = Vec::new();
    let mut prefix_keywords = Vec::new();
    for alt in &choice.opes {
        if let Some(lit) = alt.as_any().downcast_ref::<LiteralString>() {
            if !lit.ignore_case { return; }
            exact_keywords.push(String::from_utf8_lossy(&lit.lit).to_ascii_lowercase());
            continue;
        }
        if let Some(sub_seq) = alt.as_any().downcast_ref::<Sequence>() {
            if sub_seq.opes.is_empty() { return; }
            let all_ci = sub_seq.opes.iter().all(|o| {
                o.as_any().downcast_ref::<LiteralString>().map_or(false, |l| l.ignore_case)
            });
            if all_ci {
                if let Some(first) = sub_seq.opes[0].as_any().downcast_ref::<LiteralString>() {
                    prefix_keywords.push(String::from_utf8_lossy(&first.lit).to_ascii_lowercase());
                    continue;
                }
            }
        }
        return;
    }
    if exact_keywords.is_empty() { return; }

    // Child 1: TokenBoundary(Sequence[CharacterClass, Repetition(CharacterClass)])
    let tb = match seq.opes[1].as_any().downcast_ref::<TokenBoundary>() {
        Some(tb) => tb, _ => return,
    };
    let inner_seq = match tb.ope.as_any().downcast_ref::<Sequence>() {
        Some(s) if s.opes.len() == 2 => s, _ => return,
    };
    let first_cc = match inner_seq.opes[0].as_any().downcast_ref::<CharacterClass>() {
        Some(c) if c.is_ascii_only => c, _ => return,
    };
    let rep = match inner_seq.opes[1].as_any().downcast_ref::<Repetition>() {
        Some(r) => r, _ => return,
    };
    let rest_cc = match rep.ope.as_any().downcast_ref::<CharacterClass>() {
        Some(c) if c.is_ascii_only => c, _ => return,
    };

    let mut min_len = usize::MAX;
    let mut max_len = 0;
    for k in exact_keywords.iter().chain(prefix_keywords.iter()) {
        min_len = min_len.min(k.len());
        max_len = max_len.max(k.len());
    }

    unsafe { *seq.kw_guard.get() = Some(KeywordGuardData {
        identifier_first: first_cc.ascii_bitset,
        identifier_rest: rest_cc.ascii_bitset,
        exact_keywords, prefix_keywords,
        min_keyword_len: min_len, max_keyword_len: max_len,
    }); }
}

fn first_hint(ope: &dyn Ope, rules: &[Definition]) -> (Option<*const str>, Option<usize>) {
    let any = ope.as_any();
    if let Some(lit) = any.downcast_ref::<LiteralString>() {
        return (Some(lit.lit_ptr()), None);
    }
    if let Some(r) = any.downcast_ref::<Reference>() { return (None, r.rule_id); }
    if let Some(seq) = any.downcast_ref::<Sequence>() {
        if let Some(first) = seq.opes.first() { return first_hint(first.as_ref(), rules); }
    }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { return first_hint(tb.ope.as_ref(), rules); }
    if let Some(cap) = any.downcast_ref::<Capture>() { return first_hint(cap.ope.as_ref(), rules); }
    if let Some(rep) = any.downcast_ref::<Repetition>() { return first_hint(rep.ope.as_ref(), rules); }
    if let Some(h) = any.downcast_ref::<Holder>() {
        if let Some(o) = &h.ope { return first_hint(o.as_ref(), rules); }
    }
    (None, None)
}

fn setup_is_token(rules: &mut [Definition]) {
    for i in 0..rules.len() {
        let holder = rules[i].holder.clone();
        rules[i].is_token = check_is_token(holder.as_ref());
    }
}

fn check_is_token(ope: &dyn Ope) -> bool {
    // cpp-peglib TokenChecker: has_token_boundary_ || !has_rule_
    let mut has_token_boundary = false;
    let mut has_rule = false;
    check_is_token_walk(ope, &mut has_token_boundary, &mut has_rule);
    has_token_boundary || !has_rule
}

fn check_is_token_walk(ope: &dyn Ope, has_tb: &mut bool, has_rule: &mut bool) {
    let any = ope.as_any();
    if any.downcast_ref::<Reference>().is_some() { *has_rule = true; return; }
    if let Some(h) = any.downcast_ref::<Holder>() {
        if let Some(ref o) = h.ope { check_is_token_walk(o.as_ref(), has_tb, has_rule); }
        return;
    }
    if any.downcast_ref::<TokenBoundary>().is_some() { *has_tb = true; return; }
    if let Some(seq) = any.downcast_ref::<Sequence>() { for o in &seq.opes { check_is_token_walk(o.as_ref(), has_tb, has_rule); } return; }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() { for o in &ch.opes { check_is_token_walk(o.as_ref(), has_tb, has_rule); } return; }
    if let Some(rep) = any.downcast_ref::<Repetition>() { check_is_token_walk(rep.ope.as_ref(), has_tb, has_rule); return; }
    if let Some(p) = any.downcast_ref::<AndPredicate>() { check_is_token_walk(p.ope.as_ref(), has_tb, has_rule); return; }
    if let Some(p) = any.downcast_ref::<NotPredicate>() { check_is_token_walk(p.ope.as_ref(), has_tb, has_rule); return; }
    if let Some(cap) = any.downcast_ref::<Capture>() { check_is_token_walk(cap.ope.as_ref(), has_tb, has_rule); return; }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { check_is_token_walk(cs.ope.as_ref(), has_tb, has_rule); return; }
    if let Some(ig) = any.downcast_ref::<Ignore>() { check_is_token_walk(ig.ope.as_ref(), has_tb, has_rule); return; }
}

pub fn ast_to_s(ast: &Ast) -> String {
    let mut s = String::new();
    ast_to_s_core(ast, &mut s, 0);
    s
}

fn ast_to_s_core(ast: &Ast, s: &mut String, level: usize) {
    for _ in 0..level { s.push_str("  "); }
    let mut name = ast.original_name.clone();
    if ast.original_choice_count > 0 {
        name.push('/');
        name.push_str(&ast.original_choice.to_string());
    }
    if ast.name != ast.original_name {
        name.push('[');
        name.push_str(&ast.name);
        name.push(']');
    }
    if ast.is_token {
        s.push_str("- ");
        s.push_str(&name);
        s.push_str(" (");
        s.push_str(if ast.tokens.is_empty() { ast.sv } else { ast.tokens[0] });
        s.push_str(")\n");
    } else {
        s.push_str("+ ");
        s.push_str(&name);
        s.push('\n');
    }
    for node in &ast.nodes {
        ast_to_s_core(node, s, level + 1);
    }
}

pub fn optimize_ast(ast: Ast) -> Ast { optimize_ast_with(ast, &[]) }

pub fn optimize_ast_with<'a>(ast: Ast<'a>, no_opt_rules: &[&str]) -> Ast<'a> {
    let mut result = optimize_ast_inner(ast, true, no_opt_rules);
    set_parents(&mut result);
    result
}

fn optimize_ast_inner<'a>(ast: Ast<'a>, optimize: bool, no_opt_rules: &[&str]) -> Ast<'a> {
    let found = no_opt_rules.contains(&ast.name.as_str());
    let opt = if optimize { !found } else { found };
    if opt && ast.nodes.len() == 1 {
        let child = optimize_ast_inner(ast.nodes.into_iter().next().unwrap(), optimize, no_opt_rules);
        let original_tag = str2tag(&ast.name);
        return Ast {
            original_name: ast.name.clone(),
            original_choice: ast.choice,
            original_choice_count: ast.choice_count,
            original_tag,
            name: child.name, sv: child.sv, tokens: child.tokens,
            nodes: child.nodes, is_token: child.is_token, parent: std::ptr::null(),
            choice: child.choice, choice_count: child.choice_count,
            tag: child.tag,
        };
    }
    Ast {
        nodes: ast.nodes.into_iter().map(|n| optimize_ast_inner(n, optimize, no_opt_rules)).collect(),
        ..ast
    }
}
pub(crate) fn set_parents(ast: &mut Ast) {
    let ptr: *const Ast = ast;
    for child in &mut ast.nodes {
        child.parent = ptr;
        set_parents(child);
    }
}

pub fn grammar_builder() {}

impl std::ops::Index<&str> for Grammar {
    type Output = Definition;
    fn index(&self, name: &str) -> &Definition {
        let id = self.rule_index[name];
        &self.rules[id]
    }
}

impl std::ops::IndexMut<&str> for Grammar {
    fn index_mut(&mut self, name: &str) -> &mut Definition {
        let id = self.rule_index[name];
        &mut self.rules[id]
    }
}

fn compute_can_be_empty(rules: &mut [Definition]) {
    loop {
        let mut changed = false;
        for i in 0..rules.len() {
            if rules[i].can_be_empty { continue; }
            let body = rules[i].holder.clone();
            if can_be_empty_ope(body.as_ref(), rules) {
                rules[i].can_be_empty = true;
                changed = true;
            }
        }
        if !changed { break; }
    }
}

pub fn can_be_empty_ope(ope: &dyn Ope, rules: &[Definition]) -> bool {
    let any = ope.as_any();
    if let Some(h) = any.downcast_ref::<Holder>() {
        return h.ope.as_ref().map_or(false, |o| can_be_empty_ope(o.as_ref(), rules));
    }
    if let Some(seq) = any.downcast_ref::<Sequence>() {
        return seq.opes.iter().all(|o| can_be_empty_ope(o.as_ref(), rules));
    }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() {
        return ch.opes.iter().any(|o| can_be_empty_ope(o.as_ref(), rules));
    }
    if let Some(rep) = any.downcast_ref::<Repetition>() { return rep.min == 0; }
    if any.downcast_ref::<AndPredicate>().is_some() || any.downcast_ref::<NotPredicate>().is_some() || any.downcast_ref::<Cut>().is_some() { return true; }
    if let Some(lit) = any.downcast_ref::<LiteralString>() { return lit.lit.is_empty(); }
    if any.downcast_ref::<CharacterClass>().is_some() || any.downcast_ref::<AnyCharacter>().is_some() || any.downcast_ref::<Character>().is_some() { return false; }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { return can_be_empty_ope(tb.ope.as_ref(), rules); }
    if let Some(ws) = any.downcast_ref::<Whitespace>() { return can_be_empty_ope(ws.ope.as_ref(), rules); }
    if let Some(ig) = any.downcast_ref::<Ignore>() { return can_be_empty_ope(ig.ope.as_ref(), rules); }
    if let Some(cap) = any.downcast_ref::<Capture>() { return can_be_empty_ope(cap.ope.as_ref(), rules); }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { return can_be_empty_ope(cs.ope.as_ref(), rules); }
    if let Some(rec) = any.downcast_ref::<Recovery>() { return can_be_empty_ope(rec.ope.as_ref(), rules); }
    if let Some(r) = any.downcast_ref::<Reference>() {
        if let Some(id) = r.rule_id { return rules.get(id).map_or(false, |d| d.can_be_empty); }
    }
    false
}

fn detect_left_recursion(rules: &mut [Definition]) {
    for i in 0..rules.len() {
        let body = rules[i].holder.clone();
        let mut ctx = LrCtx { target: i, refs: std::collections::HashSet::new(), done: false, found: false };
        visit_lr(body.as_ref(), &mut ctx, rules);
        if ctx.found {
            rules[i].is_left_recursive = true;
        }
    }
}

struct LrCtx { target: usize, refs: std::collections::HashSet<usize>, done: bool, found: bool }

fn visit_lr(ope: &dyn Ope, c: &mut LrCtx, rules: &[Definition]) {
    if c.done || c.found { return; }
    let any = ope.as_any();
    if let Some(h) = any.downcast_ref::<Holder>() {
        if let Some(o) = &h.ope { visit_lr(o.as_ref(), c, rules); }
        return;
    }
    if let Some(seq) = any.downcast_ref::<Sequence>() {
        for o in &seq.opes { visit_lr(o.as_ref(), c, rules); if c.done || c.found { break; } }
        return;
    }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() {
        let saved = c.done;
        for o in &ch.opes { c.done = saved; visit_lr(o.as_ref(), c, rules); if c.found { break; } }
        return;
    }
    if let Some(rep) = any.downcast_ref::<Repetition>() {
        visit_lr(rep.ope.as_ref(), c, rules);
        c.done = rep.min > 0;
        return;
    }
    if let Some(p) = any.downcast_ref::<AndPredicate>() { let s = c.done; visit_lr(p.ope.as_ref(), c, rules); c.done = s; return; }
    if let Some(p) = any.downcast_ref::<NotPredicate>() { let s = c.done; visit_lr(p.ope.as_ref(), c, rules); c.done = s; return; }
    if let Some(lit) = any.downcast_ref::<LiteralString>() { c.done = !lit.lit.is_empty(); return; }
    if any.downcast_ref::<CharacterClass>().is_some() || any.downcast_ref::<AnyCharacter>().is_some() || any.downcast_ref::<Character>().is_some() || any.downcast_ref::<Cut>().is_some() { c.done = true; return; }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { visit_lr(tb.ope.as_ref(), c, rules); return; }
    if let Some(ws) = any.downcast_ref::<Whitespace>() { visit_lr(ws.ope.as_ref(), c, rules); return; }
    if let Some(ig) = any.downcast_ref::<Ignore>() { visit_lr(ig.ope.as_ref(), c, rules); return; }
    if let Some(cap) = any.downcast_ref::<Capture>() { visit_lr(cap.ope.as_ref(), c, rules); return; }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { visit_lr(cs.ope.as_ref(), c, rules); return; }
    if let Some(rec) = any.downcast_ref::<Recovery>() { visit_lr(rec.ope.as_ref(), c, rules); return; }
    if let Some(r) = any.downcast_ref::<Reference>() {
        if let Some(id) = r.rule_id {
            if id == c.target { c.found = true; c.done = true; return; }
            if c.refs.insert(id) {
                if let Some(def) = rules.get(id) { visit_lr(def.holder.as_ref(), c, rules); if c.found { return; } }
            }
            c.done = !rules.get(id).map_or(false, |d| d.can_be_empty);
        } else {
            c.done = true;
        }
        return;
    }
    c.done = true;
}
