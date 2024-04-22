#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

if [ -z ${WORKLOAD+x} ]; then
	echo "WORKLOAD is unset"
	exit 1
fi

# Outputs an edit scalability script that can then be submitted to the SLURM queue.
# WORKLOAD is the base name of the workload (e.g. friendster_s0).
WORKLOAD_FILE="${WORKLOAD_FILE:-$SCRATCH/graphs/$WORKLOAD.bel}"
# check that WORKLOAD_FILE exists
if [ ! -f $WORKLOAD_FILE ]; then
	echo "WORKLOAD_FILE does not exist: $WORKLOAD_FILE"
	exit 1
fi

# STARTUPS is the percentage of the graph ingested before batching.
STARTUPS="${STARTUPS:-0}"
echo "STARTUPS=$STARTUPS"

# NUM_BATCHES is the number of batches the edges are split into.
NUM_BATCHES="${NUM_BATCHES:-10}"
echo "NUM_BATCHES=$NUM_BATCHES"

# ALGOS is a list of algorithms to run.
ALGOS="${ALGOS:-bfs tc}"
echo "ALGOS=$ALGOS"

# BFS_SRC is the source vertex for BFS. Does nothing if "bfs" is not in ALGOS.
BFS_SRC="${BFS_SRC:-101}"
echo "BFS_SRC=$BFS_SRC"

# BC_NUM_SRC is the number of source vertices used for betweenness centrality.
# Does nothing if "bc" is not in ALGOS.
BC_NUM_SRC="${BC_NUM_SRC:-2048}"
echo "BC_NUM_SRC=$BC_NUM_SRC"

# THREADS is a list of thread counts to run.
THREADS="${THREADS:-128}"
echo "THREADS=$THREADS"

# GRAPHS is a list of graph representations to use.
GRAPHS="${GRAPHS:-lscsr adj csr}"
echo "GRAPHS=$GRAPHS"

COMPACT_THRESHOLDS="${COMPACT_THRESHOLDS:-0.5}"
echo "COMPACT_THRESHOLDS=$COMPACT_THRESHOLDS"

BUILD="${BUILD:-$WORK/scea/graph-log-sketch/build}"
echo "BUILD=$BUILD"

ENV="${ENV:-${WORK}/scea/graph-log-sketch/scripts/tacc_env.sh}"
echo "ENV=$ENV"

OUT_DIR="${OUT_DIR:-$SCRATCH/scea/graph-log-sketch/data/$WORKLOAD}"
echo "OUT_DIR=$OUT_DIR"

# SLURM options
QUEUE=${QUEUE:-normal}
echo "QUEUE=$QUEUE"

TIME=${TIME:-6:00:00}
echo "TIME=$TIME"

NOW="$(date +%s)"
echo "NOW=$NOW"

TOTAL=0
for g in $GRAPHS; do
	for s in $STARTUPS; do
		for b in $NUM_BATCHES; do
			for a in $ALGOS; do
				for c in $COMPACT_THRESHOLDS; do
					for t in $THREADS; do
						TOTAL=$((TOTAL + 1))
					done
				done
			done
		done
	done
done

echo
echo "Total jobs: $TOTAL"

read -p "Are you sure? " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
	[[ "$0" = "$BASH_SOURCE" ]] && exit 1 || return 1
fi

for g in $GRAPHS; do
	for s in $STARTUPS; do
		for b in $NUM_BATCHES; do
			for a in $ALGOS; do
				for c in $COMPACT_THRESHOLDS; do
					for t in $THREADS; do
						JOBN="${OUT_DIR}/g$g/s${s}/b${b}/a${a}/t${t}/c${c}/${NOW}"

						# Stripe the CPUs across L3 caches.
						CORES=""
						for i in $(seq 0 $((t - 1))); do
							CORES+=","
							CORES+="$((8 * ($i % 16) + ($i / 16)))"
						done
						CORES=${CORES:1}

						SRUN_CMD="srun --exclusive --mem=0"
						TASKSET_CMD="taskset --cpu-list $CORES"
						EDIT_SCALABILITY_CMD="${BUILD}/microbench/edit-scalability-large -f $WORKLOAD_FILE -b $b -s $s -a $a -g $g --lscsr-compact-threshold=$c --ingest-threads $t --algo-threads $t --bfs-src $BFS_SRC --bc-num-src $BC_NUM_SRC"

						echo "Submitting job: $JOBN"

						sbatch <<EOF
#!/bin/bash
#SBATCH -J $JOBN
#SBATCH -o $JOBN.out
#SBATCH -e $JOBN.err
#SBATCH -t $TIME
#SBATCH -N 1
#SBATCH -n 1
#SBATCH -c 128
#SBATCH --exclusive
#SBATCH --mail-type=none
#SBATCH --mail-user=meyer.zinn@utexas.edu
#SBATCH -p $QUEUE

echo "SLURM_JOB_ID=\$SLURM_JOB_ID"
echo "WORKLOAD=$WORKLOAD"
echo "WORKLOAD_FILE=$WORKLOAD_FILE"
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
		done
	done
done

# ensure that repeated invocations don't have the same timestamp
sleep 1
