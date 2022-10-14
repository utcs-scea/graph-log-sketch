# Graph Log Sketches
This repository has sketches for graphs to be designed to ingest updates

## Build Instructions
Currently everything is linked to small-tests which can be run as a ctest or a binary itself.

```bash
mkdir -p build
cd build
cmake ../
make -j4
```
The small-test binary has an extensive help menu to aid identifying failing cases.

## Benchmarking
If you wish to run benchmarking then add `-DBENCH` to the GCC compiler flags in your `CMakeCache.txt` to
`CMAKE_CXX_FLAGS_RELEASE`.



