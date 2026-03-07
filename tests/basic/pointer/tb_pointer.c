#include <stdio.h>
#include <stdint.h>

extern int64_t  dq_ptr_basic(int64_t a);
extern int64_t  dq_ptr_read(int64_t a);
extern int64_t  dq_ptr_null();
extern int64_t  dq_ptr_cmp(int64_t a, int64_t b);
extern int64_t  dq_ptr_notnull(int64_t a);
extern int64_t  dq_ptr_and_arr();

int main()
{
  printf("Testing DQ pointer operations:\n\n");

  int64_t r;

  r = dq_ptr_basic(42);
  printf("dq_ptr_basic(42) = %li (expected 100)\n", r);

  r = dq_ptr_read(55);
  printf("dq_ptr_read(55) = %li (expected 55)\n", r);

  r = dq_ptr_null();
  printf("dq_ptr_null() = %li (expected 1)\n", r);

  r = dq_ptr_cmp(10, 20);
  printf("dq_ptr_cmp(10, 20) = %li (expected 0)\n", r);

  r = dq_ptr_notnull(77);
  printf("dq_ptr_notnull(77) = %li (expected 77)\n", r);

  r = dq_ptr_and_arr();
  printf("dq_ptr_and_arr() = %li (expected 9)\n", r);

  return 0;
}
