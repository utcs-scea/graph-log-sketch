#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

BUILD="${BUILD:-/home/$(whoami)/scea/graph-log-sketch/build}"
echo "BUILD=$BUILD"

SCRATCH="${SCRATCH:-/var/local/$(whoami)}"
echo "SCRATCH=$SCRATCH"

echo "WORKLOAD=$WORKLOAD"
WORKLOAD_FILE="${WORKLOAD_FILE:-$SCRATCH/graphs/$WORKLOAD.bel}"
# check that WORKLOAD_FILE exists
if [ ! -f $WORKLOAD_FILE ]; then
	echo "WORKLOAD_FILE does not exist: $WORKLOAD_FILE"
	exit 1
fi
echo "WORKLOAD_FILE=$WORKLOAD_FILE"

if [-z ${NUM_BATCHES+x}]; then
	echo "NUM_BATCHES is unset"
	exit 1
fi

if [-z ${STARTUP+x}]; then
	echo "STARTUP is unset, which is probably an error. Set it explicitly to 0 if you want no startup time."
	exit 1
fi
echo "STARTUP=$STARTUP"

RESULTS_DIR="${RESULTS_DIR:-$SCRATCH/scea/graph-log-sketch/results/$WORKLOAD}"
mkdir -p $RESULTS_DIR
echo "RESULTS_DIR=$RESULTS_DIR"

GRAPHS="${GRAPHS:-lscsr adj csr}"
echo "GRAPHS=$GRAPHS"

ALGOS="${ALGOS:-bfs pr tc bc}"
echo "ALGOS=$ALGOS"

COMPACT_THRESHOLDS="${COMPACT_THRESHOLDS:-0.5}"
echo "COMPACT_THRESHOLDS=$COMPACT_THRESHOLDS"

NOW=$(date "+%s")
echo "NOW=$NOW"

CORES="${CORES:-0-31}"
echo "CORES=$CORES"

THREADS="${THREADS:-32}"
echo "THREADS=$THREADS"

BFS_SRC="${BFS_SRC:-95}"
echo "BFS_SRC=$BFS_SRC"

BC_NUM_SRC="${BC_NUM_SRC:-64}"
echo "BC_NUM_SRC=$BC_NUM_SRC"

for threads in $THREADS; do
	for algo in $ALGOS; do
		for graph in $GRAPHS; do
			for compact_threshold in $COMPACT_THRESHOLDS; do
				TASKSET_CMD="taskset --cpu-list $CORES"
				EDIT_SCALABILITY_CMD="${BUILD}/microbench/edit-scalability-large -f $WORKLOAD_FILE -a $algo -g $graph --ingest-threads $threads --algo-threads $threads --lscsr-compact-threshold $compact_threshold --bfs-src $BFS_SRC --bc-num-src $BC_NUM_SRC"
				JOBN="$RESULTS_DIR/$algo/edit-scalability_t=${threads}_g=${graph}_c=${compact_threshold}_${NOW}"
				mkdir -p $(dirname $JOBN)

				CMD="$TASKSET_CMD $EDIT_SCALABILITY_CMD"
				echo "$JOBN: $CMD"
				$CMD 2>$JOBN.err 1>$JOBN.out
			done
		done
	done
done
