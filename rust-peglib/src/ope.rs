use std::any::Any;
use std::rc::Rc;

pub const FAIL: usize = usize::MAX;

pub struct BitVec { data: Vec<u64> }
impl BitVec {
    pub fn new(len: usize) -> Self { Self { data: vec![0u64; (len + 63) / 64] } }
    #[inline] pub fn get(&self, i: usize) -> bool { (self.data[i >> 6] >> (i & 63)) & 1 != 0 }
    #[inline] pub fn set(&mut self, i: usize, v: bool) {
        if v { self.data[i >> 6] |= 1u64 << (i & 63); }
        else { self.data[i >> 6] &= !(1u64 << (i & 63)); }
    }
}

#[inline]
pub fn success(len: usize) -> bool { len != FAIL }
#[inline]
pub fn fail(len: usize) -> bool { len == FAIL }

pub trait Ope: AsAny {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize;
    fn accept(&self, v: &mut dyn Visitor);
}

pub trait AsAny: 'static {
    fn as_any(&self) -> &dyn Any;
}
impl<T: 'static + Ope> AsAny for T {
    fn as_any(&self) -> &dyn Any { self }
}

// ---- SemanticValues ----

pub struct SemanticValues {
    pub ss: *const str,
    pub sv: std::ops::Range<usize>,
    pub name: String,
    pub choice_count: usize,
    pub choice: usize,
    pub tokens: Vec<std::ops::Range<usize>>,
    pub tags: Vec<u32>,
    pub values: Vec<Box<dyn Any>>,
}

impl SemanticValues {
    pub fn new() -> Self {
        Self {
            ss: "" as *const str,
            sv: 0..0, name: String::new(),
            choice_count: 0, choice: 0,
            tokens: Vec::new(), tags: Vec::new(), values: Vec::new(),
        }
    }
    pub fn ss(&self) -> &str { unsafe { &*self.ss } }
    pub fn sv_str(&self) -> &str { &self.ss()[self.sv.clone()] }
    pub fn token(&self, id: usize) -> &str {
        if self.tokens.is_empty() { self.sv_str() }
        else { &self.ss()[self.tokens[id].clone()] }
    }
    pub fn token0(&self) -> &str { self.token(0) }
    pub fn token_to_number<T: std::str::FromStr>(&self) -> T where T::Err: std::fmt::Debug {
        self.token0().parse::<T>().unwrap()
    }
    pub fn try_token_to_number<T: std::str::FromStr>(&self) -> Result<T, T::Err> {
        self.token0().parse::<T>()
    }
    pub fn token_to_string(&self) -> String {
        self.token0().to_string()
    }
    pub fn line_info(&self) -> (usize, usize) {
        crate::line_col(self.ss(), self.sv.start)
    }
    pub fn val<T: Any>(&self, i: usize) -> &T {
        self.values[i].downcast_ref::<T>().unwrap()
    }
    pub fn get<T: Any + Copy>(&self, i: usize) -> T {
        *self.val::<T>(i)
    }
    pub fn transform<T: Any + Clone>(&self) -> Vec<T> {
        self.values.iter().map(|v| v.downcast_ref::<T>().unwrap().clone()).collect()
    }
    pub fn choice(&self) -> usize { self.choice }
    pub fn len(&self) -> usize { self.values.len() }
    pub fn clear(&mut self) {
        self.values.clear();
        self.tags.clear();
        self.tokens.clear();
        self.sv = 0..0;
        self.choice_count = 0;
        self.choice = 0;
    }
}

// ---- Context ----

pub struct Context {
    pub(crate) s: *const str,
    pub(crate) l: usize,
    pub(crate) rules: *const [crate::Definition],
    pub error_info: ErrorInfo,
    pub recovered: bool,
    pub(crate) value_stack: Vec<SemanticValues>,
    pub(crate) value_stack_size: usize,
    pub(crate) rule_stack: Vec<usize>,
    pub(crate) args_stack: Vec<Vec<Rc<dyn Ope>>>,
    pub(crate) in_token_boundary_count: usize,
    pub(crate) whitespace_ope: Option<Rc<dyn Ope>>,
    pub(crate) in_whitespace: bool,
    pub(crate) word_ope: Option<Rc<dyn Ope>>,
    pub(crate) capture_entries: Vec<(String, String)>,
    pub(crate) cut_stack: Vec<bool>,
    pub(crate) def_count: usize,
    pub(crate) enable_packrat: bool,
    pub(crate) cache_registered: BitVec,
    pub(crate) cache_success: BitVec,
    pub(crate) cache_values: std::collections::HashMap<u32, (u32, Option<Box<dyn Any>>)>,
    pub(crate) lr_memo: std::collections::HashMap<(usize, usize), (usize, Option<Box<dyn Any>>)>,
    pub(crate) lr_refs_hit: std::collections::HashSet<usize>,
    pub(crate) lr_active_seeds: std::collections::HashSet<(usize, usize)>,
    pub(crate) depth: usize,
    pub(crate) max_depth: usize,
    pub(crate) handlers: Option<*mut dyn crate::Handlers>,
    pub(crate) predicate_errors: Vec<(usize, String)>,
    pub(crate) enable_ast: bool,
    pub(crate) tracer: Option<*const crate::Tracer>,
    pub(crate) root_ast: Option<Box<dyn Any>>,
}

#[derive(Default)]
pub struct ErrorInfo {
    pub error_pos: usize,
    pub expected_tokens: Vec<(Option<*const str>, Option<usize>)>,
    pub keep_previous_token: bool,
    pub message_pos: usize,
    pub message: String,
    pub label: String,
}

impl Context {
    pub fn new(s: &str, rules: &[crate::Definition], def_count: usize, enable_packrat: bool) -> Self {
        let l = s.len();
        let (cr, cs) = if enable_packrat {
            let sz = def_count * (l + 1);
            (BitVec::new(sz), BitVec::new(sz))
        } else {
            (BitVec::new(0), BitVec::new(0))
        };
        Self {
            s: s as *const str, l,
            rules: rules as *const [crate::Definition],
            error_info: ErrorInfo::default(),
            recovered: false,
            value_stack: Vec::new(), value_stack_size: 0,
            rule_stack: Vec::new(),
            args_stack: Vec::new(),
            in_token_boundary_count: 0,
            whitespace_ope: None, in_whitespace: false,
            word_ope: None,
            capture_entries: Vec::new(),
            cut_stack: Vec::new(),
            def_count, enable_packrat,
            cache_registered: cr, cache_success: cs,
            cache_values: std::collections::HashMap::new(),
            lr_memo: std::collections::HashMap::new(),
            lr_refs_hit: std::collections::HashSet::new(),
            lr_active_seeds: std::collections::HashSet::new(),
            depth: 0, max_depth: 1024,
            handlers: None,
            predicate_errors: Vec::new(),
            enable_ast: false,
            tracer: None,
            root_ast: None,
        }
    }

