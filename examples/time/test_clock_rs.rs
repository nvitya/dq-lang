#[repr(C)]
struct timespec {
    tv_sec: i64,
    tv_nsec: i64,
}

extern "C" {
    fn clock_gettime(clk_id: i32, tp: *mut timespec) -> i32;
}

const CLOCK_MONOTONIC: i32 = 1;

fn main() {
    let mut ts = timespec {
        tv_sec: 0,
        tv_nsec: 0,
    };
    unsafe {
        clock_gettime(CLOCK_MONOTONIC, &mut ts);
    }
    println!("{} {}", ts.tv_sec, ts.tv_nsec);
}
