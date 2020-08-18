#! /bin/bash

gcc hello.c -o hello
../../build/sold hello -o hello.out --section-headers 
../../build/print_dynsymtab hello.out
./hello.out