    pub fn s(&self) -> &str { unsafe { &*self.s } }
    pub fn rules(&self) -> &[crate::Definition] { unsafe { &*self.rules } }

    pub fn push_semantic_values_scope(&mut self) -> usize {
        let idx = self.value_stack_size;
        if idx == self.value_stack.len() {
            self.value_stack.push(SemanticValues::new());
        } else {
            self.value_stack[idx].clear();
        }
        self.value_stack[idx].ss = self.s;
        self.value_stack_size += 1;
        idx
    }
    pub fn pop_semantic_values_scope(&mut self) { self.value_stack_size -= 1; }

    pub fn skip_whitespace(&mut self, pos: usize) -> usize {
        if self.in_token_boundary_count > 0 { return 0; }
        let Some(ws) = self.whitespace_ope.clone() else { return 0; };
        let mut dummy = SemanticValues::new();
        let len = ws.parse_core(pos, &mut dummy, self);
        if fail(len) { 0 } else { len }
    }

    #[inline]
    fn packrat_idx(&self, pos: usize, def_id: usize) -> usize {
        self.def_count * pos + def_id
    }

    pub fn packrat_len<F>(&mut self, pos: usize, def_id: usize, fn_: F) -> usize
    where F: FnOnce(&mut Context) -> usize
    {
        if !self.enable_packrat { return fn_(self); }
        let idx = self.packrat_idx(pos, def_id);
        if self.cache_registered.get(idx) {
            if self.cache_success.get(idx) {
                return self.cache_values[&(idx as u32)].0 as usize;
            }
            return FAIL;
        }
        self.cache_registered.set(idx, true);
        self.cache_success.set(idx, false);
        let len = fn_(self);
        if success(len) {
            self.cache_success.set(idx, true);
            self.cache_values.insert(idx as u32, (len as u32, None));
        }
        len
    }

    pub fn clear_packrat_cache(&mut self, pos: usize, def_id: usize) {
        if !self.enable_packrat { return; }
        let idx = self.packrat_idx(pos, def_id);
        self.cache_registered.set(idx, false);
        self.cache_success.set(idx, false);
        self.cache_values.remove(&(idx as u32));
    }

    pub fn write_packrat_cache(&mut self, pos: usize, def_id: usize, len: usize, val: Option<Box<dyn Any>>) {
        if !self.enable_packrat { return; }
        let idx = self.packrat_idx(pos, def_id);
        self.cache_registered.set(idx, true);
        self.cache_success.set(idx, true);
        self.cache_values.insert(idx as u32, (len as u32, val));
    }

    pub fn push_args(&mut self, args: Vec<Rc<dyn Ope>>) { self.args_stack.push(args); }
    pub fn pop_args(&mut self) { self.args_stack.pop(); }

    pub fn set_error_pos(&mut self, pos: usize, literal: Option<*const str>) {
        use std::cmp::Ordering;
        match pos.cmp(&self.error_info.error_pos) {
            Ordering::Less => return,
            Ordering::Greater => {
                self.error_info.error_pos = pos;
                self.error_info.expected_tokens.clear();
            }
            Ordering::Equal => {
                if !self.error_info.keep_previous_token {
                    self.error_info.expected_tokens.clear();
                }
            }
        }
        let rule = self.rule_stack.last().copied();
        self.error_info.expected_tokens.push((literal, rule));
    }
}

// ---- Visitor ----

pub trait Visitor {
    fn visit_sequence(&mut self, _ope: &Sequence) {}
    fn visit_prioritized_choice(&mut self, _ope: &PrioritizedChoice) {}
    fn visit_repetition(&mut self, _ope: &Repetition) {}
    fn visit_and_predicate(&mut self, _ope: &AndPredicate) {}
    fn visit_not_predicate(&mut self, _ope: &NotPredicate) {}
    fn visit_dictionary(&mut self, _ope: &Dictionary) {}
    fn visit_literal_string(&mut self, _ope: &LiteralString) {}
    fn visit_character_class(&mut self, _ope: &CharacterClass) {}
    fn visit_character(&mut self, _ope: &Character) {}
    fn visit_any_character(&mut self, _ope: &AnyCharacter) {}
    fn visit_capture_scope(&mut self, _ope: &CaptureScope) {}
    fn visit_capture(&mut self, _ope: &Capture) {}
    fn visit_token_boundary(&mut self, _ope: &TokenBoundary) {}
    fn visit_ignore(&mut self, _ope: &Ignore) {}
    #[allow(private_interfaces)] fn visit_weak_holder(&mut self, _ope: &WeakHolder) {}
    #[allow(private_interfaces)] fn visit_holder(&mut self, _ope: &Holder) {}
    #[allow(private_interfaces)] fn visit_reference(&mut self, _ope: &Reference) {}
    #[allow(private_interfaces)] fn visit_whitespace(&mut self, _ope: &Whitespace) {}
    fn visit_back_reference(&mut self, _ope: &BackReference) {}
    #[allow(private_interfaces)] fn visit_precedence_climbing(&mut self, _ope: &PrecedenceClimbing) {}
    #[allow(private_interfaces)] fn visit_recovery(&mut self, _ope: &Recovery) {}
    #[allow(private_interfaces)] fn visit_cut(&mut self, _ope: &Cut) {}
}

// ---- Ope struct definitions ----

pub struct Sequence { pub(crate) opes: Vec<Rc<dyn Ope>>, pub(crate) kw_guard: std::cell::UnsafeCell<Option<KeywordGuardData>> }

pub(crate) struct KeywordGuardData {
    pub(crate) identifier_first: [u64; 4],
    pub(crate) identifier_rest: [u64; 4],
    pub(crate) exact_keywords: Vec<String>,
    pub(crate) prefix_keywords: Vec<String>,
    pub(crate) min_keyword_len: usize,
    pub(crate) max_keyword_len: usize,
}

