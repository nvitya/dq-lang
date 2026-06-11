#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

int64_t CalcSum(int64_t amax)
{
  int64_t result = 0;
  for (int64_t i = 1; i <= amax; i++)
  {
    result += i;
  }
  return result;
}

uint64_t NanoTime()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(int argc, char *argv[])
{
  printf("Sum time test [C]\n");

  int64_t maxval = 100000000;

  if (argc > 1)
  {
    maxval = strtoll(argv[1], NULL, 10);
  }

  printf("Calculating sum 1..%ld ...\n", maxval);

  uint64_t t1 = NanoTime();

  int64_t sum = CalcSum(maxval);

  uint64_t t2 = NanoTime();

  printf("sum = %ld\n", sum);
  printf("Total exec time: %lu ns\n", t2 - t1);

  return 0;
}
