#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

BUILD="${BUILD:-/home/$(whoami)/scea/graph-log-sketch/build}"
echo "BUILD=$BUILD"

SCRATCH="${SCRATCH:-/var/local/mzinn}"
echo "SCRATCH=$SCRATCH"

echo "WORKLOAD=$WORKLOAD"
WORKLOAD_FILE="${WORKLOAD_FILE:-$SCRATCH/graphs/$WORKLOAD.txt}"
# check that WORKLOAD_FILE exists
if [ ! -f $WORKLOAD_FILE ]; then
	echo "WORKLOAD_FILE does not exist: $WORKLOAD_FILE"
	exit 1
fi
echo "WORKLOAD_FILE=$WORKLOAD_FILE"

RESULTS_DIR="${RESULTS_DIR:-$/scea/graph-log-sketch/results/$WORKLOAD}"

# Count the number of vertices in the workload, if not already set.
if [ -z "$WORKLOAD_NUM_VERTICES" ]; then
	WORKLOAD_NUM_VERTICES="$($BUILD/scripts/count-batched -q -v $WORKLOAD_FILE)"
fi
echo "WORKLOAD_NUM_VERTICES=$WORKLOAD_NUM_VERTICES"

GRAPHS="${GRAPHS:-lscsr adj csr}"
echo "GRAPHS=$GRAPHS"

THREADS="${THREADS:-32}"
echo "THREADS=$THREADS"

ALGOS="${ALGOS:-bfs pr bc tc}"
echo "ALGOS=$ALGOS"

COMPACT_THRESHOLDS="${COMPACT_THRESHOLDS:-0.6}"
echo "COMPACT_THRESHOLDS=$COMPACT_THRESHOLDS"

NOW=$(date "+%s")
echo "NOW=$NOW"

CORES="${CORES:-0-31}"
echo "CORES=$CORES"

BFS_SRC="${BFS_SRC:-95}"
echo "BFS_SRC=$BFS_SRC"

BC_NUM_SRC="${BC_NUM_SRC:-64}"
echo "BC_NUM_SRC=$BC_NUM_SRC"

for threads in $THREADS; do
	for algo in $ALGOS; do
		for graph in $GRAPHS; do
			for compact_threshold in $COMPACT_THRESHOLDS; do
				TASKSET_CMD="taskset --cpu-list $CORES"
				EDIT_SCALABILITY_CMD="${BUILD}/microbench/edit-scalability --algo $algo --bfs-src $BFS_SRC --bc-num-src $BC_NUM_SRC --graph $graph --lscsr-compact-threshold $compact_threshold --ingest-threads $threads --algo-threads $threads --input-file $WORKLOAD_FILE --num-vertices $WORKLOAD_NUM_VERTICES"
				JOBN="$SCRATCH/data/$WORKLOAD/$algo/edit-scalability_t=${threads}_g=${graph}_c=${compact_threshold}_${NOW}"
				mkdir -p $(dirname $JOBN)

				CMD="$TASKSET_CMD $EDIT_SCALABILITY_CMD"
				echo "$JOBN: $CMD"
				$CMD 2>$JOBN.err 1>$JOBN.out
			done
		done
	done
done
