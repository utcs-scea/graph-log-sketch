#!/bin/sh

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

EXEC=$1
INPUT=$2
SET=$3 # hosts + time
QUEUE=$4
PART=$5
GRAPH_TYPE=$6

echo EXEC:" $EXEC " INPUT:" $INPUT " SET:"$SET " QUEUE:"$QUEUE " PART:"$PART" GRAPH_TYPE:" $GRAPH_TYPE

# Remove " from the tail
SET="${SET%\"}"
# Remove " from the head
SET="${SET#\"}"

for task in $SET; do
	IFS=","
	set $task
	cp run_ls6.template.sbatch run_ls6.sbatch

	sed -i "2i#SBATCH -t $2" run_ls6.sbatch
	sed -i "2i#SBATCH -p $QUEUE" run_ls6.sbatch
	sed -i "2i#SBATCH -N $1 -n $1" run_ls6.sbatch
	#sed -i "2i#SBATCH --ntasks-per-node 1" run_ls6.sbatch
	sed -i "2i#SBATCH -o ${EXEC}_${INPUT}_${PART}_${1}_%j.out" run_ls6.sbatch
	sed -i "2i#SBATCH -J ${EXEC}_${INPUT}_${PART}_${1}" run_ls6.sbatch
	threads=48

	if [[ $QUEUE == "normal" || $QUEUE == "development" || $QUEUE == "large" || $QUEUE == "long" ]]; then
		threads=272
	fi

	echo "CPU-only " $EXEC $INPUT $PART $1 $threads ${EXEC}_${INPUT}_${PART}_${1}_${GRAPH_TYPE}
	# sbatch run_ls6.sbatch $EXEC $INPUT $PART $1 $threads ${GRAPH_TYPE}
	source run_ls6.sbatch $EXEC $INPUT $PART $1 $threads ${GRAPH_TYPE}
	#rm run_ls6.sbatch
done
