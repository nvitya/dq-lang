import sys
import time

default_maxval = 1000000

darr = []

def FillArray(maxval):
    global darr
    darr.clear()
    for i in range(maxval):
        darr.append(i)

def FillArrayPtr(maxval):
    global darr
    darr = [0] * maxval
    for i in range(maxval):
        darr[i] = i

def CalcSum():
    result = 0
    arrlen = len(darr)
    for i in range(arrlen):
        result += darr[i]
    return result

def CalcSumPtr():
    result = 0
    arrlen = len(darr)
    for i in range(arrlen):
        result += darr[i]
    return result

def NanoTime():
    return time.monotonic_ns()

def main():
    print("DynArray Test [Python]")

    maxval = default_maxval

    if len(sys.argv) > 1:
        maxval = int(sys.argv[1])

    print(f"maxval = {maxval}")

    print("Filling the dynamic array...")
    t1 = NanoTime()
    FillArray(maxval)
    t2 = NanoTime()
    print(f"Total fill time: {(t2 - t1) // 1000} us")

    print("Summing the dynamic array...")
    t1 = NanoTime()
    sum_val = CalcSum()
    t2 = NanoTime()
    print(f"sum = {sum_val}")
    print(f"Total sum time: {(t2 - t1) // 1000} us")

    print("\nUsing pointer operations\n")

    print("Filling the dynamic array (ptr)...")
    t1 = NanoTime()
    FillArrayPtr(maxval)
    t2 = NanoTime()
    print(f"Total fill time: {(t2 - t1) // 1000} us")

    print("Summing the dynamic array (ptr)...")
    t1 = NanoTime()
    sum_val = CalcSumPtr()
    t2 = NanoTime()
    print(f"sum = {sum_val}")
    print(f"Total sum time: {(t2 - t1) // 1000} us")

if __name__ == "__main__":
    main()
