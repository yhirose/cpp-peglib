use crate::ope::*;
use crate::Grammar;
use std::collections::HashMap;
use std::rc::Rc;

const UNICODE_LEFT_ARROW: &str = "\u{2190}";
const UNICODE_CUT: &str = "\u{2191}";

pub fn parse(input: &str, start_override: Option<&str>) -> Result<Grammar, String> {
    let mut p = Meta::new(input);
    p.parse_grammar()?;
    // Check undefined rules
    for (name, &_id) in &p.rule_ids {
        if !p.defined.contains(name) {
            return Err(format!("undefined rule: {name}"));
        }
    }
    let start_idx = match start_override {
        Some(name) => *p.rule_ids.get(name).ok_or_else(|| format!("start rule '{name}' not found"))?,
        None => {
            let want = p.first_defined.as_deref().ok_or_else(|| "empty grammar".to_string())?;
            *p.rule_ids.get(want).ok_or_else(|| format!("start rule '{want}' not found"))?
        }
    };
    p.grammar.start = start_idx;
    if let Some(ws) = p.whitespace {
        p.grammar.whitespace_ope = Some(ws);
    }
    if let Some(word) = p.word {
        p.grammar.word_ope = Some(word);
    }
    p.grammar.finalize();

    // Validation passes
    if p.grammar.rules[start_idx].ignore_semantic_value {
        return Err("start rule cannot use ignore operator '~'".into());
    }
    check_infinite_loop(&p.grammar)?;
    check_macro_references(&p.grammar)?;
    check_backreferences(&p.grammar)?;

    Ok(p.grammar)
}

fn check_infinite_loop(g: &crate::Grammar) -> Result<(), String> {
    for rule in &g.rules {
        if rule.is_left_recursive { continue; }
        walk_infinite(rule.holder.as_ref(), &rule.name, &g.rules)?;
    }
    if let Some(ws) = &g.whitespace_ope {
        walk_infinite(ws.as_ref(), "%whitespace", &g.rules)?;
    }
    if let Some(w) = &g.word_ope {
        walk_infinite(w.as_ref(), "%word", &g.rules)?;
    }
    Ok(())
}

fn walk_infinite(ope: &dyn Ope, rule: &str, rules: &[crate::Definition]) -> Result<(), String> {
    let any = ope.as_any();
    if let Some(h) = any.downcast_ref::<Holder>() {
        if let Some(o) = &h.ope { return walk_infinite(o.as_ref(), rule, rules); }
        return Ok(());
    }
    if let Some(rep) = any.downcast_ref::<Repetition>() {
        if rep.max == usize::MAX && crate::can_be_empty_ope(rep.ope.as_ref(), rules) {
            return Err(format!("'{rule}' has an infinite loop"));
        }
        return walk_infinite(rep.ope.as_ref(), rule, rules);
    }
    if let Some(seq) = any.downcast_ref::<Sequence>() {
        for o in &seq.opes { walk_infinite(o.as_ref(), rule, rules)?; }
        return Ok(());
    }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() {
        for o in &ch.opes { walk_infinite(o.as_ref(), rule, rules)?; }
        return Ok(());
    }
    if let Some(p) = any.downcast_ref::<AndPredicate>() { return walk_infinite(p.ope.as_ref(), rule, rules); }
    if let Some(p) = any.downcast_ref::<NotPredicate>() { return walk_infinite(p.ope.as_ref(), rule, rules); }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { return walk_infinite(tb.ope.as_ref(), rule, rules); }
    if let Some(ws) = any.downcast_ref::<Whitespace>() { return walk_infinite(ws.ope.as_ref(), rule, rules); }
    if let Some(ig) = any.downcast_ref::<Ignore>() { return walk_infinite(ig.ope.as_ref(), rule, rules); }
    if let Some(cap) = any.downcast_ref::<Capture>() { return walk_infinite(cap.ope.as_ref(), rule, rules); }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { return walk_infinite(cs.ope.as_ref(), rule, rules); }
    if let Some(rec) = any.downcast_ref::<Recovery>() { return walk_infinite(rec.ope.as_ref(), rule, rules); }
    Ok(())
}

