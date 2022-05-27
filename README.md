# sold
`sold` is a linker software which links a shared objects and its depending
shared objects. For example,
```bash
% ldd libhoge.so
    libfuga.so (0x00007fb6550b2000)
% sold -i libhoge.so -o libhoge2.so
% ldd libhoge2.so
%
```
`sold` works only for shared objects not for executables. Sorry.

## Branches
- `master`: I am developing on this branch.
- `stable`: Stable version of master for our internal use.

## Requirements
`sold` works only on Linux on x86-64 and aarch64 architectures.

## How to build
```bash
git clone https://github.com/akawashiro/sold.git
cd sold
mkdir -p build
cd build
cmake ..
make
```
Now you can see the `sold` in `build` directory.

## How to use
```bash
sold -i [INPUT] -o [OUTPUT]
```
Options
- `--section-headers`: Emit section headers. Output shared objects work without section headers but they are useful for debugging.
- `--check-output`: Check integrity of the output by parsing it again.
- `--exclude-so`: Specify a shared object not to combine.

# Renamer
`renamer` is software to rename symbols in shared objects.  You can rename symbols in shared objects like the following.
```
% cat mapping
hoge fugafuga
% renamer libhoge_original.so --output libhoge_renamed.so --rename-mapping-file mapping
```

## Requirements & How to build
Just same as `sold`.

## How to use
```bash
renamer [INPUT] --output [OUTPUT] --rename-mapping-file [MAPPING]
```
All lines in [MAPPING] must be a space separated pair of the old name of a symbol and the new name.

# For developers
Please run "./run-format.sh" before merging to master branch.

## TODO
- Executables
- TLS in executables
    - Initial exec and local exec
- x86-32
- Test Fedora linux in CI

## Integration test with practical libraries
### pybind test
The purpose of this test is to check `sold` can preserve the complex ABI.
```bash
git clone https://github.com/akawashiro/sold.git
cd sold
mkdir -p build
cd build
cmake .. -DSOLD_PYBIND_TEST=ON
make
ctest
```
## libtorch test
```
git clone https://github.com/akawashiro/sold.git
cd sold
mkdir -p build
cd build
cmake -DCMAKE_PREFIX_PATH=/absolute/path/to/libtorch/dir -DSOLD_LIBTORCH_TEST=ON -GNinja ..
ninja
```

## Test with Docker
```
sudo docker build -f ubuntu18.04.Dockerfile .
sudo docker build -f ubuntu20.04.Dockerfile .
sudo docker build -f ubuntu22.04.Dockerfile .
sudo docker build -f fedora-latest.Dockerfile .
```
