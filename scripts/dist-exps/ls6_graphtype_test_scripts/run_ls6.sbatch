#!/bin/bash
#SBATCH -J connected-components-cli-dist_twitter40_blocked-cvc_2
#SBATCH -o connected-components-cli-dist_twitter40_blocked-cvc_2_%j.out
#SBATCH -N 2 -n 2
#SBATCH -p skx-normal
#SBATCH -t 01:30:00
#SBATCH --mail-user=hochan@utexas.edu
#SBATCH --mail-type=begin
#SBATCH --mail-type=fail
#SBATCH --mail-type=end

EXEC_NAME=${1}
INPUT=${2}
PART=${3}
NUM_HOSTS=${4}
THREADS=${5}
GRAPH_TYPE=${6}

execdir=$(pwd)
EXEC=${execdir}/binaries/${EXEC_NAME}
OUTPUT=${execdir}/${EXEC_NAME}

# Create directory structures
E2E_OUT_DIR="e2e_${EXEC_NAME}_${INPUT}/${GRAPH_TYPE}/${NUM_HOSTS}"
mkdir -p $E2E_OUT_DIR

outname=${EXEC_NAME}_${INPUT}_${PART}_${NUM_HOSTS}_${SLURM_JOB_ID}.out
basename=${EXEC_NAME}_${INPUT}_${PART}_${SLURM_NNODES}_${SLURM_JOB_ID}
statname=outputs/${basename}.stats

<<comment
RUN=mpirun
mpirun --version

set -x #echo on
echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.1 --variant_type=2
PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.1 --variant_type=2
echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.2 --variant_type=2
PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.2 --variant_type=2
echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.3 --variant_type=2
PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.3 --variant_type=2

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
