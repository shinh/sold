#! /bin/bash -eu

g++ -o main.out main.cc
../../build/sold main.out -o main.soldout --section-headers
./main.soldout
