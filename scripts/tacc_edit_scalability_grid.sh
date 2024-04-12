#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

SAMPLES=${SAMPLES:-3}

## FRIENDSTER

WORKLOAD=friendster_randomized_20
WORKLOAD_NUM_VERTICES=124836180

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="64" TIME="3:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="128" TIME="01:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="128" TIME="00:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done

## RMAT27

WORKLOAD=rmat27_randomized_20
WORKLOAD_NUM_VERTICES=134217728

for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="64" TIME="3:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="tc" THREADS="128" TIME="01:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bfs" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="bc" THREADS="128" TIME="00:59:00" ./scripts/tacc_edit_scalability.sh; done
for i in $(seq $SAMPLES); do ALGOS="pr" THREADS="128" TIME="00:29:00" ./scripts/tacc_edit_scalability.sh; done
