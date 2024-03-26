<!--
  ~ SPDX-License-Identifier: BSD-2-Clause
  ~ Copyright (c) 2023. University of Texas at Austin. All rights reserved.
  -->

# Graph Log Sketch

This repository implements benchmarking tools to evaluate
graph representations against various workloads.

## Goal

Using LS_CSR as an in-memory graph representation
and demonstrating that it outperforms
other in-memory representations in terms of various metrics

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

Developers can use Ninja instead of Make to build by adding the following to the
git ignored file `env-docker.sh` in the source tree root.

```shell
export GALOIS_BUILD_TOOL=Ninja
```

## Workload Format

A workload is a text file comprising batched updates and algorithm execution points.

* A blank line indicates an algorithm execution point.
* Any other line is an update to the graph.
Edge insertions are of the form `src dst1 dst2 dst3 ...`.
* Updates are batched together and executed in parallel,
with algorithm executions acting as a logical barrier.

Let's look at an example:

```plaintext
1 2
1 3
1 4

2 3 4
```

This workload has two "batches":
the first creates three edges in parallel (`1->2`, `1->3`, and `1->4`).
Then, the algorithm is executed once.
Finally, two more edges are created (`2->3` and `2->4`).

## Microbenchmarks

### Ingestion

1. For the same type of graph, various ingestion methods
(currently streaming workload, batched updates
and ingesting complete graph at the same time
2. Order of edges
    * Globally Sorted (uninteresting workload since edits will never happen)
    * Uniformly random (extreme case)
    * A more realistic workload, where there is a distribution such that a
    contiguous set of edges for a given vertex occur together,
    for example, N1 edges for v1, N2 edges for v2, …,
    Nm edges for vm, N1’ edges for v1, N2’ edges for v2, …
    * Start with random edges (not ordered in any fashion or way)
    * Can we define a quantitative measure of “randomness”
    for the workload?
    For example, if there are a total of N updates to be made to the graph,
    every time in the update edge list, if list[i].src != list[i+1].src,
    increment a count variable and then obtain (count/N)
    * The above methodology does not take into account the out-degree of the vertex
    when the switch happens
    (when we switch from vertex i to j
    while making our updates,
    we will have to copy the entire edge list of vertex i
    to the tail of the LS_CSR -
    can we weigh the individual counts by the outdegrees
    to get a more realistic sense of the “randomness”?
3. More generally,
ingests can be thought of as updates if we include deletions as well
4. Running algorithms on the graph
    * Nop
    * BFS
    * Triangle Counting
    * PageRank
    * Connected Components

## Experimental Inputs

We will use several types of graphs which can vary by

1. Input Size
2. Topology
    * Sparse
    * Power Law graphs

## Measurements

We plan to measure the following properties:

1. Speed (follows from cacheability?)
2. Memory Usage
    * Compaction strategies should impact memory usage -
    more specifically,
    we want to observe that whenever a compaction call is made,
    how much memory do we recover as well as
    how does it affect the overall memory usage of the program
    * How do deletions impact memory usage
    (how much memory do we recover in comparison to
     when we don’t use compactions
    to recover memory from deletions) -
    basically measure resident set size with and without compactions
3. Scalability (doesn’t exist for now) ->
Chunk up buffer and copy (parallelize memcpy)
4. Cacheability
(measuring the number of cache misses across different workloads?) -
Investigate this (ingest itself caches the last access) -
why? Rather than an actual metric

Start measurements on a single local machine and then move to a distributed setting

## Correctness

Graph constructed correctly
(any exercising of graph API same as using the CSR constructed from it)

## Moving to a Distributed Setting

1. Partitioning Policy - given an initial graph,
how do we distribute it efficiently among the hosts
(depending on how much of the graph is available to us,
complete graph vs streaming workload,
will the partitioning policy look different for these scenarios?)
2. Edits - efficient method to figure which edit corresponds to which host
    * Edits to existing vertices
    * Adding new vertices (which host gets the ownership of the new vertex)

## Baselines

1. GraphOne
