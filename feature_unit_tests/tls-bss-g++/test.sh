#! /bin/bash -eu

g++ -o main.out main.cc
$(git rev-parse --show-toplevel)/build/sold main.out -o main.soldout --section-headers
./main.soldout
