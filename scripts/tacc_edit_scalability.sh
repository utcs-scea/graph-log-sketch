#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

# Outputs an edit scalability script that can then be submitted to the SLURM queue.
# WORKLOAD is the base name of the workload (e.g. friendster_randomized_20).

# ALGOS is a list of algorithms to run.
ALGOS="${ALGOS:-bfs tc}"
# BFS_SRC is the source vertex for BFS. Does nothing if BFS is not in ALGOS.
BFS_SRC="${BFS_SRC:-101}"
BC_NUM_SRC="${BC_NUM_SRC:-512}"
# THREADS is a list of thread counts to run.
THREADS="${THREADS:-128}"
# GRAPHS is a list of graph representations to use.
GRAPHS="${GRAPHS:-lscsr adj csr}"
LSCSR_COMPACT_THRESHOLD="${LSCSR_COMPACT_THRESHOLD:-0.6}"

ENV="${ENV:-${WORK}/scea/graph-log-sketch/scripts/tacc_env.sh}"

BUILD="${BUILD:-$WORK/scea/graph-log-sketch/build}"

WORKLOAD_FILE="${WORKLOAD_FILE:-$SCRATCH/graphs/$WORKLOAD.txt}"
# check that WORKLOAD_FILE exists
if [ ! -f $WORKLOAD_FILE ]; then
	echo "WORKLOAD_FILE does not exist: $WORKLOAD_FILE"
	exit 1
fi

# Count the number of vertices in the workload, if not already set.
if [ -z "$WORKLOAD_NUM_VERTICES" ]; then
	WORKLOAD_NUM_VERTICES="$($BUILD/scripts/count-batched -q -v $WORKLOAD_FILE)"
fi

OUT_DIR="${OUT_DIR:-$SCRATCH/scea/graph-log-sketch/data/$WORKLOAD}"

# SLURM options
QUEUE=${QUEUE:-normal}
TIME=${TIME:-00:45:00}

NOW="$(date +%s)"

for a in $ALGOS; do
	for t in $THREADS; do
		for g in $GRAPHS; do
			JOBN="${OUT_DIR}/${a}/edit-scalability_t=${t}_g=${g}_${NOW}"

			# Stripe the CPUs across L3 caches.
			CORES=""
			for i in $(seq 0 $((t - 1))); do
				CORES+=","
				CORES+="$((8 * ($i % 16) + ($i / 16)))"
			done
			CORES=${CORES:1}

			SRUN_CMD="srun --exclusive --mem=0"
			TASKSET_CMD="taskset --cpu-list $CORES"
			EDIT_SCALABILITY_CMD="${BUILD}/microbench/edit-scalability --algo $a --bfs-src $BFS_SRC --bc-num-src $BC_NUM_SRC --graph $g --lscsr-compact-threshold 0.3 --ingest-threads $t --algo-threads $t --input-file $WORKLOAD_FILE --num-vertices $WORKLOAD_NUM_VERTICES"

			echo "Submitting job: $JOBN"

			sbatch <<EOF
#!/bin/bash
#SBATCH -J $JOBN
#SBATCH -o $JOBN.out
#SBATCH -e $JOBN.err
#SBATCH -t $TIME
#SBATCH -N 1
#SBATCH -n 1
#SBATCH --exclusive
#SBATCH --mail-type=none
#SBATCH --mail-user=meyer.zinn@utexas.edu
#SBATCH -p $QUEUE

echo "SLURM_JOB_ID=\$SLURM_JOB_ID"
echo "WORKLOAD=$WORKLOAD"
echo "WORKLOAD_FILE=$WORKLOAD_FILE"
echo "WORKLOAD_NUM_VERTICES=$WORKLOAD_NUM_VERTICES"
echo "ENV=$ENV"
echo "BUILD=$BUILD"
echo "OUT_DIR=$OUT_DIR"
echo "QUEUE=$QUEUE"
echo "TIME=$TIME"
echo "NOW=$NOW"

module purge
. $ENV

mkdir -p $(dirname $JOBN)
echo "$SRUN_CMD $TASKSET_CMD $EDIT_SCALABILITY_CMD"
$SRUN_CMD $TASKSET_CMD $EDIT_SCALABILITY_CMD
EOF
		done
	done
done

# ensure that repeated invocations don't have the same timestamp
sleep 1