impl KeywordGuardData {
    fn test_first(&self, b: u8) -> bool { (self.identifier_first[(b >> 6) as usize] >> (b & 63)) & 1 != 0 }
    fn test_rest(&self, b: u8) -> bool { (self.identifier_rest[(b >> 6) as usize] >> (b & 63)) & 1 != 0 }
}
pub struct PrioritizedChoice {
    pub(crate) opes: Vec<Rc<dyn Ope>>,
    pub(crate) for_label: bool,
    pub(crate) first_sets: std::cell::UnsafeCell<Vec<FirstSet>>,
}
pub(crate) struct FirstSet {
    pub(crate) chars: [u64; 4],
    pub(crate) can_be_empty: bool,
    pub(crate) any_char: bool,
    pub(crate) first_literal: Option<*const str>,
    pub(crate) first_rule: Option<usize>,
}
impl Default for FirstSet { fn default() -> Self { Self { chars: [0;4], can_be_empty: false, any_char: false, first_literal: None, first_rule: None } } }
impl FirstSet {
    #[inline] pub(crate) fn set(&mut self, b: u8) { self.chars[(b >> 6) as usize] |= 1u64 << (b & 63); }
    #[inline] pub(crate) fn contains(&self, b: u8) -> bool { (self.chars[(b >> 6) as usize] >> (b & 63)) & 1 != 0 }
    pub(crate) fn merge(&mut self, o: &FirstSet) { for i in 0..4 { self.chars[i] |= o.chars[i]; } self.can_be_empty |= o.can_be_empty; self.any_char |= o.any_char; }
}
pub struct Repetition { pub(crate) ope: Rc<dyn Ope>, pub(crate) min: usize, pub(crate) max: usize }
pub struct AndPredicate { pub(crate) ope: Rc<dyn Ope> }
pub struct NotPredicate { pub(crate) ope: Rc<dyn Ope> }
pub struct Dictionary { pub(crate) trie: Trie }

pub struct Trie {
    dic: std::collections::BTreeMap<Vec<u8>, TrieInfo>,
    max_len: usize,
    items_count: usize,
    ignore_case: bool,
}

struct TrieInfo { done: bool, match_: bool, id: usize }

impl Trie {
    pub fn new(items: &[Vec<u8>], ignore_case: bool) -> Self {
        let mut dic = std::collections::BTreeMap::new();
        let mut max_len = 0;
        let items_count = items.len();
        for (id, item) in items.iter().enumerate() {
            let s: Vec<u8> = if ignore_case { item.iter().map(|b| b.to_ascii_lowercase()).collect() } else { item.clone() };
            if item.len() > max_len { max_len = item.len(); }
            for len in 1..=item.len() {
                let last = len == item.len();
                let key = s[..len].to_vec();
                let entry = dic.entry(key).or_insert(TrieInfo { done: last, match_: last, id });
                if last { entry.match_ = true; } else { entry.done = false; }
            }
        }
        Self { dic, max_len, items_count, ignore_case }
    }

    pub fn match_(&self, text: &[u8], text_len: usize) -> (usize, usize) {
        let limit = text_len.min(self.max_len);
        let lower: Vec<u8>;
        let bytes = if self.ignore_case {
            lower = text[..limit].iter().map(|b| b.to_ascii_lowercase()).collect();
            &lower
        } else {
            &text[..limit]
        };
        let mut match_len = 0;
        let mut match_id = 0;
        let mut len = 1;
        while len <= limit {
            let key = &bytes[..len];
            match self.dic.get(key) {
                Some(info) => {
                    if info.match_ { match_len = len; match_id = info.id; }
                    if info.done { break; }
                }
                None => break,
            }
            len += 1;
        }
        (match_len, match_id)
    }

    pub fn items_count(&self) -> usize { self.items_count }
}
pub struct LiteralString { pub(crate) lit: Vec<u8>, pub(crate) ignore_case: bool, pub(crate) is_word: std::cell::Cell<bool> }
pub struct CharacterClass {
    pub(crate) ranges: Vec<(char, char)>, pub(crate) negated: bool, pub(crate) ignore_case: bool,
    pub(crate) is_ascii_only: bool, pub(crate) ascii_bitset: [u64; 4],
}
pub struct Character { pub(crate) ch: char }
pub struct AnyCharacter;
pub struct CaptureScope { pub(crate) ope: Rc<dyn Ope> }
pub struct Capture { pub(crate) ope: Rc<dyn Ope>, pub(crate) name: String }
pub struct TokenBoundary { pub(crate) ope: Rc<dyn Ope> }
pub struct Ignore { pub(crate) ope: Rc<dyn Ope> }
pub(crate) struct WeakHolder { pub(crate) outer: usize }
pub(crate) struct Holder {
    pub(crate) ope: Option<Rc<dyn Ope>>, pub(crate) outer: usize,
    pub(crate) is_lr: bool, pub(crate) is_macro: bool,
    pub(crate) is_token: bool, pub(crate) ignore: bool,
}
pub(crate) struct Reference {
    pub(crate) name: String, pub(crate) rule_id: Option<usize>,
    pub(crate) is_macro: bool, pub(crate) args: Vec<Rc<dyn Ope>>, pub(crate) iarg: usize,
}
pub(crate) struct Whitespace { pub(crate) ope: Rc<dyn Ope> }
pub struct BackReference { pub(crate) name: String }
pub(crate) struct PrecedenceClimbing {
    pub(crate) atom: Rc<dyn Ope>, pub(crate) binop: Rc<dyn Ope>,
    pub(crate) info: std::collections::HashMap<String, (usize, char)>, pub(crate) rule_id: usize,
}
pub(crate) struct Recovery { pub(crate) ope: Rc<dyn Ope> }
pub struct User { pub fn_: Box<dyn Fn(usize, &mut SemanticValues, &mut Context) -> usize> }
pub(crate) struct Cut;

// ---- Constructors ----

impl LiteralString {
    pub fn new(s: &str, ignore_case: bool) -> Self {
        Self { lit: s.as_bytes().to_vec(), ignore_case, is_word: std::cell::Cell::new(false) }
    }
    pub(crate) fn lit_ptr(&self) -> *const str {
        // Safety: self.lit lives as long as the Grammar that owns this LiteralString
        unsafe { std::str::from_utf8_unchecked(&self.lit) as *const str }
    }
}

