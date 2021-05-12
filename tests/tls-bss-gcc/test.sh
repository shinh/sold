#! /bin/bash -eu

gcc -Wl,--hash-style=gnu -o main.out main.c

../../build/sold main.out -o main.soldout --section-headers
./main.soldout
