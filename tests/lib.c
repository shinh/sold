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
