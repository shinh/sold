#! /bin/bash -eu

g++ -Wl,--hash-style=gnu -o main main.cc

../../build/sold main -o main.out --section-headers
./main.out
