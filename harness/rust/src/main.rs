//! Rust harness for the language-independent PEG spec tests. Walks
//! `spec/tests/**/*.json` and runs each case against `rust-peglib`.
//!
//! Environment variables:
//!   PEG_SPEC_FOCUS=<name>   Run only the group with this name.
//!   PEG_SPEC_VERBOSE=1      Print each group as it runs.

mod handlers;
mod semantic;

use handlers::SpecHandlers;
use peglib::Parser;
use semantic::{value_matches, Evaluator};
use serde_json::Value;
use std::collections::BTreeMap;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

fn main() -> ExitCode {
    let focus = env::var("PEG_SPEC_FOCUS").ok();
    let verbose = env::var("PEG_SPEC_VERBOSE").is_ok();

    let spec_dir = find_spec_dir();
    let files = discover_json_files(&spec_dir);

    let mut totals = Stats::default();

    for path in &files {
        let file_stats = run_file(path, focus.as_deref(), verbose);
        totals.add(&file_stats);
    }

    println!();
    println!("==================================================");
    println!("Rust harness summary");
    println!("==================================================");
    print_stats(&totals);

    if !totals.skip_reasons.is_empty() {
        println!();
        println!("Skip breakdown (by unsupported feature):");
        let mut entries: Vec<_> = totals.skip_reasons.iter().collect();
        entries.sort_by(|a, b| b.1.cmp(a.1));
        for (reason, count) in entries {
            println!("  {count:5}  {reason}");
        }
    }

    if totals.failed > 0 {
        ExitCode::from(1)
    } else {
        ExitCode::SUCCESS
    }
}

fn find_spec_dir() -> PathBuf {
    let start = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    for ancestor in start.ancestors() {
        let candidate = ancestor.join("spec").join("tests");
        if candidate.is_dir() {
            return candidate;
        }
    }
    panic!("could not locate spec/tests directory");
}

fn discover_json_files(dir: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    walk(dir, &mut files);
    files.sort();
    files
}

fn walk(dir: &Path, out: &mut Vec<PathBuf>) {
    for entry in fs::read_dir(dir).unwrap() {
        let entry = entry.unwrap();
        let path = entry.path();
        if path.is_dir() {
            walk(&path, out);
        } else if path.extension().and_then(|s| s.to_str()) == Some("json") {
            out.push(path);
        }
    }
}

#[derive(Default)]
struct Stats {
    passed: usize,
    failed: usize,
    skipped_groups: usize,
    skip_reasons: BTreeMap<String, usize>,
}

impl Stats {
    fn add(&mut self, other: &Stats) {
        self.passed += other.passed;
        self.failed += other.failed;
        self.skipped_groups += other.skipped_groups;
        for (k, v) in &other.skip_reasons {
            *self.skip_reasons.entry(k.clone()).or_insert(0) += v;
        }
    }

    fn record_skip(&mut self, reason: &str) {
        self.skipped_groups += 1;
        *self.skip_reasons.entry(reason.to_string()).or_insert(0) += 1;
    }
}

fn print_stats(stats: &Stats) {
    let run = stats.passed + stats.failed;
    println!("  Cases run:     {run}");
    println!("  Passed:        {}", stats.passed);
    println!("  Failed:        {}", stats.failed);
    println!("  Groups skipped:{}", stats.skipped_groups);
}

fn run_file(path: &Path, focus: Option<&str>, verbose: bool) -> Stats {
    let mut stats = Stats::default();
    let text = fs::read_to_string(path).unwrap();
    let data: Value = serde_json::from_str(&text).unwrap();
    let groups = data.as_array().expect("top-level JSON must be an array");


    for group in groups {
        let name = group
            .get("name")
            .and_then(|v| v.as_str())
            .unwrap_or("<unnamed>");

        if let Some(f) = focus {
            if name != f {
                continue;
            }
        }

        match group_support(group) {
            Support::Skip(reason) => {
                if verbose {
                    eprintln!("[skip] {name} ({reason})");
                }
                stats.record_skip(&reason);
                continue;
            }
            Support::Run => {}
        }

        if verbose {
            eprintln!("[run ] {name} ({})", path.display());
        }

        run_group(name, group, &mut stats, path);
    }

    stats
}

