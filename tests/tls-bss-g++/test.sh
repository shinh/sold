#! /bin/bash -eu

g++ -o main main.cc
../../build/sold main -o main.out --section-headers
./main.out
