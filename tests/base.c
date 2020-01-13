#include "base.h"

int base_42() {
    printf("called %s\n", __func__);
    return 42;
}

int base_add_42(int x) {
    printf("called %s\n", __func__);
    return x + 42;
}
