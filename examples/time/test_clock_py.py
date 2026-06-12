import time

def main():
    # Python 3.7+ supports time.clock_gettime_ns
    ts = time.clock_gettime_ns(time.CLOCK_MONOTONIC)
    sec = ts // 1000000000
    nsec = ts % 1000000000
    print(f"{sec} {nsec}")

if __name__ == "__main__":
    main()
