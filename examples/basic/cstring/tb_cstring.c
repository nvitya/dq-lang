#include <stdio.h>
#include <stdint.h>

extern void dq_test_cstring();

int main()
{
  printf("Testing DQ cstring operations:\n\n");
  dq_test_cstring();
  return 0;
}
