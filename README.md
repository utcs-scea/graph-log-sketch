<!--
  ~ SPDX-License-Identifier: BSD-2-Clause
  ~ Copyright (c) 2023. University of Texas at Austin. All rights reserved.
  -->

# Graph Log Sketches

This repository implements benchmarking tools to evaluate
graph representations against various workloads.

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

A workload is a text file comprising batched updates and
algorithm execution points.

* A blank line indicates an algorithm execution point.
* Any other line is an update to the graph. Edge insertions
are of the form `src dst1 dst2 dst3 ...`.
* Updates are batched together and executed in parallel,
with algorithm executions acting as a logical barrier.

Let's look at an example:

```plaintext
1 2
1 3
1 4

2 3 4
```

This workload has two "batches": the first creates three edges in parallel
(`1->2`, `1->3`, and `1->4`).
Then, the algorithm is executed once. Finally, two more edges are created
(`2->3` and `2->4`).

## Quick Setup

```shell
git submodule update --init --recursive
git lfs install
git lfs pull
make docker-image
make docker
# These commands are run in the container `make docker` drops you into
make setup
make -C build -j8
make tests
```

Contributors should also run:

```shell
make dependencies
make hooks
```

## Tools

### [asdf](https://asdf-vm.com)

Provides a declarative set of tools pinned to
specific versions for environmental consistency.

These tools are defined in `.tool-versions`.
Run `make dependencies` to initialize a new environment.

### [pre-commit](https://pre-commit.com)

A left shifting tool to consistently run a set of checks on the code repo.
Our checks enforce syntax validations and formatting.
We encourage contributors to use pre-commit hooks.

```shell
# install all pre-commit hooks
make hooks

# run pre-commit on repo once
make pre-commit
```

### [ninja](https://ninja-build.org/)

Developers can use Ninja instead of Make to build by adding the following to their
`.bashrc` or `.profile` settings.

```shell
export GALOIS_CONTAINER_ENV="-e GALOIS_BUILD_TOOL=Ninja"
```
