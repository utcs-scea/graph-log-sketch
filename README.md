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
Note that you *must* [enable distributed programs](#Distributed) in galois for this to work and compile.
The build target for this has changed to a more distributed version.
First ensure that your submodules is using the `dgraph` branch of the galois repository.
The build command for this is now ``make -j `nproc` -C galois/lonestar/analytics/distributed/bfs``.
The binary is `galois/lonestar/analytics/distributed/bfs/bfs-push-dist`.

In order to run the binary using slurm the following command is used:
```
srun -N <#hosts> -n <#processes> <BFS_BINARY> <graph-file-name> -graphTranspose=<graph-transpose-filename>
-t <#threads> --runs=<#runs> --partition=<partition-type> --exec=<Sync|Async> --statFile=<stat-filename>
```

- `#hosts` is the Number of Computer Nodes
- `#processes` is the Number of process you would like deployed (usually equal to `#hosts`
- `graph-file-name` Name of generated GR file
- `graph-transpose-file-name` Name of generated TGR file
- `#threads` is the number of threads you want deployed per process
- `#runs` number of times the algorithm should run (64 is Graph500 compliant)\
- `partition-type`: Partitioner Policy
  - `oec`: Outgoing Edge cut
  - `iec`: Incoming Edge cut
  - `cvc`: Cartesian Vertex Cut of oec
  - ... For more look at `--help`
- `Sync` Bulk-Synchronous Model
- `Async` Bulk-Asynchronous Model
- `stat-filename` Name of the stats file

To use mpirun simply replace the first part of the command with:
```
mpirun -n <#processes> <BFS_BINARY>
```
