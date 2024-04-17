#!/bin/sh

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

EXEC=$1
INPUT=$2
SET=$3
QUEUE=$4
PART=$5
ALGO=$6
POSTFIX=$7
POSTFIX_FLAG=$8

echo EXEC:" $EXEC " INPUT:" $INPUT " SET:"$SET " QUEUE:"$QUEUE " PART:"$PART " ALGO:" $ALGO " POSTFIX:" $POSTFIX " POSTFIX FLAG:" $POSTFIX_FLAG

# Remove " from the tail
SET="${SET%\"}"
# Remove " from the head
SET="${SET#\"}"

for task in $SET; do
	IFS=","
	set $task
	cp run_stampede.template.sbatch.memcached run_stampede.sbatch
	#cp run_stampede.template.sbatch run_stampede.sbatch

	sed -i "2i#SBATCH -t $2" run_stampede.sbatch
	sed -i "2i#SBATCH -p $QUEUE" run_stampede.sbatch
	sed -i "2i#SBATCH -N $1 -n $1" run_stampede.sbatch
	#sed -i "2i#SBATCH --ntasks-per-node 1" run_stampede.sbatch
	sed -i "2i#SBATCH -o ${EXEC}_${INPUT}_${PART}_${1}_%j${POSTFIX}.out" run_stampede.sbatch
	sed -i "2i#SBATCH -J ${EXEC}_${INPUT}_${PART}_${1}${POSTFIX}" run_stampede.sbatch
	threads=48

	if [[ $QUEUE == "normal" || $QUEUE == "development" || $QUEUE == "large" || $QUEUE == "long" ]]; then
		threads=272
	fi

	echo "CPU-only " $EXEC $INPUT $PART $1 $threads $ALGO $POSTFIX ${EXEC}_${INPUT}_${PART}_${1}${POSTFIX} $POSTFIX_FLAG
	#source run_stampede.sbatch $EXEC $INPUT $PART $1 $threads $ALGO $POSTFIX $POSTFIX_FLAG
	sbatch run_stampede.sbatch $EXEC $INPUT $PART $1 $threads $ALGO "$POSTFIX" "$POSTFIX_FLAG"
	#rm run_stampede.sbatch
done
