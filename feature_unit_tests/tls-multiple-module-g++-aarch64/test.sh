#! /bin/bash -eu

aarch64-linux-gnu-g++ -shared -fPIC -o libfuga.so -Wl,-soname,libfuga.so fuga.cc
aarch64-linux-gnu-g++ -shared -fPIC -o libhoge.so -Wl,-soname,libhoge.so hoge.cc
aarch64-linux-gnu-g++ -shared -fPIC -o libfugahoge.so.original fugahoge.cc libfuga.so libhoge.so
aarch64-linux-gnu-g++ -o main main.cc -ldl 

LD_LIBRARY_PATH=. $(git rev-parse --show-toplevel)/build/sold -i libfugahoge.so.original -o libfugahoge.so.soldout --section-headers --check-output --custom-library-path /usr/aarch64-linux-gnu/lib

LD_LIBRARY_PATH=. qemu-aarch64 -L /usr/aarch64-linux-gnu ./main
