#include <stdio.h>
#include "lib.h"

int main() {
    printf("i = %d\n", return_tls_i());
    printf("j = %d\n", thread_local_j);
    return 0;
}