enum Support {
    Run,
    Skip(String),
}

fn group_support(group: &Value) -> Support {
    if group.get("peg_meta_test").and_then(|v| v.as_bool()) == Some(true) {
        return Support::Skip("peg_meta_test".into());
    }
    Support::Run
}

fn all_grammar_error(cases: &[Value]) -> bool {
    cases
        .iter()
        .all(|c| c.get("grammar_error").and_then(|v| v.as_bool()) == Some(true))
}

fn run_group(name: &str, group: &Value, stats: &mut Stats, path: &Path) {
    let allow_lr = group
        .get("left_recursion")
        .and_then(|v| v.as_bool())
        .unwrap_or(true);
    if group.get("grammar_compiles").and_then(|v| v.as_bool()) == Some(true) {
        let grammar = group["grammar"].as_str().unwrap();
        match Parser::compile(grammar, None, allow_lr) {
            Ok(_) => stats.passed += 1,
            Err(e) => {
                stats.failed += 1;
                report_failure(name, path, "grammar_compiles", "", &format!("grammar failed to compile: {e}"));
            }
        }
        return;
    }

    let Some(grammar_text) = group.get("grammar").and_then(|v| v.as_str()) else {
        return;
    };
    let Some(cases) = group.get("cases").and_then(|v| v.as_array()) else {
        return;
    };

    let start_rule = group.get("start_rule").and_then(|v| v.as_str());
    let parser_result = Parser::compile(grammar_text, start_rule, allow_lr);
    let mut parser = match parser_result {
        Ok(p) => p,
        Err(e) => {
            if all_grammar_error(cases) {
                stats.passed += cases.len();
                return;
            }
            stats.failed += 1;
            report_failure(name, path, "grammar_compile", "", &e.to_string());
            return;
        }
    };
    if group.get("packrat").and_then(|v| v.as_bool()) == Some(true) {
        parser.enable_packrat_parsing();
    }
    let actions_json = group.get("actions");
    let evaluator = actions_json.map(Evaluator::new);
    let has_handlers = group.get("handlers").is_some()
        || group.get("vars").is_some()
        || group.get("trace").is_some();
    let mut spec_handlers = if has_handlers {
        Some(SpecHandlers::new(
            group.get("vars"),
            group.get("handlers"),
            group.get("trace"),
        ))
    } else {
        None
    };
    let ast_mode = group.get("ast").is_some() || evaluator.is_some() || has_handlers;
    let ast_optimize = group
        .get("ast")
        .and_then(|v| v.get("optimize"))
        .and_then(|v| v.as_bool())
        == Some(true);
    // Grammars using `^label` or `%recover` always route through the
    // recovery-aware entry point, even without AST mode.
    let needs_recovery = grammar_text.contains('^') || grammar_text.contains("%recover");

    for (idx, case) in cases.iter().enumerate() {
        if case.get("grammar_error").and_then(|v| v.as_bool()) == Some(true) {
            stats.failed += 1;
            report_failure(
                name,
                path,
                "grammar_error",
                "",
                "grammar compiled but grammar_error was expected",
            );
            continue;
        }

        let input = case.get("input").and_then(|v| v.as_str()).unwrap_or("");
        let expected_match = case.get("match").and_then(|v| v.as_bool());

        let expected_value = case.get("expected_value");
        let expected_state = case.get("expected_state");
        let expected_error = case.get("expected_error");
        let expected_trace = case.get("expected_trace");
        let expected_trace_prefix = case.get("expected_trace_prefix");

        let Some(expected) = expected_match else {
            continue;
        };

        let expected_errors = case.get("expected_errors");
        let expected_ast = case.get("expected_ast").and_then(|v| v.as_str());

        if ast_mode || needs_recovery || expected_errors.is_some() || expected_error.is_some() {
            // Fresh handlers per case so vars/trace don't leak between runs.
            if has_handlers {
                spec_handlers = Some(SpecHandlers::new(
                    group.get("vars"),
                    group.get("handlers"),
                    group.get("trace"),
                ));
            }
            let mut result = parser.parse_with_errors(
                input,
                ast_mode,
                spec_handlers.as_mut().map(|h| h as &mut _),
            );
            if ast_optimize {
                if let Some(ast) = result.ast.take() {
                    result.ast = Some(peglib::optimize_ast(ast));
                }
            }
            if result.ok != expected {
                stats.failed += 1;
                let detail = if let Some(err) = &result.last_error {
                    format!(
                        "expected={expected} actual={} (case[{idx}]): {err}",
                        result.ok
                    )
                } else {
                    format!("expected={expected} actual={} (case[{idx}])", result.ok)
                };
                report_failure(name, path, "match", input, &detail);
                continue;
            }
            if let (Some(ev), Some(evaluator), Some(ast)) =
                (expected_value, evaluator.as_ref(), result.ast.as_ref())
            {
                let actual_value = evaluator.eval(ast);
                if !value_matches(&actual_value, ev) {
                    stats.failed += 1;
                    report_failure(
                        name,
                        path,
                        "expected_value",
                        input,
                        &format!("expected={ev} actual={actual_value:?} (case[{idx}])"),
                    );
                    continue;
                }
            }
            if let (Some(es), Some(h)) = (expected_state, spec_handlers.as_ref()) {
                if !h.state.matches(es) {
                    stats.failed += 1;
                    report_failure(
                        name,
                        path,
                        "expected_state",
                        input,
                        &format!("expected={es} (case[{idx}])"),
                    );
                    continue;
                }
            }
            if !check_trace(
                name,
                path,
                input,
                idx,
                expected_trace,
                expected_trace_prefix,
                spec_handlers.as_ref(),
                stats,
            ) {
                continue;
            }
            if let Some(ee) = expected_error {
                if !check_expected_error(name, path, input, idx, ee, &result, stats) {
                    continue;
                }
            }
            if let Some(ees) = expected_errors {
                if !check_expected_errors(name, path, input, idx, ees, &result.errors, stats) {
                    continue;
                }
            }
            if let (Some(want), Some(ast)) = (expected_ast, result.ast.as_ref()) {
                let got = peglib::ast_to_s(ast);
                if got != want {
                    stats.failed += 1;
                    report_failure(
                        name,
                        path,
                        "expected_ast",
                        input,
                        &format!("expected:\n{want}\nactual:\n{got}"),
                    );
                    continue;
                }
            }
            stats.passed += 1;
        } else {
            let actual = parser.parse(input);
            if actual == expected {
                stats.passed += 1;
            } else {
                stats.failed += 1;
                report_failure(
                    name,
                    path,
                    "match",
                    input,
                    &format!("expected={expected} actual={actual} (case[{idx}])"),
                );
            }
        }
    }
}