impl CharacterClass {
    pub fn new(ranges: Vec<(char, char)>, negated: bool, ignore_case: bool) -> Self {
        let mut cls = Self { ranges, negated, ignore_case, is_ascii_only: false, ascii_bitset: [0;4] };
        cls.setup_ascii_bitset();
        cls
    }
    fn setup_ascii_bitset(&mut self) {
        if self.negated { return; }
        for &(lo, hi) in &self.ranges {
            if (lo as u32) > 0x7F || (hi as u32) > 0x7F { return; }
        }
        self.is_ascii_only = true;
        let ranges = self.ranges.clone();
        for (lo, hi) in ranges {
            for cp in (lo as u32)..=(hi as u32) {
                let b = cp as u8;
                self.set_bit(b);
                if self.ignore_case {
                    self.set_bit(b.to_ascii_uppercase());
                    self.set_bit(b.to_ascii_lowercase());
                }
            }
        }
    }
    fn set_bit(&mut self, b: u8) { self.ascii_bitset[(b >> 6) as usize] |= 1u64 << (b & 63); }
    #[inline]
    pub fn ascii_test(&self, b: u8) -> bool { (self.ascii_bitset[(b >> 6) as usize] >> (b & 63)) & 1 != 0 }
    fn in_range_full(&self, c: char) -> bool {
        let hit = |x: char| self.ranges.iter().any(|(lo, hi)| x >= *lo && x <= *hi);
        let r = hit(c) || (self.ignore_case && (hit(c.to_ascii_lowercase()) || hit(c.to_ascii_uppercase())));
        r != self.negated
    }
}

impl Repetition {
    pub fn zom(ope: Rc<dyn Ope>) -> Self { Self { ope, min: 0, max: usize::MAX } }
    pub fn oom(ope: Rc<dyn Ope>) -> Self { Self { ope, min: 1, max: usize::MAX } }
    pub fn opt(ope: Rc<dyn Ope>) -> Self { Self { ope, min: 0, max: 1 } }
}

impl PrioritizedChoice {
    pub fn new(opes: Vec<Rc<dyn Ope>>) -> Self {
        Self { opes, for_label: false, first_sets: std::cell::UnsafeCell::new(Vec::new()) }
    }
}

// ---- parse_core implementations ----

impl Ope for Sequence {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        if let Some(ref kw) = unsafe { &*self.kw_guard.get() } {
            match parse_keyword_guarded(kw, pos, vs, ctx) {
                Some(len) => return len,
                None => {} // prefix match — fall through to normal path
            }
        }
        let sv_mark = if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.len() } else { 0 };
        let mut i = 0;
        for ope in &self.opes {
            let len = ope.parse_core(pos + i, vs, ctx);
            if fail(len) {
                if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.truncate(sv_mark); }
                return FAIL;
            }
            i += len;
        }
        i
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_sequence(self); }
}

impl Ope for PrioritizedChoice {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        if !self.for_label { ctx.cut_stack.push(false); }
        let first_sets = unsafe { &*self.first_sets.get() };
        let have_fs = first_sets.len() == self.opes.len();
        let next_byte = ctx.s().as_bytes().get(pos).copied();
        let saved_keep = ctx.error_info.keep_previous_token;
        let sv_mark = if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.len() } else { 0 };
        let mut id = 0;
        for ope in &self.opes {
            ctx.error_info.keep_previous_token = id > 0;
            if have_fs {
                let fs = &first_sets[id];
                if !fs.any_char && !fs.can_be_empty {
                    match next_byte {
                        Some(b) if fs.contains(b) => {}
                        _ => {
                            if fs.first_literal.is_some() || fs.first_rule.is_some() {
                                ctx.set_error_pos(pos, fs.first_literal);
                            }
                            id += 1; continue;
                        }
                    }
                }
            }
            ctx.error_info.keep_previous_token = id > 0;
            let save_values_len = vs.values.len();
            let save_tags_len = vs.tags.len();
            let save_tokens_len = vs.tokens.len();
            let save_sv = vs.sv.clone();
            let save_capture_len = ctx.capture_entries.len();
            let len = ope.parse_core(pos, vs, ctx);
            if success(len) {
                vs.choice_count = self.opes.len();
                vs.choice = id;
                if ctx.value_stack_size > 0 {
                    let sv = &mut ctx.value_stack[ctx.value_stack_size - 1];
                    sv.choice_count = self.opes.len();
                    sv.choice = id;
                }
                if !self.for_label { ctx.cut_stack.pop(); }
                ctx.error_info.keep_previous_token = false;
                return len;
            }
            vs.values.truncate(save_values_len);
            vs.tags.truncate(save_tags_len);
            vs.tokens.truncate(save_tokens_len);
            vs.sv = save_sv;
            ctx.capture_entries.truncate(save_capture_len);
            if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.truncate(sv_mark); }
            if !ctx.cut_stack.is_empty() && *ctx.cut_stack.last().unwrap() { break; }
            id += 1;
        }
        if !self.for_label { ctx.cut_stack.pop(); }
        ctx.error_info.keep_previous_token = saved_keep;
        FAIL
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_prioritized_choice(self); }
}

impl Ope for Repetition {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        // ispan fast path
        if let Some(cls) = as_character_class(&*self.ope) {
            if cls.is_ascii_only {
                let bytes = ctx.s().as_bytes();
                let limit = (pos.saturating_add(self.max)).min(bytes.len());
                let mut i = pos;
                while i < limit {
                    let b = bytes[i];
                    if b >= 0x80 || !cls.ascii_test(b) { break; }
                    i += 1;
                }
                let consumed = i - pos;
                if consumed < self.min {
                    ctx.set_error_pos(i, None);
                    return FAIL;
                }
                return consumed;
            }
        }
        let mut count = 0;
        let mut i = 0;
        while count < self.min {
            let len = self.ope.parse_core(pos + i, vs, ctx);
            if fail(len) { return FAIL; }
            i += len;
            count += 1;
        }
        while count < self.max {
            let save_vl = vs.values.len();
            let save_tl = vs.tags.len();
            let save_tkl = vs.tokens.len();
            let save_sv = vs.sv.clone();
            let save_cl = ctx.capture_entries.len();
            let len = self.ope.parse_core(pos + i, vs, ctx);
            if fail(len) {
                vs.values.truncate(save_vl);
                vs.tags.truncate(save_tl);
                vs.tokens.truncate(save_tkl);
                vs.sv = save_sv;
                ctx.capture_entries.truncate(save_cl);
                break;
            }
            if len == 0 { break; }
            i += len;
            count += 1;
        }
        i
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_repetition(self); }
}

