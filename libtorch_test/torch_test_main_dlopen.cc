#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    void* handle;
    void (*test)();
    handle = dlopen("libtorch_test.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }
    dlerror();
    test = (void (*)())dlsym(handle, "test");
    test();
    return 0;
}
