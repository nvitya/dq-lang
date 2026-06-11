/* License: public domain

The benchmark code is very simple, it is taken from this project

  https://github.com/jzawodn/arm-neon-vfp-test


*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const double  default_f1            = 2.200002;
const double  default_f2            = 2.200001;
const int     default_million_iter  = 5;

double MilliTime(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * (double)1000 + ts.tv_nsec / (double)1000000;
}

//---------------------------------------------------------------------------------------

float FpBench_f32(float f1, float f2, int iterations)
{
	float ans = 1.0;
	for(int i = 0; i < iterations; i++)
	{
		ans *= f1;
		ans /= f2;
	}
	return ans;
}

double FpBench_f64(double f1, double f2, int iterations)
{
	double ans = 1.0;
	for(int i = 0; i < iterations; i++)
	{
		ans *= f1;
		ans /= f2;
	}
	return ans;
}

int main(int argc, char * argv[])
{
  printf("Floating Point Benchmark [C]\n");

	double  f1 = default_f1;
	double  f2 = default_f2;
	double  ans;
	int     million_iter = default_million_iter;
	double  tstart, tend;

	if (argc > 1)
	{
		million_iter = atoi(argv[1]);
	}

	int iterations = million_iter * 1000000;

	printf("FP32 benchmark with F1=%f, F2=%f, million_iterations=%i:\n", f1, f2, million_iter);
	tstart = MilliTime();
	ans = FpBench_f32(f1, f2, iterations);
	tend = MilliTime();
	printf("  ans = %f, time = %.3f ms\n", ans, tend - tstart);
	printf("  %d loop/msec\n", (int)(iterations/(tend - tstart)));

	printf("FP64 benchmark with F1=%f, F2=%f, million_iterations=%i:\n", f1, f2, million_iter);
	tstart = MilliTime();
	ans = FpBench_f64(f1, f2, iterations);
	tend = MilliTime();
	printf("  ans = %f, time = %.3f ms\n", ans, tend - tstart);
	printf("  %d loop/msec\n", (int)(iterations/(tend - tstart)));

	return 0;
}
