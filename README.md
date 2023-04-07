# Graph Log Sketches
This repository has sketches for graphs to be designed to ingest updates

## Build Instructions
Make sure that you have run the following
```bash
git submodule update --init --recursive --force
```

```bash
mkdir -p build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release -DNO_TORCH=1
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

#### BFS Randomization
Generating a random file is done *exactly* the same way as in the combBLAS DirOpt benchmark.
The binary would be `bfs-rand-gen` with the scale parameter used for DirOpt.
The graph in the edge-list format is output to `stdout`.
This can then be put through `dist-graph-convert` converted to a `gr -> cgr -> tgr`, so that the BFS can be run on the *exact* same kind of graph as the
combBLAS one.

Sample commands:
```
mpirun -N <#processes> ./gen-rand-bfs <scale> | sed '1d' > temp.el
mpirun -N <#processes> ./dist-graph-convert --edgelist2gr  temp.el  temp.gr
mpirun -N <#processes> ./dist-graph-convert --gr2cgr       temp.gr  temp.cgr
mpirun -N <#processes> ./dist-graph-convert --gr2tgr       temp.cgr temp.tgr
mpirun -N <#processes> <BFS_BINARY> temp.cgr -graphTranspose=temp.tgr <other-args>
```

### Triangle Counting
There are two versions of triangle counting in Galois.
- Pangolin
- DistGraph
#### Pangolin
To build this run
``make -j `nproc` -C galois/lonestar/mining/cpu/traingle-counting ``
#### DistGraph
``make -j `nproc` -C galois/lonestar/analytics/distributed/triangle-counting ``

#### Triangle Counting Randomization
The binary would be `bfs-rand-triangle` with the following inputs:
```
./bfs-rand-triangle <threads> <scale> <edge ratio> <A> <B> <C>
```

The graph in the edge-list format is output to `stdout`.
This can then be put through `dist-graph-convert` converted to a `gr -> cgr -> tgr`.

Sample commands:
```
mpirun -N <#processes> sh -c './gen-rand-tricount <threads> <scale> <edge ratio> <A> <B> <C> > temp-${OMPI_COMM_WORLD_RANK}.el'
cat temp-*.el > ./temp.el
mpirun -N <#processes> ./dist-graph-convert --edgelist2gr  temp.el  temp.gr
mpirun -N <#processes> ./dist-graph-convert --gr2sgr       temp.gr  temp.sgr
mpirun -N <#processes> <TC-BINARY> temp.cgr  <other-args>
```

## Tools
### Graph Conversion
This can be build ``make -C ./galois/tools/dist-graph-convert/ -j `nproc` ``.

