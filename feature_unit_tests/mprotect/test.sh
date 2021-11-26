#! /bin/bash

gcc -shared -fPIC base.c -o base.so -Wl,-soname,base.so
gcc -shared -fPIC mprotect_check.c base.so -o mprotect_check.so
gcc main.c mprotect_check.so -o main

LD_LIBRARY_PATH=. $(git rev-parse --show-toplevel)/build/sold -i mprotect_check.so -o mprotect_check.so.soldout
mv mprotect_check.so mprotect_check.so.original
ln -sf mprotect_check.so.soldout mprotect_check.so
# ln -sf mprotect_check.so.original mprotect_check.so

LD_LIBRARY_PATH=. ./main
