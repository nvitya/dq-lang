import sys
import time

def NanoTime():
    return time.monotonic_ns()

#-----------------------------------------------------------------------------

def CalcSum(amax):
    result = 0
    for i in range(1, amax + 1):
        result += i
    return result

def main():
    print("Sum time test [Python]")

    maxval = 100000000

    if len(sys.argv) > 1:
        maxval = int(sys.argv[1])

    print(f"Calculating sum 1..{maxval} ...")

    t1 = NanoTime()
    sum_val = CalcSum(maxval)
    t2 = NanoTime()

    print(f"sum = {sum_val}")
    print(f"Total exec time: {t2 - t1} ns")

if __name__ == "__main__":
    main()
