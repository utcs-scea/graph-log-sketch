#!/bin/bash
#SBATCH --mail-user=hochan@utexas.edu
#SBATCH --mail-type=begin
#SBATCH --mail-type=fail
#SBATCH --mail-type=end

EXEC_NAME=${1}
INPUT=${2}
PART=${3}
NUM_HOSTS=${4}
THREADS=${5}

execdir=$(pwd)
EXEC=${execdir}/binaries/${EXEC_NAME}
OUTPUT=${execdir}/${EXEC_NAME}

# Create directory structures
PPOLICY_OUT_DIR="${EXEC_NAME}_${INPUT}/${PART}/${NUM_HOSTS}"
mkdir -p $PPOLICY_OUT_DIR

outname=${EXEC_NAME}_${INPUT}_${PART}_${NUM_HOSTS}_${SLURM_JOB_ID}.out
basename=${EXEC_NAME}_${INPUT}_${PART}_${SLURM_NNODES}_${SLURM_JOB_ID}
statname=outputs/${basename}.stats
