#! /bin/bash

g++ return.cc -o return
../../build/sold return -o return.out --section-headers
../../build/print_dynsymtab return.out
./return.out
