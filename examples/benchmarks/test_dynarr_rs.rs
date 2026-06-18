use std::env;
use std::time::Instant;
use std::hint::black_box;

const DEFAULT_MAXVAL: i32 = 1000000;

#[allow(non_upper_case_globals)]
static mut darr: Vec<i32> = Vec::new();

fn fill_array(maxval: i32) {
    unsafe {
        darr.clear();
        for i in 0..maxval {
            darr.push(black_box(i));
        }
    }
}

fn fill_array_ptr(maxval: i32) {
    unsafe {
        darr.resize(maxval as usize, 0);
        let ptr = darr.as_mut_ptr();
        for i in 0..maxval {
            *ptr.add(i as usize) = i;
        }
    }
}

fn calc_sum() -> i64 {
    let mut result: i64 = 0;
    unsafe {
        for i in 0..darr.len() {
            result += black_box(darr[i] as i64);
        }
    }
    result
}

fn calc_sum_ptr() -> i64 {
    let mut result: i64 = 0;
    unsafe {
        let ptr = darr.as_ptr();
        for i in 0..darr.len() {
            result += black_box(*ptr.add(i) as i64);
        }
    }
    result
}

fn main() {
    println!("DynArray Test [Rust]");

    let mut maxval = DEFAULT_MAXVAL;

    let args: Vec<String> = env::args().collect();
    if args.len() > 1 {
        if let Ok(v) = args[1].parse::<i32>() {
            maxval = v;
        }
    }

    println!("maxval = {}", maxval);

    println!("Filling the dynamic array...");
    let start1 = Instant::now();
    fill_array(maxval);
    let elapsed1 = start1.elapsed();
    println!("Total fill time: {} us", elapsed1.as_micros());

    println!("Summing the dynamic array...");
    let start2 = Instant::now();
    let sum = calc_sum();
    let elapsed2 = start2.elapsed();
    println!("sum = {}", sum);
    println!("Total sum time: {} us", elapsed2.as_micros());

    println!("\nUsing pointer operations\n");

    println!("Filling the dynamic array (ptr)...");
    let start3 = Instant::now();
    fill_array_ptr(maxval);
    let elapsed3 = start3.elapsed();
    println!("Total fill time: {} us", elapsed3.as_micros());

    println!("Summing the dynamic array (ptr)...");
    let start4 = Instant::now();
    let sum_ptr = calc_sum_ptr();
    let elapsed4 = start4.elapsed();
    println!("sum = {}", sum_ptr);
    println!("Total sum time: {} us", elapsed4.as_micros());
}
