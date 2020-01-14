#!/bin/bash

set -e

if [ ! -z $1 ]; then
    cd "$1"
fi

out=out
mkdir -p "${out}"

sotest="${out}/sotest"
mkdir -p "${sotest}"
./sold tests/libtest_lib.so "${sotest}/libtest_lib.so"
cp tests/test_exe "${sotest}/test_exe"
"${sotest}/test_exe"