fn check_macro_references(g: &Grammar) -> Result<(), String> {
    for rule in &g.rules {
        walk_macro_refs(rule.holder.as_ref(), &g.rules)?;
    }
    Ok(())
}

fn walk_macro_refs(ope: &dyn Ope, rules: &[crate::Definition]) -> Result<(), String> {
    let any = ope.as_any();
    if let Some(h) = any.downcast_ref::<Holder>() {
        if let Some(o) = &h.ope { return walk_macro_refs(o.as_ref(), rules); }
        return Ok(());
    }
    if let Some(r) = any.downcast_ref::<Reference>() {
        if let Some(id) = r.rule_id {
            if r.is_macro {
                let def = &rules[id];
                if !def.is_macro { return Err(format!("'{}' is not a macro but called with arguments", def.name)); }
                if def.params.len() != r.args.len() { return Err(format!("macro '{}' takes {} args, got {}", def.name, def.params.len(), r.args.len())); }
                for a in &r.args { walk_macro_refs(a.as_ref(), rules)?; }
            } else if r.iarg == 0 {
                let def = &rules[id];
                if def.is_macro { return Err(format!("macro '{}' must be invoked with arguments", def.name)); }
            }
        }
        return Ok(());
    }
    if let Some(seq) = any.downcast_ref::<Sequence>() { for o in &seq.opes { walk_macro_refs(o.as_ref(), rules)?; } return Ok(()); }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() { for o in &ch.opes { walk_macro_refs(o.as_ref(), rules)?; } return Ok(()); }
    if let Some(rep) = any.downcast_ref::<Repetition>() { return walk_macro_refs(rep.ope.as_ref(), rules); }
    if let Some(p) = any.downcast_ref::<AndPredicate>() { return walk_macro_refs(p.ope.as_ref(), rules); }
    if let Some(p) = any.downcast_ref::<NotPredicate>() { return walk_macro_refs(p.ope.as_ref(), rules); }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { return walk_macro_refs(tb.ope.as_ref(), rules); }
    if let Some(cap) = any.downcast_ref::<Capture>() { return walk_macro_refs(cap.ope.as_ref(), rules); }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { return walk_macro_refs(cs.ope.as_ref(), rules); }
    if let Some(rec) = any.downcast_ref::<Recovery>() { return walk_macro_refs(rec.ope.as_ref(), rules); }
    Ok(())
}

fn check_backreferences(g: &Grammar) -> Result<(), String> {
    let mut captures = std::collections::HashSet::new();
    for rule in &g.rules { collect_captures(rule.holder.as_ref(), &mut captures); }
    for rule in &g.rules { walk_backrefs(rule.holder.as_ref(), &captures)?; }
    Ok(())
}

fn collect_captures(ope: &dyn Ope, out: &mut std::collections::HashSet<String>) {
    let any = ope.as_any();
    if let Some(cap) = any.downcast_ref::<Capture>() {
        out.insert(cap.name.clone());
        collect_captures(cap.ope.as_ref(), out);
        return;
    }
    for_each_child(ope, &mut |c| collect_captures(c, out));
}

fn walk_backrefs(ope: &dyn Ope, captures: &std::collections::HashSet<String>) -> Result<(), String> {
    let any = ope.as_any();
    if let Some(br) = any.downcast_ref::<BackReference>() {
        if !captures.contains(&br.name) { return Err(format!("undefined backreference '${}'", br.name)); }
        return Ok(());
    }
    let mut result = Ok(());
    for_each_child(ope, &mut |c| { if result.is_ok() { result = walk_backrefs(c, captures); } });
    result
}

