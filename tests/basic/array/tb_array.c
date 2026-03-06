#include <stdio.h>
#include <stdint.h>

extern int64_t  dq_arr_basic();
extern int64_t  dq_arr_index(int64_t i);

typedef struct { void * ptr; int64_t len; } dq_slice;
extern int64_t  dq_arr_sum(dq_slice arr);
extern int64_t  dq_arr_slice_test();
extern int64_t  dq_arr_len();
extern int64_t  dq_arr_modify(dq_slice arr);

int main()
{
  printf("Testing DQ array operations:\n\n");

  int64_t r;

  r = dq_arr_basic();
  printf("dq_arr_basic() = %li (expected 60)\n", r);

  r = dq_arr_index(0);
  printf("dq_arr_index(0) = %li (expected 100)\n", r);

  r = dq_arr_index(2);
  printf("dq_arr_index(2) = %li (expected 300)\n", r);

  r = dq_arr_slice_test();
  printf("dq_arr_slice_test() = %li (expected 6)\n", r);

  r = dq_arr_len();
  printf("dq_arr_len() = %li (expected 5)\n", r);

  // Test slice modification from C
  int64_t data[3] = {10, 20, 30};
  dq_slice s = { data, 3 };
  r = dq_arr_modify(s);
  printf("dq_arr_modify({10,20,30}) = %li (expected 999)\n", r);
  printf("  data[0] after modify = %li (expected 999)\n", data[0]);

  return 0;
}
