#! /bin/bash

g++ hello.cc -o hello
$(git rev-parse --show-toplevel)/build/sold hello -o hello.out --section-headers --check-output
./hello.out
