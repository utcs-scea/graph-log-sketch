#!/bin/bash
#SBATCH --mail-user=hochan@utexas.edu
#SBATCH --mail-type=begin
#SBATCH --mail-type=fail
#SBATCH --mail-type=end
#SBATCH -A ASC23036

EXEC_NAME=${1}
INPUT=${2}
NUM_HOSTS=${3}
THREADS=${4}
GRAPH_TYPE=${5}

execdir=$(pwd)
EXEC=${execdir}/bin/${EXEC_NAME}
OUTPUT=${execdir}/${EXEC_NAME}

# Create directory structures
E2E_OUT_DIR="e2e_${EXEC_NAME}_${INPUT}/${GRAPH_TYPE}/${NUM_HOSTS}"
mkdir -p $E2E_OUT_DIR

outname=${EXEC_NAME}_${INPUT}_${NUM_HOSTS}_${SLURM_JOB_ID}.out
basename=${EXEC_NAME}_${INPUT}_${SLURM_NNODES}_${SLURM_JOB_ID}
statname=outputs/${basename}.stats

RUN=mpirun
mpirun --version

set -x #echo on
echo $RUN --bind-to none -np $NUM_HOSTS $EXEC $INPUT 0 10
$RUN --bind-to none -np $NUM_HOSTS $EXEC $INPUT 0 10

mv total_e2e_*.out $E2E_OUT_DIR

set +x #echo off

# give permissions to output files
#chmod 664 ${outname}
#chmod 664 $statname

succeed=$(ls ${statname}.* 2>/dev/null | wc -l)
if [[ "$succeed" != "0" ]]; then
	mv ${outname} outputs
else
	mv ${outname} fails
fi

echo "Algorithm: " $EXEC_NAME
echo "Input: " $INPUT
echo "Number of nodes: " $SLURM_NNODES
comment
