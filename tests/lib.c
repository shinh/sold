#include "lib.h"

#include "base.h"

int lib_42() {
    return 42;
}

int lib_add_42_via_base(int v) {
    return base_add_42(v);
}
