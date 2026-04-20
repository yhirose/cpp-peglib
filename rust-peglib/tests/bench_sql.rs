use std::fs;
use std::time::Instant;

fn bench<F: FnMut()>(name: &str, iters: usize, mut f: F) {
    for _ in 0..2 { f(); }
    let mut samples = Vec::with_capacity(iters);
    for _ in 0..iters {
        let t0 = Instant::now();
        f();
        samples.push(t0.elapsed().as_secs_f64() * 1000.0);
    }
    samples.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let n = samples.len();
    let median = samples[n / 2];
    let min = samples[0];
    let max = samples[n - 1];
    let mean: f64 = samples.iter().sum::<f64>() / n as f64;
    println!("  {name:<36} median: {median:>10.3} ms  mean: {mean:>10.3} ms  min: {min:>10.3} ms  max: {max:>10.3} ms  (n={n})");
}

#[test]
fn sql_benchmark() {
    let d = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..").join("benchmark").join("data");
    let grammar = fs::read_to_string(d.join("sql.peg")).unwrap();
    let q1 = fs::read_to_string(d.join("q1.sql")).unwrap();
    let tpch = fs::read_to_string(d.join("all-tpch.sql")).unwrap();
    let big = fs::read_to_string(d.join("big.sql")).unwrap();

    println!();
    println!("=========================================");
    println!(" rust-peglib v3 SQL benchmark");
    println!("=========================================");
    println!(" grammar: {} bytes", grammar.len());

    let parser = peglib::Parser::new(&grammar).unwrap();
    assert!(parser.parse(&q1), "q1.sql");
    assert!(parser.parse(&tpch), "all-tpch.sql");
    assert!(parser.parse(&big), "big.sql");

    println!(" q1.sql: {} bytes", q1.len());
    println!(" all-tpch.sql: {} bytes", tpch.len());
    println!(" big.sql: {} bytes", big.len());
    println!();

    bench("PEG: TPC-H Q1", 10, || { let _ = parser.parse(&q1); });
    bench("PEG: all TPC-H", 10, || { let _ = parser.parse(&tpch); });
    bench("PEG: big.sql (~1MB)", 10, || { let _ = parser.parse(&big); });
}
