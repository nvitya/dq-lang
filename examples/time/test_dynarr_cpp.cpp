#include <print>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <time.h>

using namespace std;

const int default_maxval = 1000000;

vector<int32_t> darr;

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
  int64_t    result = 0;
  int32_t    arrlen = darr.size();
  int32_t *  pi32   = darr.data();
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
  print("DynArray Test [C++]\n");

  int64_t maxval = default_maxval;

  if (argc > 1) {
    maxval = strtoll(argv[1], nullptr, 10);
  }

  print("maxval = {}\n", maxval);

  uint64_t t1;
  uint64_t t2;
  int64_t sum = 0;

  print("Filling the dynamic array...\n");
  t1 = NanoTime();
  FillArray(maxval);
  t2 = NanoTime();
  print("Total fill time: {} us\n", (t2 - t1) / 1000);

  print("Summing the dynamic array...\n");
  t1 = NanoTime();
  sum = CalcSum();
  t2 = NanoTime();
  print("sum = {}\n", sum);
  print("Total sum time: {} us\n", (t2 - t1) / 1000);

  print("\nUsing pointer operations\n\n");

  print("Filling the dynamic array (ptr)...\n");
  t1 = NanoTime();
  FillArrayPtr(maxval);
  t2 = NanoTime();
  print("Total fill time: {} us\n", (t2 - t1) / 1000);

  print("Summing the dynamic array (ptr)...\n");
  t1 = NanoTime();
  sum = CalcSumPtr();
  t2 = NanoTime();
  print("sum = {}\n", sum);
  print("Total sum time: {} us\n", (t2 - t1) / 1000);

  return 0;
}
