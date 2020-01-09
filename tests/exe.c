#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "lib.h"



int main() {
    if (lib_add_42_via_base(10) != 52) abort();
    puts("OK");
}
