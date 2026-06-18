use std::env;
use std::time::Instant;

const DEFAULT_F1: f64 = 2.200002;
const DEFAULT_F2: f64 = 2.200001;
const DEFAULT_MILLION_ITER: i32 = 5;

fn fp_bench_f32(f1: f32, f2: f32, iterations: i32) -> f32 {
    let mut ans: f32 = 1.0;
    for _ in 0..iterations {
        ans *= f1;
        ans /= f2;
    }
    ans
}

fn fp_bench_f64(f1: f64, f2: f64, iterations: i32) -> f64 {
    let mut ans: f64 = 1.0;
    for _ in 0..iterations {
        ans *= f1;
        ans /= f2;
    }
    ans
}

fn main() {
    println!("Floating Point Benchmark [Rust]");

    let f1: f64 = DEFAULT_F1;
    let f2: f64 = DEFAULT_F2;
    let mut million_iter = DEFAULT_MILLION_ITER;

    let args: Vec<String> = env::args().collect();
    if args.len() > 1 {
        if let Ok(v) = args[1].parse::<i32>() {
            million_iter = v;
        }
    }

    let iterations = million_iter * 1000000;

    println!("FP32 benchmark with F1={}, F2={}, million_iterations={}:", f1, f2, million_iter);
    let start = Instant::now();
    let ans_f32 = fp_bench_f32(f1 as f32, f2 as f32, iterations);
    let elapsed = start.elapsed();
    let elapsed_ms = elapsed.as_secs_f64() * 1000.0;
    println!("  ans = {}, time = {:.3} ms", ans_f32, elapsed_ms);
    if elapsed_ms > 0.0 {
        println!("  {} loop/msec", (iterations as f64 / elapsed_ms) as i32);
    }

    println!("FP64 benchmark with F1={}, F2={}, million_iterations={}:", f1, f2, million_iter);
    let start = Instant::now();
    let ans_f64 = fp_bench_f64(f1, f2, iterations);
    let elapsed = start.elapsed();
    let elapsed_ms = elapsed.as_secs_f64() * 1000.0;
    println!("  ans = {}, time = {:.3} ms", ans_f64, elapsed_ms);
    if elapsed_ms > 0.0 {
        println!("  {} loop/msec", (iterations as f64 / elapsed_ms) as i32);
    }
}
