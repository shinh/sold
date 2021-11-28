#include "lib.h"

int func() {
    static int v = 0;
    v++;
    return v;
}
