#! /bin/bash

gcc return.c -o return.out
../../build/sold return.out -o return.soldout --section-headers --check-output
./return.soldout
