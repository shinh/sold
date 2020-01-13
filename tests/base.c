#include "base.h"

#include <stdio.h>
#include <stdlib.h>

int base_42() {
    printf("called %s\n", __func__);
    return 42;
}

int base_add_42(int x) {
    printf("called %s\n", __func__);
    return x + 42;
}

int in_both_lib_and_base() {
    fprintf(stderr, "Must have been overriden\n");
    abort();
}

void base_use_stderr() {
    fprintf(stderr, "Use stderr from base\n");
}
