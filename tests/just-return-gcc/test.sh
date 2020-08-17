#! /bin/bash

gcc return.c -o return
../../build/sold return -o return.out
../../build/print_dynsymtab return.out
./return.out
