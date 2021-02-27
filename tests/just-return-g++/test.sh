#! /bin/bash

g++ return.cc -o return
../../build/sold return -o return.out --section-headers --check-output
./return.out
