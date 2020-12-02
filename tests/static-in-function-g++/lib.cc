#include "lib.h"
#include <vector>

int func() {
    static int v = 0;
    v++;
    return v;
}
