#include <dlfcn.h>
#include <stdio.h>

int main() {
    void* handle = dlopen("lib.so", RTLD_LAZY);
    if (handle != NULL) {
        void (*func)() = (void (*)())dlsym(handle, "show_tls_variables");
        func();
    } else {
        printf("Cannot open lib.so\n");
    }
    return 0;
}
