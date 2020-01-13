#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "lib.h"

static int g_init;
__attribute__((constructor)) static void init() {
    puts("Constructor");
    g_init = 42;
}

__attribute__((destructor)) static void quit() {
    puts("Destructor");
}

int main() {
    puts("Start main");
    if (lib_add_42_via_base(10) != 52) abort();
    if (in_both_lib_and_base() != 100) abort();

    fprintf(stderr, "Use stderr from main\n");
    lib_use_stderr();
    base_use_stderr();

    if (g_init != 42) abort();

    puts("OK");
}
