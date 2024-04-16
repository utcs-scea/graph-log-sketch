#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

SAMPLES=${SAMPLES:-5}

## FRIENDSTER

export WORKLOAD="friendster_randomized_50_s=1_bigfirst.txt"
export WORKLOAD_NUM_VERTICES="124836180"

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="128" TIME="05:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="128" TIME="01:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done

## RMAT27

export WORKLOAD="rmat27_randomized_20_s=0_bigfirst.txt"
export WORKLOAD_NUM_VERTICES="134217728"

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="128" TIME="05:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="128" TIME="01:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