fn for_each_child(ope: &dyn Ope, f: &mut dyn FnMut(&dyn Ope)) {
    let any = ope.as_any();
    if let Some(seq) = any.downcast_ref::<Sequence>() { for o in &seq.opes { f(o.as_ref()); } return; }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() { for o in &ch.opes { f(o.as_ref()); } return; }
    if let Some(rep) = any.downcast_ref::<Repetition>() { f(rep.ope.as_ref()); return; }
    if let Some(p) = any.downcast_ref::<AndPredicate>() { f(p.ope.as_ref()); return; }
    if let Some(p) = any.downcast_ref::<NotPredicate>() { f(p.ope.as_ref()); return; }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() { f(tb.ope.as_ref()); return; }
    if let Some(cap) = any.downcast_ref::<Capture>() { f(cap.ope.as_ref()); return; }
    if let Some(cs) = any.downcast_ref::<CaptureScope>() { f(cs.ope.as_ref()); return; }
    if let Some(ws) = any.downcast_ref::<Whitespace>() { f(ws.ope.as_ref()); return; }
    if let Some(rec) = any.downcast_ref::<Recovery>() { f(rec.ope.as_ref()); return; }
    if let Some(ig) = any.downcast_ref::<Ignore>() { f(ig.ope.as_ref()); return; }
    if let Some(h) = any.downcast_ref::<Holder>() { if let Some(o) = &h.ope { f(o.as_ref()); } return; }
    if let Some(r) = any.downcast_ref::<Reference>() {
        if r.is_macro { for a in &r.args { f(a.as_ref()); } }
    }
}

struct Meta<'a> {
    input: &'a str,
    pos: usize,
    grammar: Grammar,
    rule_ids: HashMap<String, usize>,
    defined: std::collections::HashSet<String>,
    first_defined: Option<String>,
    whitespace: Option<Rc<dyn Ope>>,
    word: Option<Rc<dyn Ope>>,
    current_params: Vec<String>,
}

impl<'a> Meta<'a> {
    fn new(input: &'a str) -> Self {
        Self {
            input, pos: 0,
            grammar: Grammar::new(),
            rule_ids: HashMap::new(),
            defined: std::collections::HashSet::new(),
            first_defined: None,
            whitespace: None,
            word: None,
            current_params: Vec::new(),
        }
    }

    fn bytes(&self) -> &'a [u8] { self.input.as_bytes() }

    fn get_or_create_rule(&mut self, name: &str) -> usize {
        if let Some(&id) = self.rule_ids.get(name) {
            return id;
        }
        let id = self.grammar.add_rule(name);
        self.rule_ids.insert(name.to_string(), id);
        id
    }

    fn parse_grammar(&mut self) -> Result<(), String> {
        self.skip_spacing();
        while self.pos < self.input.len() {
            self.parse_definition()?;
            self.skip_spacing();
        }
        if self.grammar.rules.is_empty() {
            return Err("empty grammar".into());
        }
        Ok(())
    }

