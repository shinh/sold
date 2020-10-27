#! /bin/bash -eu

gcc -Wl,--hash-style=gnu -o main main.c

../../build/sold main -o main.out --section-headers
./main.out
