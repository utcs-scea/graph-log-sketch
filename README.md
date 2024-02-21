# Graph Log Sketches

This repository implements benchmarking tools to evaluate graph representations against various workloads.

## Build Instructions

Clone the dependencies:
```bash
git submodule update --init --recursive --force
```

Then, build the CMake project.
```bash
mkdir -p build
cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release
make -j4
```

## Workload Format

A workload is a text file comprising batched updates and algorithm execution points.

* A blank line indicates an algorithm execution point.
* Any other line is an update to the graph. Edge insertions are of the form `src dst1 dst2 dst3 ...`.
* Updates are batched together and executed in parallel, with algorithm executions acting as a logical barrier.

Let's look at an example:

```
1 2
1 3
1 4

2 3 4
```

This workload has two "batches": the first creates three edges in parallel (`1->2`, `1->3`, and `1->4`). Then, the algorithm is executed once. Finally, two more edges are created (`2->3` and `2->4`).
