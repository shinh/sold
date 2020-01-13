#include "lib.h"

#include <stdio.h>

#include "base.h"

int lib_42() {
    printf("called %s\n", __func__);
    return 42;
}

int lib_add_42_via_base(int v) {
    printf("called %s\n", __func__);
    return base_add_42(v);
}

int in_both_lib_and_base() {
    printf("called %s\n", __func__);
    return 100;
}

void lib_use_stderr() {
    fprintf(stderr, "Use stderr from base\n");
}
