#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <time.h>

const int default_maxval = 1000000;

std::vector<int32_t> darr;

void FillArray(int32_t maxval) {
    darr.clear();
    for (int32_t i = 0; i < maxval; ++i) {
        darr.push_back(i);
    }
}

void FillArrayPtr(int32_t maxval) {
    darr.resize(maxval);
    int32_t* pi32 = darr.data();
    for (int32_t i = 0; i < maxval; ++i) {
        pi32[i] = i;
    }
}

int64_t CalcSum() {
    int64_t result = 0;
    int32_t arrlen = darr.size();
    for (int32_t i = 0; i < arrlen; ++i) {
        result += darr[i];
    }
    return result;
}

int64_t CalcSumPtr() {
    int64_t result = 0;
    int32_t arrlen = darr.size();
    int32_t* pi32 = darr.data();
    for (int32_t i = 0; i < arrlen; ++i) {
        result += pi32[i];
    }
    return result;
}

uint64_t NanoTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(int argc, char* argv[]) {
    std::cout << "DynArray Test [C++]\n";

    int64_t maxval = default_maxval;

    if (argc > 1) {
        maxval = std::strtoll(argv[1], nullptr, 10);
    }

    std::cout << "maxval = " << maxval << "\n";

    uint64_t t1;
    uint64_t t2;
    int64_t sum = 0;

    std::cout << "Filling the dynamic array...\n";
    t1 = NanoTime();
    FillArray(maxval);
    t2 = NanoTime();
    std::cout << "Total fill time: " << (t2 - t1) / 1000 << " us\n";

    std::cout << "Summing the dynamic array...\n";
    t1 = NanoTime();
    sum = CalcSum();
    t2 = NanoTime();
    std::cout << "sum = " << sum << "\n";
    std::cout << "Total sum time: " << (t2 - t1) / 1000 << " us\n";

    std::cout << "\nUsing pointer operations\n\n";

    std::cout << "Filling the dynamic array (ptr)...\n";
    t1 = NanoTime();
    FillArrayPtr(maxval);
    t2 = NanoTime();
    std::cout << "Total fill time: " << (t2 - t1) / 1000 << " us\n";

    std::cout << "Summing the dynamic array (ptr)...\n";
    t1 = NanoTime();
    sum = CalcSumPtr();
    t2 = NanoTime();
    std::cout << "sum = " << sum << "\n";
    std::cout << "Total sum time: " << (t2 - t1) / 1000 << " us\n";

    return 0;
}