    fn parse_definition(&mut self) -> Result<(), String> {
        let is_ignore = self.consume_byte(b'~');
        let is_special = self.consume_byte(b'%');
        let name = self.parse_identifier()?;
        let full_name = if is_special { format!("%{name}") } else { name.clone() };

        let mut params: Vec<String> = Vec::new();
        if self.peek_byte() == Some(b'(') {
            self.pos += 1;
            self.skip_spacing();
            if self.peek_byte() != Some(b')') {
                loop {
                    let p = self.parse_identifier()?;
                    params.push(p);
                    self.skip_spacing();
                    if !self.consume_byte(b',') { break; }
                    self.skip_spacing();
                }
            }
            if !self.consume_byte(b')') {
                return Err(self.err("expected ')' in macro parameter list"));
            }
        }
        self.skip_spacing();
        if !self.consume_str("<-") && !self.consume_str(UNICODE_LEFT_ARROW) {
            return Err(self.err(&format!("expected '<-' after rule name '{full_name}'")));
        }
        self.skip_spacing();

        let rule_id = if !is_special {
            let id = self.get_or_create_rule(&full_name);
            if self.defined.contains(&full_name) {
                return Err(format!("'{full_name}' is already defined"));
            }
            if !params.is_empty() {
                self.grammar.rules[id].is_macro = true;
                self.grammar.rules[id].params = params.clone();
            }
            Some(id)
        } else {
            None
        };

        let saved_params = std::mem::replace(&mut self.current_params, params);
        let body_result = self.parse_expression();
        self.current_params = saved_params;
        let mut body = body_result?;

        if is_special && name == "whitespace" {
            self.whitespace = Some(Rc::new(Whitespace { ope: body }));
            return Ok(());
        }
        if is_special && name == "word" {
            self.word = Some(body);
            return Ok(());
        }
        if is_special { return Ok(()); }

        let id = rule_id.unwrap();
        self.skip_spacing();
        if self.peek_byte() == Some(b'{') {
            let block_start = self.pos + 1;
            self.consume_balanced_braces();
            let block_end = self.pos.saturating_sub(1);
            let block = &self.input[block_start..block_end];
            if block.contains("precedence") {
                validate_precedence_shape(&body)?;
                // Replace body with PrecedenceClimbing Ope
                if let Some(seq) = body.as_any().downcast_ref::<Sequence>() {
                    if seq.opes.len() == 2 {
                        let atom = seq.opes[0].clone();
                        let rep = seq.opes[1].as_any().downcast_ref::<Repetition>().unwrap();
                        let inner = rep.ope.as_any().downcast_ref::<Sequence>().unwrap();
                        let binop = inner.opes[0].clone();
                        let info = parse_precedence_info(block);
                        body = Rc::new(PrecedenceClimbing { atom, binop, info, rule_id: id });
                    }
                }
            }
            if let Some(msg) = extract_error_message(block) {
                self.grammar.rules[id].error_message = msg;
            }
        }
        if is_ignore {
            self.grammar.rules[id].ignore_semantic_value = true;
        }
        self.grammar.rules[id].set_body(body);
        self.defined.insert(full_name.clone());
        if self.first_defined.is_none() {
            self.first_defined = Some(full_name);
        }
        Ok(())
    }

    fn parse_expression(&mut self) -> Result<Rc<dyn Ope>, String> {
        let first = self.parse_sequence()?;
        let mut alts = vec![first];
        let mut saw_pipe = false;
        let mut saw_slash = false;
        loop {
            self.skip_spacing();
            if self.consume_byte(b'/') { saw_slash = true; }
            else if self.consume_byte(b'|') { saw_pipe = true; }
            else { break; }
            self.skip_spacing();
            alts.push(self.parse_sequence()?);
        }
        if alts.len() == 1 { return Ok(alts.into_iter().next().unwrap()); }
        if saw_pipe {
            if saw_slash { return Err(self.err("cannot mix `|` and `/`")); }
            let lits: Vec<&LiteralString> = alts.iter().filter_map(|a| {
                a.as_any().downcast_ref::<LiteralString>()
            }).collect();
            if lits.len() != alts.len() { return Err(self.err("`|` requires literal alternatives")); }
            let first_ci = lits[0].ignore_case;
            if !lits.iter().all(|l| l.ignore_case == first_ci) {
                return Err(self.err("`|` dictionary alternatives must share case-sensitivity"));
            }
            let literals: Vec<Vec<u8>> = lits.iter().map(|l| l.lit.clone()).collect();
            return Ok(Rc::new(Dictionary { trie: Trie::new(&literals, first_ci) }));
        }
        Ok(Rc::new(PrioritizedChoice::new(alts)))
    }

    fn parse_sequence(&mut self) -> Result<Rc<dyn Ope>, String> {
        let mut items: Vec<Rc<dyn Ope>> = Vec::new();
        loop {
            self.skip_spacing();
            if self.at_sequence_terminator() { break; }
            items.push(self.parse_prefix()?);
        }
        Ok(match items.len() {
            0 => Rc::new(Sequence { opes: vec![], kw_guard: std::cell::UnsafeCell::new(None) }),
            1 => items.into_iter().next().unwrap(),
            _ => Rc::new(Sequence { opes: items, kw_guard: std::cell::UnsafeCell::new(None) }),
        })
    }

    fn at_sequence_terminator(&self) -> bool {
        let Some(b) = self.peek_byte() else { return true; };
        match b {
            b'/' | b'|' | b')' | b'>' | b'}' | b',' | b'{' => return true,
            _ => {}
        }
        if b == b'%' {
            if self.input[self.pos..].starts_with("%recover") { return false; }
            return true;
        }
        if !is_ident_start(b) && b != b'~' { return false; }
        self.lookahead_is_definition(self.pos)
    }