fn parse_keyword_guarded(kw: &KeywordGuardData, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> Option<usize> {
    let bytes = ctx.s().as_bytes();
    let n = bytes.len() - pos;
    if n < 1 || !kw.test_first(bytes[pos]) {
        ctx.set_error_pos(pos, None);
        return Some(FAIL);
    }
    let mut id_len = 1;
    while id_len < n && kw.test_rest(bytes[pos + id_len]) { id_len += 1; }
    if id_len >= kw.min_keyword_len && id_len <= kw.max_keyword_len {
        let lower: String = bytes[pos..pos + id_len].iter().map(|&b| (b as char).to_ascii_lowercase()).collect();
        if kw.exact_keywords.iter().any(|k| k == &lower) {
            ctx.set_error_pos(pos, None);
            return Some(FAIL);
        }
        if kw.prefix_keywords.iter().any(|k| k == &lower) {
            return None; // fall through to normal path
        }
    }
    vs.tokens.push(pos..pos + id_len);
    if ctx.value_stack_size > 0 {
        ctx.value_stack[ctx.value_stack_size - 1].tokens.push(pos..pos + id_len);
    }
    let wl = ctx.skip_whitespace(pos + id_len);
    Some(id_len + wl)
}

fn resolve_macro_arg(ope: &Rc<dyn Ope>, args_stack: &[Vec<Rc<dyn Ope>>]) -> Rc<dyn Ope> {
    let any = ope.as_any();
    if let Some(r) = any.downcast_ref::<Reference>() {
        if r.rule_id.is_none() {
            if let Some(frame) = args_stack.last() {
                if r.iarg < frame.len() {
                    return frame[r.iarg].clone();
                }
            }
        }
        return ope.clone();
    }
    if let Some(seq) = any.downcast_ref::<Sequence>() {
        let opes: Vec<Rc<dyn Ope>> = seq.opes.iter().map(|o| resolve_macro_arg(o, args_stack)).collect();
        return Rc::new(Sequence { opes, kw_guard: std::cell::UnsafeCell::new(None) });
    }
    if let Some(ch) = any.downcast_ref::<PrioritizedChoice>() {
        let opes: Vec<Rc<dyn Ope>> = ch.opes.iter().map(|o| resolve_macro_arg(o, args_stack)).collect();
        return Rc::new(PrioritizedChoice { opes, for_label: ch.for_label, first_sets: std::cell::UnsafeCell::new(Vec::new()) });
    }
    if let Some(rep) = any.downcast_ref::<Repetition>() {
        return Rc::new(Repetition { ope: resolve_macro_arg(&rep.ope, args_stack), min: rep.min, max: rep.max });
    }
    if let Some(p) = any.downcast_ref::<AndPredicate>() {
        return Rc::new(AndPredicate { ope: resolve_macro_arg(&p.ope, args_stack) });
    }
    if let Some(p) = any.downcast_ref::<NotPredicate>() {
        return Rc::new(NotPredicate { ope: resolve_macro_arg(&p.ope, args_stack) });
    }
    if let Some(tb) = any.downcast_ref::<TokenBoundary>() {
        return Rc::new(TokenBoundary { ope: resolve_macro_arg(&tb.ope, args_stack) });
    }
    ope.clone()
}

fn as_character_class(ope: &dyn Ope) -> Option<&CharacterClass> {
    ope.as_any().downcast_ref::<CharacterClass>()
}

impl Ope for AndPredicate {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let save_vl = vs.values.len();
        let save_tl = vs.tags.len();
        let save_tkl = vs.tokens.len();
        let save_sv = vs.sv.clone();
        let save_cl = ctx.capture_entries.len();
        let save_sv_vl = if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.len() } else { 0 };
        let len = self.ope.parse_core(pos, vs, ctx);
        vs.values.truncate(save_vl);
        vs.tags.truncate(save_tl);
        vs.tokens.truncate(save_tkl);
        vs.sv = save_sv;
        ctx.capture_entries.truncate(save_cl);
        if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.truncate(save_sv_vl); }
        if success(len) { 0 } else { FAIL }
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_and_predicate(self); }
}

impl Ope for NotPredicate {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let save_vl = vs.values.len();
        let save_tl = vs.tags.len();
        let save_tkl = vs.tokens.len();
        let save_sv = vs.sv.clone();
        let save_cl = ctx.capture_entries.len();
        let save_sv_vl = if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.len() } else { 0 };
        let len = self.ope.parse_core(pos, vs, ctx);
        vs.values.truncate(save_vl);
        vs.tags.truncate(save_tl);
        vs.tokens.truncate(save_tkl);
        vs.sv = save_sv;
        ctx.capture_entries.truncate(save_cl);
        if ctx.value_stack_size > 0 { ctx.value_stack[ctx.value_stack_size - 1].values.truncate(save_sv_vl); }
        if success(len) { FAIL } else { 0 }
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_not_predicate(self); }
}

impl Ope for LiteralString {
    fn parse_core(&self, pos: usize, _vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let input = ctx.s().as_bytes();
        let end = pos + self.lit.len();
        if end > input.len() {
            ctx.set_error_pos(pos, Some(self.lit_ptr()));
            return FAIL;
        }
        if self.ignore_case {
            for (i, &c) in self.lit.iter().enumerate() {
                if !input[pos + i].eq_ignore_ascii_case(&c) {
                    ctx.set_error_pos(pos, Some(self.lit_ptr()));
                    return FAIL;
                }
            }
        } else if &input[pos..end] != self.lit.as_slice() {
            ctx.set_error_pos(pos, Some(self.lit_ptr()));
            return FAIL;
        }
        // Word check
        if self.is_word.get() {
            if let Some(word) = ctx.word_ope.clone() {
                let mut dummy = SemanticValues::new();
                dummy.ss = ctx.s;
                let wlen = word.parse_core(end, &mut dummy, ctx);
                if success(wlen) && wlen > 0 {
                    ctx.set_error_pos(end, None);
                    return FAIL;
                }
            }
        }
        let wl = ctx.skip_whitespace(end);
        self.lit.len() + wl
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_literal_string(self); }
}

impl Ope for CharacterClass {
    fn parse_core(&self, pos: usize, _vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let bytes = ctx.s().as_bytes();
        if pos >= bytes.len() { ctx.set_error_pos(pos, None); return FAIL; }
        if self.is_ascii_only {
            let b = bytes[pos];
            if b < 0x80 && self.ascii_test(b) { return 1; }
            ctx.set_error_pos(pos, None);
            return FAIL;
        }
        let rest = &ctx.s()[pos..];
        match rest.chars().next() {
            Some(c) if self.in_range_full(c) => c.len_utf8(),
            _ => { ctx.set_error_pos(pos, None); FAIL }
        }
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_character_class(self); }
}

impl Ope for Character {
    fn parse_core(&self, pos: usize, _vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        match ctx.s().get(pos..).and_then(|s| s.chars().next()) {
            Some(c) if c == self.ch => c.len_utf8(),
            _ => { ctx.set_error_pos(pos, None); FAIL }
        }
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_character(self); }
}

