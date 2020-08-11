#! /bin/bash

g++ hello.cc -o hello
../../build/sold hello -o hello.out
../../build/print_dynsymtab hello.out
./hello.out