    fn lookahead_is_definition(&self, start: usize) -> bool {
        let bytes = self.bytes();
        let mut q = start;
        if bytes.get(q).copied() == Some(b'~') { q += 1; }
        if !bytes.get(q).copied().map_or(false, is_ident_start) { return false; }
        while q < bytes.len() && is_ident_cont(bytes[q]) { q += 1; }
        if bytes.get(q).copied() == Some(b'(') {
            let mut depth = 1; q += 1;
            while q < bytes.len() && depth > 0 {
                match bytes[q] { b'(' => depth += 1, b')' => depth -= 1, _ => {} }
                q += 1;
            }
            if depth != 0 { return false; }
        }
        while q < bytes.len() {
            match bytes[q] {
                b' ' | b'\t' | b'\r' | b'\n' => q += 1,
                b'#' => { while q < bytes.len() && bytes[q] != b'\n' { q += 1; } }
                _ => break,
            }
        }
        self.input[q..].starts_with("<-") || self.input[q..].starts_with(UNICODE_LEFT_ARROW)
    }

    fn parse_prefix(&mut self) -> Result<Rc<dyn Ope>, String> {
        while self.consume_byte(b'~') { self.skip_spacing(); }
        let op = if self.consume_byte(b'&') { Some(true) }
                 else if self.consume_byte(b'!') { Some(false) }
                 else { None };
        self.skip_spacing();
        let suffix = self.parse_suffix()?;
        Ok(match op {
            Some(true) => Rc::new(AndPredicate { ope: suffix }),
            Some(false) => Rc::new(NotPredicate { ope: suffix }),
            None => suffix,
        })
    }

    fn parse_suffix(&mut self) -> Result<Rc<dyn Ope>, String> {
        let primary = self.parse_primary()?;
        let ope: Rc<dyn Ope> = if self.consume_byte(b'?') {
            Rc::new(Repetition::opt(primary))
        } else if self.consume_byte(b'*') {
            Rc::new(Repetition::zom(primary))
        } else if self.consume_byte(b'+') {
            Rc::new(Repetition::oom(primary))
        } else if self.consume_byte(b'{') {
            let (min, max) = self.parse_repetition_range()?;
            Rc::new(Repetition { ope: primary, min, max })
        } else {
            primary
        };
        if self.consume_byte(b'^') {
            let label_name = self.parse_identifier()?;
            let label_id = self.get_or_create_rule(&label_name);
            let recovery: Rc<dyn Ope> = Rc::new(Recovery {
                ope: Rc::new(Reference { name: label_name, rule_id: Some(label_id), is_macro: false, args: Vec::new(), iarg: 0 }),
            });
            return Ok(Rc::new(PrioritizedChoice { opes: vec![ope, recovery], for_label: true, first_sets: std::cell::UnsafeCell::new(Vec::new()) }));
        }
        Ok(ope)
    }