impl Ope for AnyCharacter {
    fn parse_core(&self, pos: usize, _vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        match ctx.s().get(pos..).and_then(|s| s.chars().next()) {
            Some(c) => c.len_utf8(),
            None => { ctx.set_error_pos(pos, None); FAIL }
        }
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_any_character(self); }
}

impl Ope for TokenBoundary {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        ctx.in_token_boundary_count += 1;
        let len = self.ope.parse_core(pos, vs, ctx);
        ctx.in_token_boundary_count -= 1;
        if fail(len) { return FAIL; }
        let range = pos..pos + len;
        vs.tokens.push(range.clone());
        // Also push into the current SV scope for AST token collection
        if ctx.value_stack_size > 0 {
            ctx.value_stack[ctx.value_stack_size - 1].tokens.push(range);
        }
        let wl = ctx.skip_whitespace(pos + len);
        len + wl
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_token_boundary(self); }
}

impl Ope for CaptureScope {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let mark = ctx.capture_entries.len();
        let len = self.ope.parse_core(pos, vs, ctx);
        ctx.capture_entries.truncate(mark);
        len
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_capture_scope(self); }
}

impl Ope for Capture {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let len = self.ope.parse_core(pos, vs, ctx);
        if success(len) {
            ctx.capture_entries.push((self.name.clone(), ctx.s()[pos..pos + len].to_string()));
        }
        len
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_capture(self); }
}

impl Ope for Ignore {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        self.ope.parse_core(pos, vs, ctx)
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_ignore(self); }
}

impl Ope for WeakHolder {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let holder = unsafe { &(&*ctx.rules)[self.outer].holder }.clone();
        holder.parse_core(pos, vs, ctx)
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_weak_holder(self); }
}

struct HolderInfo<'a> {
    is_macro: bool,
    is_token: bool,
    ignore: bool,
    rule_name: &'a str,
    has_error_msg: bool,
    error_msg: &'a str,
}

fn do_parse<'a>(def_id: usize, pos: usize, body: &dyn Ope, info: &HolderInfo<'a>,
            vs: &mut SemanticValues, ctx: &mut Context) -> (usize, Option<Box<dyn Any>>) {
    let has_action = unsafe {
        let rules = &*ctx.rules;
        def_id < rules.len() && rules[def_id].action.is_some() && !rules[def_id].disable_action
    };
    let any_action = ctx.value_stack_size > 0 || has_action;
    let need_scope = !info.is_macro && (ctx.enable_ast || any_action);
    let sv_idx = if need_scope { Some(ctx.push_semantic_values_scope()) } else { None };
    ctx.rule_stack.push(def_id);
    let mut len = body.parse_core(pos, vs, ctx);
    ctx.rule_stack.pop();

    if info.has_error_msg && (pos > ctx.error_info.message_pos || ctx.error_info.message_pos == 0) {
        ctx.error_info.message_pos = pos;
        ctx.error_info.message = info.error_msg.to_string();
        ctx.error_info.label = info.rule_name.to_string();
    }

    if success(len) {
        if let Some(h) = ctx.handlers {
            let sv = &ctx.s()[pos..pos + len];
            let tok_strs: Vec<&str> = vs.tokens.iter().map(|r| &ctx.s()[r.clone()]).collect();
            let result = unsafe { (*h).predicate(info.rule_name, sv, &tok_strs) };
            if let Err(msg) = result {
                ctx.predicate_errors.push((pos, msg));
                len = FAIL;
            }
        }
    }

    let val = if let Some(idx) = sv_idx {
        if success(len) && !ctx.recovered && !info.ignore {
            ctx.value_stack[idx].sv = pos..pos + len;
            ctx.value_stack[idx].name = info.rule_name.to_string();

            // reduce: action > AST > first child
            let result = if has_action {
                let sv_ref = &mut ctx.value_stack[idx];
                let mut dummy_dt: Box<dyn Any> = Box::new(());
                let action = unsafe { (&*ctx.rules)[def_id].action.as_ref().unwrap() };
                Some(action(sv_ref, &mut *dummy_dt))
            } else if ctx.enable_ast {
                let input = unsafe { &*ctx.s };
                let sv_ref = &mut ctx.value_stack[idx];
                let sv_str: &str = &input[sv_ref.sv.clone()];
                let tokens: Vec<&str> = sv_ref.tokens.iter().map(|r| &input[r.clone()]).collect();
                let nodes: Vec<crate::Ast> = sv_ref.values.drain(..)
                    .filter_map(|v| unsafe {
                        std::mem::transmute::<Option<Box<crate::Ast<'static>>>, Option<Box<crate::Ast<'_>>>>(
                            v.downcast::<crate::Ast<'static>>().ok()
                        ).map(|b| *b)
                    }).collect();
                let tag = crate::str2tag(&sv_ref.name);
                let ast_node = crate::Ast {
                    name: sv_ref.name.clone(), original_name: sv_ref.name.clone(),
                    sv: sv_str, tokens, nodes: if info.is_token { Vec::new() } else { nodes },
                    parent: std::ptr::null(),
                    is_token: info.is_token,
                    choice: sv_ref.choice, original_choice: sv_ref.choice,
                    choice_count: sv_ref.choice_count, original_choice_count: sv_ref.choice_count,
                    tag, original_tag: tag,
                };
                let boxed: Box<dyn Any> = unsafe {
                    let sa: Box<crate::Ast<'static>> = std::mem::transmute(Box::new(ast_node));
                    sa as Box<dyn Any>
                };
                Some(boxed)
            } else if !ctx.value_stack[idx].values.is_empty() {
                Some(ctx.value_stack[idx].values.remove(0))
            } else {
                None
            };
            ctx.pop_semantic_values_scope();
            result
        } else {
            ctx.pop_semantic_values_scope();
            None
        }
    } else {
        None
    };
    (len, val)
}

