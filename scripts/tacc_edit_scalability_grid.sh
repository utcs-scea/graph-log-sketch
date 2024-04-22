#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

export BUILD=${BUILD:-/home/$(whoami)/scea/graph-log-sketch/build}
RESULTS_DIR="${RESULTS_DIR:-$SCRATCH/scea/graph-log-sketch/data}"

## friendster
SAMPLES=${SAMPLES:-3}
export WORKLOAD="friendster_randomized_50_s=1_bigfirst"
export WORKLOAD_NUM_VERTICES="124836180"
export OUT_DIR="${RESULTS_DIR}/${WORKLOAD}"

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="128" TIME="01:44:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="128" TIME="02:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="128" TIME="00:59:00" ./scripts/tacc_edit_scalability.sh; done

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="64" TIME="05:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="64" TIME="00:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="64" TIME="05:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="64" TIME="01:59:00" ./scripts/tacc_edit_scalability.sh; done

## rmat27
SAMPLES=${SAMPLES:-3}
export WORKLOAD="rmat27_randomized_20_s=0_bigfirst"
export WORKLOAD_NUM_VERTICES="134217728"
export OUT_DIR="${RESULTS_DIR}/${WORKLOAD}"

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="128" TIME="05:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="128" TIME="01:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="128" TIME="00:59:00" ./scripts/tacc_edit_scalability.sh; done

## road-USA

SAMPLES=${SAMPLES:-10}
export WORKLOAD="road-USA_50_s=0_bigfirst"
export WORKLOAD_NUM_VERTICES="23947347"
export OUT_DIR="${RESULTS_DIR}/${WORKLOAD}"

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="128" TIME="00:10:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:9:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="128" TIME="00:9:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="128" TIME="00:9:00" ./scripts/tacc_edit_scalability.sh; done
