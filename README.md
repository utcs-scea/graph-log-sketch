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

Additionally, there is a bug upon in-place insertion.
