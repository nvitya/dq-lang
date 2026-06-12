import sys
import time

default_f1 = 2.200002
default_f2 = 2.200001
default_million_iter = 5

def MilliTime():
    return time.monotonic_ns() / 1000000.0

def FpBench(f1, f2, iterations):
    ans = 1.0
    for _ in range(iterations):
        ans *= f1
        ans /= f2
    return ans

def main():
    print("Floating Point Benchmark [Python]")

    f1 = default_f1
    f2 = default_f2
    million_iter = default_million_iter

    if len(sys.argv) > 1:
        million_iter = int(sys.argv[1])

    iterations = million_iter * 1000000

    print(f"FP benchmark with F1={f1:f}, F2={f2:f}, million_iterations={million_iter}:")
    tstart = MilliTime()
    ans = FpBench(f1, f2, iterations)
    tend = MilliTime()
    elapsed = tend - tstart
    print(f"  ans = {ans:f}, time = {elapsed:.3f} ms")
    if elapsed > 0:
        print(f"  {int(iterations / elapsed)} loop/msec")

if __name__ == "__main__":
    main()
