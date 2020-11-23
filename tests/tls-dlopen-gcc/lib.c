#include <stdio.h>

__thread int tls_data = 0;
__thread int tls_bss;

void show_tls_variables() {
    tls_data = 10;
    tls_bss = 10;
    printf("tls_data = %d, tls_bss = %d\n", tls_data, tls_bss);
}
