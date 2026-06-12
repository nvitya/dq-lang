use std::env;
use std::time::Instant;
use std::hint::black_box;

const DEFAULT_MAXVAL: i32 = 1000000;

fn fill_array(darr: &mut Vec<i32>, maxval: i32) {
    darr.clear();
    for i in 0..maxval {
        darr.push(black_box(i));
    }
}

fn fill_array_ptr(darr: &mut Vec<i32>, maxval: i32) {
    darr.resize(maxval as usize, 0);
    let ptr = darr.as_mut_ptr();
    for i in 0..maxval {
        unsafe {
            *ptr.add(i as usize) = i;
        }
    }
}

fn calc_sum(darr: &Vec<i32>) -> i64 {
    let mut result: i64 = 0;
    for i in 0..darr.len() {
        result += black_box(darr[i] as i64);
    }
    result
}

fn calc_sum_ptr(darr: &Vec<i32>) -> i64 {
    let mut result: i64 = 0;
    let ptr = darr.as_ptr();
    for i in 0..darr.len() {
        unsafe {
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

    let mut darr: Vec<i32> = Vec::new();

    println!("Filling the dynamic array...");
    let start1 = Instant::now();
    fill_array(&mut darr, maxval);
    let elapsed1 = start1.elapsed();
    println!("Total fill time: {} us", elapsed1.as_micros());

    println!("Summing the dynamic array...");
    let start2 = Instant::now();
    let sum = calc_sum(&darr);
    let elapsed2 = start2.elapsed();
    println!("sum = {}", sum);
    println!("Total sum time: {} us", elapsed2.as_micros());

    println!("\nUsing pointer operations\n");

    println!("Filling the dynamic array (ptr)...");
    let start3 = Instant::now();
    fill_array_ptr(&mut darr, maxval);
    let elapsed3 = start3.elapsed();
    println!("Total fill time: {} us", elapsed3.as_micros());

    println!("Summing the dynamic array (ptr)...");
    let start4 = Instant::now();
    let sum_ptr = calc_sum_ptr(&darr);
    let elapsed4 = start4.elapsed();
    println!("sum = {}", sum_ptr);
    println!("Total sum time: {} us", elapsed4.as_micros());
}
