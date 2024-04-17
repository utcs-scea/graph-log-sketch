# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

EXECS=("connected-components-cli-dist")

# Format: list("#-of-hosts,time")

#SET="4,03:00:00 8,03:00:00 16,03:00:00 32,03:00:00"
#SET="16,00:30:00"
SET="2,01:30:00"
#SET="4,02:30:00 8,02:30:00 16,02:30:00"
#SET="4,00:40:00 8,00:40:00 16,00:40:00 32,00:40:00"
#SET="16,05:00:00"
#SET=" 16,02:00:00 32,02:00:00"
#SET="4,01:00:00 8,01:00:00"
#SET="4,01:00:00 8,01:00:00 16,01:00:00 32,01:00:00 64,01:00:00"
#SET="4,01:00:00 32,01:00:00"
#SET="8,01:00:00"
#SET="32,02:00:00"
#SET="32,01:00:00"
#SET="16,01:10:00 32,01:10:00"

# Format: (input-graph;\"${SET}\"")

INPUTS=("twitter40;\"${SET}\"")

QUEUE=skx-normal
#QUEUE=skx-large
#QUEUE=skx-dev
#QUEUE=normal
#QUEUE=development#PARTS=( "oec" "cvc" )
#PARTS=( "oec" "cvc" )
#PARTS=( "oec" "cvc" )
#PARTS=( "oec" )
#PARTS=( "cvc" )

#PARTS=( "oec" "cvc" )
#PARTS=( "blocked-oec" )
PARTS=("blocked-cvc")
#PARTS=( "random-oec" )
#PARTS=( "random-oec" )
#PARTS=( "random-cvc" )

GRAPH_TYPES=( "lscsr" "lccsr" "adj" )

for j in "${INPUTS[@]}"; do
	IFS=";"
	set $j
	for i in "${EXECS[@]}"; do
		for p in "${PARTS[@]}"; do
      for g in "${GRAPH_TYPES[@]}"; do
        echo "./run_stampede_all.sh ${i} ${1} ${2} $QUEUE $p $a $g"
        ./run_ls6_all.sh ${i} ${1} ${2} $QUEUE $p $a $g |& tee -a jobs
      done
		done
	done
done