    fn parse_repetition_range(&mut self) -> Result<(usize, usize), String> {
        let read_num = |p: &mut Meta<'a>| -> Option<usize> {
            let start = p.pos;
            while let Some(b) = p.peek_byte() {
                if b.is_ascii_digit() { p.pos += 1; } else { break; }
            }
            if p.pos == start { None } else { p.input[start..p.pos].parse().ok() }
        };
        self.skip_spacing();
        let first = read_num(self);
        self.skip_spacing();
        let (min, max) = if self.consume_byte(b',') {
            self.skip_spacing();
            let second = read_num(self);
            self.skip_spacing();
            (first.unwrap_or(0), second.unwrap_or(usize::MAX))
        } else {
            let n = first.ok_or_else(|| self.err("expected number in '{...}'"))?;
            (n, n)
        };
        if !self.consume_byte(b'}') { return Err(self.err("expected '}'")); }
        let max = if max < min { min } else { max };
        Ok((min, max))
    }

    fn parse_primary(&mut self) -> Result<Rc<dyn Ope>, String> {
        self.skip_spacing();
        let b = self.peek_byte().ok_or_else(|| self.err("unexpected end of input"))?;

        if b == b'(' {
            self.pos += 1; self.skip_spacing();
            let e = self.parse_expression()?;
            self.skip_spacing();
            if !self.consume_byte(b')') { return Err(self.err("expected ')'")); }
            return Ok(e);
        }
        if b == b'<' && self.bytes().get(self.pos + 1) != Some(&b'-') {
            self.pos += 1; self.skip_spacing();
            let e = self.parse_expression()?;
            self.skip_spacing();
            if !self.consume_byte(b'>') { return Err(self.err("expected '>'")); }
            return Ok(Rc::new(TokenBoundary { ope: e }));
        }
        if b == b'\'' || b == b'"' { return Ok(self.parse_literal()?); }
        if b == b'[' { return Ok(self.parse_char_class()?); }
        if b == b'.' { self.pos += 1; return Ok(Rc::new(AnyCharacter)); }

        if self.input[self.pos..].starts_with("%recover") {
            self.pos += "%recover".len(); self.skip_spacing();
            if !self.consume_byte(b'(') { return Err(self.err("expected '(' after %recover")); }
            self.skip_spacing();
            let e = self.parse_expression()?;
            self.skip_spacing();
            if !self.consume_byte(b')') { return Err(self.err("expected ')'")); }
            return Ok(Rc::new(Recovery { ope: e }));
        }

        if self.consume_str(UNICODE_CUT) { return Ok(Rc::new(Cut)); }

        if b == b'$' {
            self.pos += 1;
            if self.consume_byte(b'(') {
                self.skip_spacing();
                let e = self.parse_expression()?;
                self.skip_spacing();
                if !self.consume_byte(b')') { return Err(self.err("expected ')' in $(...)")); }
                return Ok(Rc::new(CaptureScope { ope: e }));
            }
            let name = self.parse_identifier()?;
            self.skip_spacing();
            if self.peek_byte() == Some(b'<') && self.bytes().get(self.pos + 1) != Some(&b'-') {
                self.pos += 1; self.skip_spacing();
                let e = self.parse_expression()?;
                self.skip_spacing();
                if !self.consume_byte(b'>') { return Err(self.err("expected '>'")); }
                return Ok(Rc::new(Capture { ope: e, name }));
            }
            return Ok(Rc::new(BackReference { name }));
        }

        if is_ident_start(b) {
            let name = self.parse_identifier()?;
            // Macro parameter
            if let Some(iarg) = self.current_params.iter().position(|p| p == &name) {
                return Ok(Rc::new(Reference { name, rule_id: None, is_macro: false, args: Vec::new(), iarg }));
            }
            // Macro call: `(` immediately after
            if self.peek_byte() == Some(b'(') {
                self.pos += 1; self.skip_spacing();
                let mut args: Vec<Rc<dyn Ope>> = Vec::new();
                if self.peek_byte() != Some(b')') {
                    loop {
                        self.skip_spacing();
                        args.push(self.parse_expression()?);
                        self.skip_spacing();
                        if !self.consume_byte(b',') { break; }
                    }
                }
                self.skip_spacing();
                if !self.consume_byte(b')') { return Err(self.err("expected ')' in macro call")); }
                let id = self.get_or_create_rule(&name);
                return Ok(Rc::new(Reference { name, rule_id: Some(id), is_macro: true, args, iarg: 0 }));
            }
            let id = self.get_or_create_rule(&name);
            return Ok(Rc::new(Reference { name, rule_id: Some(id), is_macro: false, args: Vec::new(), iarg: 0 }));
        }

        Err(self.err("expected a primary expression"))
    }

    fn parse_literal(&mut self) -> Result<Rc<dyn Ope>, String> {
        let quote = self.peek_byte().unwrap();
        self.pos += 1;
        let mut out = String::new();
        while let Some(b) = self.peek_byte() {
            if b == quote {
                self.pos += 1;
                let ignore_case = self.consume_byte(b'i');
                return Ok(Rc::new(LiteralString::new(&out, ignore_case)));
            }
            if b == b'\\' {
                self.pos += 1;
                if let Some(ch) = self.parse_escape_char()? { out.push(ch); }
                continue;
            }
            if let Some(ch) = self.input.get(self.pos..).and_then(|s| s.chars().next()) {
                out.push(ch);
                self.pos += ch.len_utf8();
                continue;
            }
            break;
        }
        Err(self.err("unterminated string literal"))
    }

    fn parse_char_class(&mut self) -> Result<Rc<dyn Ope>, String> {
        self.pos += 1;
        let negated = self.consume_byte(b'^');
        let mut ranges: Vec<(char, char)> = Vec::new();
        while let Some(b) = self.peek_byte() {
            if b == b']' {
                self.pos += 1;
                let ignore_case = self.consume_byte(b'i');
                if ranges.is_empty() { return Err(self.err("empty character class")); }
                return Ok(Rc::new(CharacterClass::new(ranges, negated, ignore_case)));
            }
            let lo = self.parse_class_char()?;
            let hi = if self.peek_byte() == Some(b'-') && self.bytes().get(self.pos + 1) != Some(&b']') {
                self.pos += 1; self.parse_class_char()?
            } else { lo };
            ranges.push((lo, hi));
        }
        Err(self.err("unterminated character class"))
    }

    fn parse_escape_char(&mut self) -> Result<Option<char>, String> {
        let Some(e) = self.peek_byte() else { return Ok(None); };
        if e == b'x' || e == b'u' {
            self.pos += 1;
            let max = if e == b'x' { 2 } else { 4 };
            let start = self.pos;
            let mut end = start;
            while end - start < max && self.bytes().get(end).map_or(false, |b| b.is_ascii_hexdigit()) { end += 1; }
            if end == start { return Err(self.err("bad hex escape")); }
            let n = u32::from_str_radix(&self.input[start..end], 16).map_err(|_| self.err("bad hex"))?;
            self.pos = end;
            return Ok(char::from_u32(n));
        }
        if (b'0'..=b'7').contains(&e) {
            let start = self.pos;
            let mut end = start;
            while end - start < 3 && self.bytes().get(end).map_or(false, |b| (b'0'..=b'7').contains(b)) { end += 1; }
            let n = u32::from_str_radix(&self.input[start..end], 8).map_err(|_| self.err("bad octal"))?;
            self.pos = end;
            return Ok(char::from_u32(n));
        }
        self.pos += 1;
        Ok(Some(match e {
            b'n' => '\n', b'r' => '\r', b't' => '\t',
            b'\\' => '\\', b'\'' => '\'', b'"' => '"',
            b'[' => '[', b']' => ']', b'-' => '-',
            other => other as char,
        }))
    }

    fn parse_class_char(&mut self) -> Result<char, String> {
        if self.consume_byte(b'\\') {
            return self.parse_escape_char()?.ok_or_else(|| self.err("unterminated escape"));
        }
        let ch = self.input[self.pos..].chars().next().ok_or_else(|| self.err("unexpected end"))?;
        self.pos += ch.len_utf8();
        Ok(ch)
    }

    fn parse_identifier(&mut self) -> Result<String, String> {
        let start = self.pos;
        let Some(b) = self.peek_byte() else { return Err(self.err("expected identifier")); };
        if !is_ident_start(b) { return Err(self.err("expected identifier")); }
        self.pos += 1;
        while let Some(b) = self.peek_byte() {
            if is_ident_cont(b) { self.pos += 1; } else { break; }
        }
        Ok(self.input[start..self.pos].to_string())
    }

    fn skip_spacing(&mut self) {
        while let Some(b) = self.peek_byte() {
            if b == b' ' || b == b'\t' || b == b'\r' || b == b'\n' { self.pos += 1; }
            else if b == b'#' {
                while let Some(b) = self.peek_byte() { self.pos += 1; if b == b'\n' { break; } }
            } else { return; }
        }
    }

    fn peek_byte(&self) -> Option<u8> { self.bytes().get(self.pos).copied() }
    fn consume_byte(&mut self, expected: u8) -> bool {
        if self.peek_byte() == Some(expected) { self.pos += 1; true } else { false }
    }
    fn consume_str(&mut self, expected: &str) -> bool {
        if self.input[self.pos..].starts_with(expected) { self.pos += expected.len(); true } else { false }
    }

    fn consume_balanced_braces(&mut self) {
        if self.peek_byte() != Some(b'{') { return; }
        self.pos += 1;
        let mut depth = 1;
        while self.pos < self.input.len() && depth > 0 {
            match self.bytes()[self.pos] {
                b'{' => depth += 1, b'}' => depth -= 1,
                b'\'' | b'"' => {
                    let q = self.bytes()[self.pos]; self.pos += 1;
                    while self.pos < self.input.len() {
                        let c = self.bytes()[self.pos]; self.pos += 1;
                        if c == b'\\' && self.pos < self.input.len() { self.pos += 1; }
                        else if c == q { break; }
                    }
                    continue;
                }
                _ => {}
            }
            self.pos += 1;
        }
    }

    fn err(&self, msg: &str) -> String {
        let before = &self.input[..self.pos];
        let line = before.bytes().filter(|&b| b == b'\n').count() + 1;
        let col = before.rfind('\n').map_or(self.pos + 1, |n| self.pos - n);
        format!("{msg} (line {line}, col {col})")
    }
}

