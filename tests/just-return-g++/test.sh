#! /bin/bash

g++ return.cc -o return
../../build/sold return return.out
../../build/print_dynsymtab return.out
./return.out
