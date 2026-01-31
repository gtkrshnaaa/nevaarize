// Rust Benchmark Suite
// Comparable to C++/Nevaarize benchmarks

use std::time::Instant;

fn bench_int() {
    let limit: i64 = 1_000_000_000;
    let start = Instant::now();
    let mut i: i64 = 0;
    while i < limit {
        std::hint::black_box(i);
        i += 1;
    }
    let elapsed = start.elapsed().as_secs_f64();
    let ops = limit as f64 / elapsed;
    println!("  Integer Add     | {:>15.2} OPS/sec | {:.4}s", ops, elapsed);
}

fn bench_double() {
    let limit: i64 = 100_000_000;
    let start = Instant::now();
    let mut val: f64 = 0.0;
    let mut j: i64 = 0;
    while j < limit {
        val += 1.1;
        j += 1;
    }
    let elapsed = start.elapsed().as_secs_f64();
    let ops = limit as f64 / elapsed;
    println!("  Double Arith    | {:>15.2} OPS/sec | {:.4}s", ops, elapsed);
    // Prevent optimization
    if val < 0.0 { println!("{}", val); }
}

fn bench_string() {
    let limit: i64 = 50_000;
    let start = Instant::now();
    let mut s = String::new();
    let mut i: i64 = 0;
    while i < limit {
        s.push('a');
        i += 1;
    }
    let elapsed = start.elapsed().as_secs_f64();
    let ops = limit as f64 / elapsed;
    println!("  String Concat   | {:>15.2} OPS/sec | {:.4}s", ops, elapsed);
    // Prevent optimization
    if s.len() == 0 { println!("{}", s); }
}

fn bench_array() {
    let limit: i64 = 1_000_000;
    let start = Instant::now();
    let mut arr: Vec<i64> = Vec::new();
    let mut k: i64 = 0;
    while k < limit {
        arr.push(k);
        k += 1;
    }
    let elapsed = start.elapsed().as_secs_f64();
    let ops = limit as f64 / elapsed;
    println!("  Array Push      | {:>15.2} OPS/sec | {:.4}s", ops, elapsed);
    // Prevent optimization
    if arr.len() == 0 { println!("{}", arr[0]); }
}

struct Obj {
    val: i64,
}

fn bench_struct() {
    let limit: i64 = 50_000_000;
    let start = Instant::now();
    let mut o = Obj { val: 0 };
    let mut i: i64 = 0;
    while i < limit {
        o.val = i;
        let x = o.val;
        std::hint::black_box(x);
        i += 1;
    }
    let elapsed = start.elapsed().as_secs_f64();
    let ops = limit as f64 / elapsed;
    println!("  Struct Access   | {:>15.2} OPS/sec | {:.4}s", ops, elapsed);
}

fn main() {
    println!(">>> Rust Benchmark Suite <<<");
    println!("  -------------------------------------------------------------");
    println!("  Benchmark       |     Performance | Time");
    println!("  -------------------------------------------------------------");
    bench_int();
    bench_double();
    bench_string();
    bench_array();
    bench_struct();
    println!("  -------------------------------------------------------------");
    println!();
}