fn is_ident_start(b: u8) -> bool { b.is_ascii_alphabetic() || b == b'_' }
fn is_ident_cont(b: u8) -> bool { is_ident_start(b) || b.is_ascii_digit() }

fn parse_precedence_info(block: &str) -> std::collections::HashMap<String, (usize, char)> {
    let mut info = std::collections::HashMap::new();
    let start = match block.find("precedence") {
        Some(i) => i + "precedence".len(),
        None => return info,
    };
    let mut level = 1usize;
    for line in block[start..].lines() {
        let trimmed = line.trim();
        if trimmed.is_empty() { continue; }
        let mut chars = trimmed.chars();
        let assoc = match chars.next() {
            Some('L') | Some('l') => 'L',
            Some('R') | Some('r') => 'R',
            _ => continue,
        };
        for token in chars.as_str().split_whitespace() {
            info.insert(token.to_string(), (level, assoc));
        }
        level += 1;
    }
    info
}

fn validate_precedence_shape(body: &Rc<dyn Ope>) -> Result<(), String> {
    let err = || "'precedence' instruction cannot be applied to this rule".to_string();
    let seq = body.as_any().downcast_ref::<Sequence>().ok_or_else(err)?;
    if seq.opes.len() != 2 { return Err(err()); }
    let atom_name = ref_name(&seq.opes[0]).ok_or_else(err)?;
    let rep = seq.opes[1].as_any().downcast_ref::<Repetition>().ok_or_else(err)?;
    if rep.min != 0 || rep.max != usize::MAX { return Err(err()); }
    let inner = rep.ope.as_any().downcast_ref::<Sequence>().ok_or_else(err)?;
    if inner.opes.len() != 2 { return Err(err()); }
    let binop_name = ref_name(&inner.opes[0]).ok_or_else(err)?;
    let atom1_name = ref_name(&inner.opes[1]).ok_or_else(err)?;
    if atom_name != atom1_name || atom_name == binop_name { return Err(err()); }
    Ok(())
}

fn ref_name(ope: &Rc<dyn Ope>) -> Option<String> {
    if let Some(r) = ope.as_any().downcast_ref::<Reference>() { return Some(r.name.clone()); }
    None
}

fn extract_error_message(block: &str) -> Option<String> {
    let needle = "error_message";
    let idx = block.find(needle)?;
    let rest = &block[idx + needle.len()..];
    let rest = rest.trim_start();
    let quote = rest.as_bytes().first()?;
    if *quote != b'\'' && *quote != b'"' { return None; }
    let rest = &rest[1..];
    let end = rest.find(*quote as char)?;
    Some(rest[..end].to_string())
}
