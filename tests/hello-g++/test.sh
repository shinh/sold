#! /bin/bash

g++ hello.cc -o hello
../../build/sold hello -o hello.out --section-headers
./hello.out
