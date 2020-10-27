#! /bin/bash

gcc return.c -o return
../../build/sold return -o return.out --section-headers
./return.out
