#include <stdio.h>
#include <stdint.h>

//extern int64_t  g_cnt;
//extern int64_t  g_var;
extern int64_t  dq_testfunc(int64_t a, int64_t b);
extern int64_t  dq_add(int64_t a, int64_t b);
extern int64_t  dq_mul(int64_t a, int64_t b);
extern int64_t  dq_ifcheck(int64_t a, int64_t b);
extern int64_t  dq_ilogiops(int64_t a, int64_t b);

extern void     dq_voidfunc(int64_t a);

int main()
{
  printf("Testing DQ integer expressions:\n\n");

  volatile int64_t r;

  r = dq_add(3, 4);
  printf("dq_add(3,4) = %li\n", r);

  r = dq_ilogiops(33, 5);
  printf("dq_ilogiops(33, 5) = %li\n", r);

  //printf("(before dq_voidfunc(): g_cnt = %li\n", g_cnt);
  //dq_voidfunc(5);
  //printf("(after dq_voidfunc(5): g_cnt = %li\n", g_cnt);

  r = dq_testfunc(1, 5);
  printf("dq_testfunc(1,5) = %li\n", r);

  return 0;
}
