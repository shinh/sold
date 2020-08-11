#! /bin/bash

g++ hello.cc -o hello
../../build/sold hello hello.out
../../build/print_dynsymtab hello.out
./hello.out
