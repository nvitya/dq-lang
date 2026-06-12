use std::env;
use std::time::Instant;
use std::hint::black_box;

fn calc_sum(amax: i64) -> i64 {
    let mut result: i64 = 0;
    for i in 1..(amax+1) {
        result += black_box(i);
    }
    result
}

fn main() {
    println!("Sum time test [Rust]");
    let mut maxval: i64 = 100000000;
    let args: Vec<String> = env::args().collect();
    if args.len() > 1 {
        if let Ok(v) = args[1].parse::<i64>() {
            maxval = v;
        }
    }
    println!("Calculating sum 1..{} ...", maxval);
    let start = Instant::now();
    let sum = calc_sum(maxval);
    let elapsed = start.elapsed();
    println!("sum = {}", sum);
    println!("Total exec time: {} ns", elapsed.as_nanos());
}
