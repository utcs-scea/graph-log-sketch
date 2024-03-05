#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

echo "Make sure this is run from the root of the source directory"

# unset these variables if running on TACC or specify manually
#export TACC=true
#export HOST_THREADS=128

#GRAPH_SCALE_0="/scratch/09601/pkenney/pando-workflow4-galois/data/wf4_100_10000_200000_15000000_0.csv"
GRAPH_SCALE_0="data/scale1"
RRR_SETS_SCALE_0="2111000"
INFLUENTIAL_THRESHOLD_SCALE_0="0"
GRAPH_NAME_SCALE_0="scale1"
HOSTS_SCALE_0="1"
TIME_SCALE_0="0:30:00"

#GRAPH_SCALE_1="/scratch/09601/pkenney/pando-workflow4-galois/data/wf4_100_20000_400000_30000000_0.csv"
GRAPH_SCALE_1="data/scale2"
RRR_SETS_SCALE_1="4222000"
INFLUENTIAL_THRESHOLD_SCALE_1="0"
GRAPH_NAME_SCALE_1="scale2"
HOSTS_SCALE_1="1"
TIME_SCALE_1="0:30:00"

#GRAPH_SCALE_2="/scratch/09601/pkenney/pando-workflow4-galois/data/wf4_100_40000_800000_60000000_0.csv"
GRAPH_SCALE_2="data/scale3"
RRR_SETS_SCALE_2="8444000"
INFLUENTIAL_THRESHOLD_SCALE_2="0"
GRAPH_NAME_SCALE_2="scale3"
HOSTS_SCALE_2="1"
TIME_SCALE_2="0:30:00"

PROCS_0="8"
PROCS_1="16"
PROCS_2="32"

# Handle graph scale 0
export HOSTS="${HOSTS_SCALE_0}"
export TIME="${TIME_SCALE_0}"
export GRAPH_DIR="${GRAPH_SCALE_0}"
export GRAPH_NAME="${GRAPH_NAME_SCALE_0}"
export RRR_SETS="${RRR_SETS_SCALE_0}"
export INFLUENTIAL_THRESHOLD="${INFLUENTIAL_THRESHOLD_SCALE_0}"

export PROCS="${PROCS_0}" # iter1, iter3
bash scripts/run.sh       # 13min, 15min
export PROCS="${PROCS_1}"
bash scripts/run.sh # 8min, 11min
export PROCS="${PROCS_2}"
bash scripts/run.sh # 6min, 10min

# Handle graph scale 1
export HOSTS="${HOSTS_SCALE_1}"
export TIME="${TIME_SCALE_1}"
export GRAPH_DIR="${GRAPH_SCALE_1}"
export GRAPH_NAME="${GRAPH_NAME_SCALE_1}"
export RRR_SETS="${RRR_SETS_SCALE_1}"
export INFLUENTIAL_THRESHOLD="${INFLUENTIAL_THRESHOLD_SCALE_1}"

export PROCS="${PROCS_0}"
bash scripts/run.sh # 25min, 31min
export PROCS="${PROCS_1}"
bash scripts/run.sh # 14min, 19min
export PROCS="${PROCS_2}"
bash scripts/run.sh # 10min, 15min

# Handle graph scale 2
export HOSTS="${HOSTS_SCALE_2}"
export TIME="${TIME_SCALE_2}"
export GRAPH_DIR="${GRAPH_SCALE_2}"
export GRAPH_NAME="${GRAPH_NAME_SCALE_2}"
export RRR_SETS="${RRR_SETS_SCALE_2}"
export INFLUENTIAL_THRESHOLD="${INFLUENTIAL_THRESHOLD_SCALE_2}"

export PROCS="${PROCS_0}"
bash scripts/run.sh # 48min, 58min
export PROCS="${PROCS_1}"
bash scripts/run.sh # 27min, 33min
export PROCS="${PROCS_2}"
bash scripts/run.sh # 16min, 22min
