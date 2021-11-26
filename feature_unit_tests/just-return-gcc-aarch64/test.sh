#! /bin/bash

aarch64-linux-gnu-gcc return.c -o return.out
$(git rev-parse --show-toplevel)/build/sold return.out -o return.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib
qemu-aarch64 -L /usr/aarch64-linux-gnu ./return.soldout
