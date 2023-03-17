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

### Docker Build Instructions
This should build and launch the docker container
```bash
make -C ./docker
```
Inside that container you can run:
```bash
./prep.sh && ./build.sh
```
This will build `RelWithDebugInfo` into the `docker-build` folder to be run within the container.

## Benchmarking
If you wish to run benchmarking then add `-DBENCH` to the GCC compiler flags in your `CMakeCache.txt` to
`CMAKE_CXX_FLAGS_RELEASE`.

## Distributed
If you wish to run things in a distributed fasion then add `-DGALOIS_ENABLE_DIST=1` to your cmake flags.

## Microbenchmarks (ISBs)
### Jaccard
The build target for jaccard is `jaccard` so you can create this inside your build folder by runing ``make -j`nproc` jaccard``
### BFS
Note that you *must* [enable distributed programs](#distributed) in galois for this to work and compile.
The build target for this has changed to a more distributed version.
First ensure that your submodules is using the `dgraph` branch of the galois repository.
The build command for this is now ``make -j `nproc` -C galois/lonestar/analytics/distributed/bfs``.
The binary is `galois/lonestar/analytics/distributed/bfs/bfs-push-dist`.