impl Ope for Holder {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let Some(ref body) = self.ope else { return FAIL; };
        if ctx.depth >= ctx.max_depth { return FAIL; }
        ctx.depth += 1;
        let def_id = self.outer;
        // rule_name and error_msg still need pointer lookup (strings on Definition)
        let (rule_name, has_error_msg, error_msg) = unsafe {
            let rules = &*ctx.rules;
            if def_id < rules.len() {
                let d = &rules[def_id];
                (std::mem::transmute::<&str, &'static str>(&d.name),
                 !d.error_message.is_empty(),
                 std::mem::transmute::<&str, &'static str>(&d.error_message))
            } else { ("", false, "") }
        };
        let info = HolderInfo {
            is_macro: self.is_macro, is_token: self.is_token,
            ignore: self.ignore, rule_name, has_error_msg, error_msg,
        };

        if let Some(h) = ctx.handlers {
            unsafe { (*h).on_enter(rule_name); }
        }
        if let Some(t) = ctx.tracer {
            if let Some(ref enter) = unsafe { &*t }.enter {
                enter(rule_name, pos);
            }
        }

        let tok_mark = vs.tokens.len();
        let (len, ast) = if self.is_lr {
            parse_left_recursive(def_id, pos, body.as_ref(), &info, vs, ctx)
        } else if ctx.enable_packrat && !self.is_macro {
            let idx = ctx.packrat_idx(pos, def_id);
            if ctx.cache_registered.get(idx) {
                if ctx.cache_success.get(idx) {
                    let (cached_len, ref cached_val) = ctx.cache_values[&(idx as u32)];
                    let val = cached_val.as_ref().map(|v| clone_any_box(v));
                    (cached_len as usize, val)
                } else {
                    (FAIL, None)
                }
            } else {
                ctx.cache_registered.set(idx, true);
                ctx.cache_success.set(idx, false);
                let (result, val) = do_parse(def_id, pos, body.as_ref(), &info, vs, ctx);
                if success(result) {
                    ctx.cache_success.set(idx, true);
                    let cached_val = val.as_ref().map(|v| clone_any_box(v));
                    ctx.cache_values.insert(idx as u32, (result as u32, cached_val));
                }
                (result, val)
            }
        } else {
            do_parse(def_id, pos, body.as_ref(), &info, vs, ctx)
        };

        if let Some(boxed) = ast {
            if ctx.value_stack_size > 0 {
                ctx.value_stack[ctx.value_stack_size - 1].values.push(boxed);
            } else {
                ctx.root_ast = Some(boxed);
            }
        }

        vs.tokens.truncate(tok_mark);

        if let Some(h) = ctx.handlers {
            unsafe { (*h).on_leave(rule_name, success(len)); }
        }
        if let Some(t) = ctx.tracer {
            if let Some(ref leave) = unsafe { &*t }.leave {
                leave(rule_name, pos, success(len));
            }
        }

        ctx.depth -= 1;
        len
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_holder(self); }
}

fn parse_left_recursive<'a>(id: usize, pos: usize, body: &dyn Ope, info: &HolderInfo<'a>,
                        vs: &mut SemanticValues, ctx: &mut Context) -> (usize, Option<Box<dyn Any>>) {
    let lr_key = (id, pos);
    if let Some((memo_len, memo_ast)) = ctx.lr_memo.get(&lr_key) {
        ctx.lr_refs_hit.insert(id);
        let len = *memo_len;
        let ast = memo_ast.as_ref().map(|b| clone_any_box(b));
        return (len, ast);
    }
    ctx.lr_memo.insert(lr_key, (FAIL, None));
    ctx.lr_active_seeds.insert(lr_key);
    let saved_refs = std::mem::take(&mut ctx.lr_refs_hit);

    let (initial_len, initial_ast) = do_parse(id, pos, body, info, vs, ctx);

    let mut cycle_rules = std::mem::take(&mut ctx.lr_refs_hit);
    if !cycle_rules.is_empty() { cycle_rules.insert(id); }
    ctx.lr_refs_hit = saved_refs;
    for r in &cycle_rules { ctx.lr_refs_hit.insert(*r); }

    let mut len = initial_len;
    let mut ast = initial_ast;
    if success(len) {
        ctx.lr_memo.insert(lr_key, (len, ast.as_ref().map(|b| clone_any_box(b))));
        loop {
            ctx.clear_packrat_cache(pos, id);
            let stale: Vec<_> = ctx.lr_memo.keys()
                .filter(|(rid, p)| *p == pos && *rid != id && cycle_rules.contains(rid) && !ctx.lr_active_seeds.contains(&(*rid, *p)))
                .copied().collect();
            for k in stale { ctx.lr_memo.remove(&k); }

            let (new_len, new_ast) = do_parse(id, pos, body, info, vs, ctx);
            if !success(new_len) || new_len <= len { break; }
            len = new_len;
            ast = new_ast;
            ctx.lr_memo.insert(lr_key, (len, ast.as_ref().map(|b| clone_any_box(b))));
        }
    }
    ctx.lr_active_seeds.remove(&lr_key);
    if success(len) {
        let val = ast.as_ref().map(|v| clone_any_box(v));
        ctx.write_packrat_cache(pos, id, len, val);
    }
    (len, ast)
}

fn clone_any_box(v: &Box<dyn Any>) -> Box<dyn Any> {
    if let Some(ast) = v.downcast_ref::<crate::Ast<'static>>() {
        let a: Box<crate::Ast<'static>> = Box::new(ast.clone());
        a as Box<dyn Any>
    } else if let Some(&n) = v.downcast_ref::<i32>() { Box::new(n) }
    else if let Some(&n) = v.downcast_ref::<i64>() { Box::new(n) }
    else if let Some(&n) = v.downcast_ref::<f64>() { Box::new(n) }
    else if let Some(s) = v.downcast_ref::<String>() { Box::new(s.clone()) }
    else if let Some(&b) = v.downcast_ref::<bool>() { Box::new(b) }
    else { Box::new(()) }
}

impl Ope for Reference {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        if self.rule_id.is_none() {
            // Macro parameter reference
            let args = ctx.args_stack.last().map(|a| a.clone()).unwrap_or_default();
            if self.iarg < args.len() {
                return args[self.iarg].parse_core(pos, vs, ctx);
            }
            return FAIL;
        }
        let rule_id = self.rule_id.unwrap();
        let holder = unsafe {
            let rules = &*ctx.rules;
            if rule_id >= rules.len() { return FAIL; }
            rules[rule_id].holder.clone()
        };
        if self.is_macro {
            // Resolve param refs in args against the current (outer) frame
            let resolved: Vec<Rc<dyn Ope>> = self.args.iter().map(|a| {
                resolve_macro_arg(a, &ctx.args_stack)
            }).collect();
            ctx.push_args(resolved);
            let len = holder.parse_core(pos, vs, ctx);
            ctx.pop_args();
            len
        } else {
            holder.parse_core(pos, vs, ctx)
        }
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_reference(self); }
}

impl Ope for Whitespace {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        if ctx.in_whitespace { return 0; }
        ctx.in_whitespace = true;
        let len = self.ope.parse_core(pos, vs, ctx);
        ctx.in_whitespace = false;
        len
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_whitespace(self); }
}

