#include <stdio.h>

extern int dq_struct_test();
extern int dq_struct_test_noderef();

int main() {
    printf("Running struct test...\n");
    dq_struct_test();
    dq_struct_test_noderef();
    printf("Struct test finished.\n");
    return 0;
}