fn check_expected_error(
    name: &str,
    path: &Path,
    input: &str,
    idx: usize,
    expected: &Value,
    result: &peglib::ParseResult<'_>,
    stats: &mut Stats,
) -> bool {
    if let Some(expected_msg) = expected.get("message").and_then(|v| v.as_str()) {
        let matched = result
            .errors
            .iter()
            .any(|e| e.message.contains(expected_msg))
            || result
                .last_error
                .as_deref()
                .is_some_and(|m| m.contains(expected_msg));
        if !matched {
            stats.failed += 1;
            report_failure(
                name,
                path,
                "expected_error",
                input,
                &format!(
                    "expected message={expected_msg:?} actual_errors={:?} last_error={:?} (case[{idx}])",
                    result.errors, result.last_error
                ),
            );
            return false;
        }
    }
    if let Some(want_line) = expected.get("line").and_then(|v| v.as_u64()) {
        let actual = result
            .errors
            .first()
            .map(|e| e.line as u64)
            .or(result.error_line.map(|l| l as u64));
        if actual != Some(want_line) {
            stats.failed += 1;
            report_failure(
                name,
                path,
                "expected_error",
                input,
                &format!("expected line={want_line} actual={actual:?} (case[{idx}])"),
            );
            return false;
        }
    }
    if let Some(want_col) = expected.get("col").and_then(|v| v.as_u64()) {
        let actual = result
            .errors
            .first()
            .map(|e| e.col as u64)
            .or(result.error_col.map(|c| c as u64));
        if actual != Some(want_col) {
            stats.failed += 1;
            report_failure(
                name,
                path,
                "expected_error",
                input,
                &format!("expected col={want_col} actual={actual:?} (case[{idx}])"),
            );
            return false;
        }
    }
    true
}

