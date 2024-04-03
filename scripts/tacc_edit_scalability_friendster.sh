#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

GRAPH_PATH="${GRAPH_PATH:-$SCRATCH/graphs/friendster_randomized_25.txt}"
GRAPH_NUM_VERTICES="${GRAPH_NUM_VERTICES:-124836180}"

# required sbatch parameters

# note that TIME is in the format H:MM:SS

# note that the bash paradigm `HOSTS="${HOSTS:-<default>}`
# means: set the env var HOSTS equal to $HOSTS if HOSTS is
# already set, if it is not set then set `HOSTS=default`
HOSTS="${HOSTS:-1}"
PROCS="${PROCS:-1}"
TIME="${TIME:-1:00:00}"
QUEUE="${QUEUE:-normal}"
JOBS="${JOBS:-edit-scalability}"

ENV=${WORK}/scea/graph-log-sketch/scripts/tacc_env.sh

# These variables are not necessary but recommended for ease of use
# The data directory is helpful for storing outputs and is recommended
# but not necessary
BUILD="${BUILD:-$WORK/scea/graph-log-sketch/build}"
DATA="${DATA:-$SCRATCH/scea/graph-log-sketch/data/friendster}"

# Create the data directory, if it does not yet exist:
mkdir -p $DATA

# print statements to validate input, can and should
# be extended with application parameters
echo $HOSTS
echo $PROCS
echo $THREADS
echo $TIME
echo $DATA
echo $QUEUE
echo $JOBN

for algo in bfs tc; do
	mkdir -p "${DATA}/$algo"
	for nthreads in 8 16 32 64; do
		for graph in lscsr lccsr adj; do
			# JOBN should be parameterized with application parameters as well
			# possibly time as well time prevent conflicts and overwriting
			JOBN=${DATA}/$algo/${JOBS}_t=${nthreads}_g=${graph}
			echo "Submitting job: $JOBN"

			# start of job that runs on the supercomputer
			sbatch <<-EOT
				#!/bin/bash

				# special arguments passed to sbatch here instead of by command line
				# the mail arguments are optional and are just there to send you email
				# notifications when your jobs are scheduled and complete
				#SBATCH -J ${JOBN}
				#SBATCH -o ${JOBN}.out
				#SBATCH -e ${JOBN}.err
				#SBATCH -t ${TIME}
				#SBATCH -N ${HOSTS}
				#SBATCH -n ${PROCS}
				#SBATCH --mail-type=none
				#SBATCH --mail-user=meyer.zinn@utexas.edu
				#SBATCH -p ${QUEUE}

				# ensure the proper runtime environment is set
				module purge
				. ${ENV}

				# actually run the equivalent of $\(mpirun) the $\(--) ensures arguments
				# are passed to your executable and not $\(ibrun)
				ibrun -- ${BUILD}/microbench/edit-scalability \
				                --algo $algo \
				                --bfs-src 101 \
				                --graph $graph \
				                --ingest-threads $nthreads \
				                --algo-threads $nthreads \
				                --input-file $GRAPH_PATH \
				                --num-vertices $GRAPH_NUM_VERTICES
			EOT
		done
	done
done