impl Ope for BackReference {
    fn parse_core(&self, pos: usize, _vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let captured = match ctx.capture_entries.iter().rev().find(|(n, _)| n == &self.name) {
            Some((_, s)) => s.clone(),
            None => { ctx.set_error_pos(pos, None); return FAIL; }
        };
        let bytes = ctx.s().as_bytes();
        let end = pos + captured.len();
        if end > bytes.len() || &bytes[pos..end] != captured.as_bytes() {
            ctx.set_error_pos(pos, None);
            return FAIL;
        }
        captured.len()
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_back_reference(self); }
}

impl Ope for Dictionary {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let input = ctx.s().as_bytes();
        let (match_len, match_id) = self.trie.match_(&input[pos..], input.len() - pos);
        if match_len == 0 {
            ctx.set_error_pos(pos, None);
            return FAIL;
        }
        // Word boundary check
        if let Some(word) = ctx.word_ope.clone() {
            let mut dummy = SemanticValues::new();
            let not_ope = NotPredicate { ope: word };
            let wlen = not_ope.parse_core(pos + match_len, &mut dummy, ctx);
            if fail(wlen) {
                ctx.set_error_pos(pos, None);
                return FAIL;
            }
        }
        vs.choice_count = self.trie.items_count();
        vs.choice = match_id;
        if ctx.value_stack_size > 0 {
            let sv = &mut ctx.value_stack[ctx.value_stack_size - 1];
            sv.choice_count = self.trie.items_count();
            sv.choice = match_id;
        }
        let wl = ctx.skip_whitespace(pos + match_len);
        match_len + wl
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_dictionary(self); }
}

impl Ope for Recovery {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        let len = self.ope.parse_core(pos, vs, ctx);
        if success(len) {
            ctx.recovered = true;
            // Extract error_message from inner Reference target
            if let Some(r) = self.ope.as_any().downcast_ref::<Reference>() {
                if let Some(id) = r.rule_id {
                    let msg = unsafe { &(&*ctx.rules)[id].error_message };
                    if !msg.is_empty() {
                        ctx.predicate_errors.push((pos, msg.clone()));
                    }
                }
            }
            if !ctx.cut_stack.is_empty() { *ctx.cut_stack.last_mut().unwrap() = true; }
        }
        len
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_recovery(self); }
}

impl Ope for PrecedenceClimbing {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        // PrecedenceClimbing replaces the Holder body but the Holder already
        // registered a packrat entry. Clear it so recursive calls to the same
        // rule via atom (e.g. parenthesized sub-expressions) work correctly.
        ctx.clear_packrat_cache(pos, self.rule_id);
        self.parse_expression(pos, vs, ctx, 0)
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_precedence_climbing(self); }
}

impl PrecedenceClimbing {
    fn parse_expression(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context, min_prec: usize) -> usize {
        let len = self.atom.parse_core(pos, vs, ctx);
        if fail(len) { return FAIL; }
        let mut i = len;
        while pos + i < ctx.l {
            let mut binop_vs = SemanticValues::new();
            binop_vs.ss = ctx.s;
            let blen = self.binop.parse_core(pos + i, &mut binop_vs, ctx);
            if fail(blen) { break; }
            let tok: &str = if !binop_vs.tokens.is_empty() {
                &ctx.s()[binop_vs.tokens[0].clone()]
            } else {
                ctx.s()[pos + i..pos + i + blen].trim()
            };
            let lookup = self.info.get(tok.trim());
            let (level, assoc) = match lookup {
                Some(&(l, a)) => (l, a),
                None => break,
            };
            if level < min_prec { break; }

            // Push binop value into SV
            if ctx.value_stack_size > 0 {
                if !binop_vs.values.is_empty() {
                    ctx.value_stack[ctx.value_stack_size - 1].values.push(binop_vs.values.remove(0));
                }
            }

            i += blen;
            let next_min = if assoc == 'L' { level + 1 } else { level };
            let rlen = self.parse_expression(pos + i, vs, ctx, next_min);
            if fail(rlen) { return FAIL; }
            i += rlen;

            // Reduce: call rule action or merge AST nodes
            if ctx.value_stack_size > 0 {
                let sv = &mut ctx.value_stack[ctx.value_stack_size - 1];
                let has_action = unsafe {
                    let rules = &*ctx.rules;
                    self.rule_id < rules.len() && rules[self.rule_id].action.is_some()
                };
                if has_action {
                    sv.sv = pos..pos + i;
                    let mut dummy_dt: Box<dyn Any> = Box::new(());
                    let action = unsafe { (&*ctx.rules)[self.rule_id].action.as_ref().unwrap() };
                    let val = action(sv, &mut *dummy_dt);
                    sv.values.clear();
                    sv.values.push(val);
                } else if sv.values.len() >= 2 {
                    // Merge last two AST children into one parent node
                    let right = sv.values.pop().unwrap();
                    let left = sv.values.pop().unwrap();
                    let input = unsafe { &*ctx.s };
                    let rule_name = unsafe { (&*ctx.rules).get(self.rule_id).map(|r| r.name.clone()).unwrap_or_default() };
                    if let (Some(l), Some(r)) = (
                        left.downcast_ref::<crate::Ast<'static>>(),
                        right.downcast_ref::<crate::Ast<'static>>(),
                    ) {
                        let tag = crate::str2tag(&rule_name);
                        let merged = crate::Ast {
                            name: rule_name.clone(), original_name: rule_name,
                            sv: &input[pos..pos + i], tokens: Vec::new(),
                            nodes: vec![l.clone(), r.clone()], parent: std::ptr::null(),
                            is_token: false,
                            choice: 0, original_choice: 0,
                            choice_count: 0, original_choice_count: 0,
                            tag, original_tag: tag,
                        };
                        let boxed: Box<dyn Any> = unsafe {
                            let sa: Box<crate::Ast<'static>> = std::mem::transmute(Box::new(merged));
                            sa as Box<dyn Any>
                        };
                        sv.values.push(boxed);
                    } else {
                        sv.values.push(left);
                    }
                }
            }
        }
        i
    }
}

impl Ope for User {
    fn parse_core(&self, pos: usize, vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        (self.fn_)(pos, vs, ctx)
    }
    fn accept(&self, _v: &mut dyn Visitor) {}
}

impl Ope for Cut {
    fn parse_core(&self, _pos: usize, _vs: &mut SemanticValues, ctx: &mut Context) -> usize {
        if !ctx.cut_stack.is_empty() { *ctx.cut_stack.last_mut().unwrap() = true; }
        0
    }
    fn accept(&self, v: &mut dyn Visitor) { v.visit_cut(self); }
}
