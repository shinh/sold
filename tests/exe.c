#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "lib.h"

int main() {
    puts("Start");
    if (lib_add_42_via_base(10) != 52) abort();
    if (in_both_lib_and_base() != 100) abort();
    puts("OK");
}
