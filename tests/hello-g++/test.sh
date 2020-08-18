#! /bin/bash

g++ hello.cc -o hello
../../build/sold hello -o hello.out --section-headers
../../build/print_dynsymtab hello.out
./hello.out
