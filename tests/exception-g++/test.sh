#! /bin/bash -eu

g++ -fPIC -shared -o libfuga.so -Wl,-soname,libfuga.so fuga.cc 
g++ -fPIC -shared -o libhoge.so.original -Wl,-soname,libhoge.so hoge.cc libfuga.so
g++ main.cc -o main libhoge.so.original libfuga.so
LD_LIBRARY_PATH=. ../../build/sold -i libhoge.so.original -o libhoge.so.soldout

# Use sold
ln -sf libhoge.so.soldout libhoge.so
# Use original
# ln -sf libhoge.so.original libhoge.so

./main