fn check_expected_errors(
    name: &str,
    path: &Path,
    input: &str,
    idx: usize,
    expected: &Value,
    actual: &[peglib::ErrorReport],
    stats: &mut Stats,
) -> bool {
    let Some(arr) = expected.as_array() else {
        return true;
    };
    if arr.len() != actual.len() {
        stats.failed += 1;
        report_failure(
            name,
            path,
            "expected_errors",
            input,
            &format!(
                "expected {} errors, got {} (case[{idx}]): actual={actual:?}",
                arr.len(),
                actual.len()
            ),
        );
        return false;
    }
    for (i, e) in arr.iter().enumerate() {
        let want_line = e.get("line").and_then(|v| v.as_u64()).map(|n| n as usize);
        let want_col = e.get("col").and_then(|v| v.as_u64()).map(|n| n as usize);
        let want_msg = e.get("message").and_then(|v| v.as_str()).unwrap_or("");
        let got = &actual[i];
        let line_ok = want_line.map_or(true, |l| l == got.line);
        let col_ok = want_col.map_or(true, |c| c == got.col);
        let msg_ok = want_msg.is_empty() || got.message.contains(want_msg);
        if !line_ok || !col_ok || !msg_ok {
            stats.failed += 1;
            report_failure(
                name,
                path,
                "expected_errors",
                input,
                &format!("error[{i}] mismatch: expected={e} actual={got:?} (case[{idx}])"),
            );
            return false;
        }
    }
    true
}

#[allow(clippy::too_many_arguments)]
fn check_trace(
    name: &str,
    path: &Path,
    input: &str,
    idx: usize,
    expected_trace: Option<&Value>,
    expected_trace_prefix: Option<&Value>,
    spec_handlers: Option<&SpecHandlers>,
    stats: &mut Stats,
) -> bool {
    let Some(h) = spec_handlers else {
        return true;
    };
    if let Some(et) = expected_trace {
        let expected: Vec<&str> = et
            .as_array()
            .map(|a| a.iter().filter_map(|v| v.as_str()).collect())
            .unwrap_or_default();
        let actual: Vec<&str> = h.trace.iter().map(|s| s.as_str()).collect();
        if actual != expected {
            stats.failed += 1;
            report_failure(
                name,
                path,
                "expected_trace",
                input,
                &format!("expected={expected:?} actual={actual:?} (case[{idx}])"),
            );
            return false;
        }
    }
    if let Some(etp) = expected_trace_prefix {
        let expected: Vec<&str> = etp
            .as_array()
            .map(|a| a.iter().filter_map(|v| v.as_str()).collect())
            .unwrap_or_default();
        let actual: Vec<&str> = h.trace.iter().map(|s| s.as_str()).collect();
        if !actual.starts_with(&expected[..]) {
            stats.failed += 1;
            report_failure(
                name,
                path,
                "expected_trace_prefix",
                input,
                &format!("expected prefix={expected:?} actual={actual:?} (case[{idx}])"),
            );
            return false;
        }
    }
    true
}

fn report_failure(name: &str, path: &Path, field: &str, input: &str, detail: &str) {
    eprintln!("FAIL {name} [{field}]");
    eprintln!("  File:  {}", path.display());
    if !input.is_empty() {
        eprintln!("  Input: {:?}", input);
    }
    eprintln!("  {}", detail);
}
