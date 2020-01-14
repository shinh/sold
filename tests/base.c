#include "base.h"

#include <stdio.h>
#include <stdlib.h>

static int g_init;
__attribute__((constructor)) static void init() {
    puts("Constructor in base");
    g_init = 44;
}

__attribute__((destructor)) static void quit() {
    puts("Destructor in base");
}

int base_init_value() {
    return g_init;
}

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

int base_global = 98;
