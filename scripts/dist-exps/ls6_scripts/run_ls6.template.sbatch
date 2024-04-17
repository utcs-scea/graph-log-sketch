#!/bin/bash
#SBATCH --mail-user=hochan@utexas.edu
#SBATCH --mail-type=begin
#SBATCH --mail-type=fail
#SBATCH --mail-type=end
#SBATCH -A Galois

source /scratch/06398/hochan/miniconda3/etc/profile.d/conda.sh
conda init bash
conda activate katana-dev
module purge
module load ooops
set_io_param_batch $SLURM_JOBID 0 low

module list

GET_INPUT_PATH() {
	if [[ "$1" =~ .*"road-USA".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/road-USA-symmetric/"
	elif [[ "$1" =~ .*"friendster".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/friendster.symmetric"
	elif [[ "$1" =~ .*"twitter40".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/twitter40-symmetric"
	elif [[ "$1" =~ .*"twitter50".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/twitter50-symmetric"
	elif [[ "$1" =~ .*"clueweb12".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/clueweb-symmetric/"
	elif [[ "$1" =~ .*"kron30".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/kron30-symmetric"
	elif [[ "$1" =~ .*"wdc".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/wdc12-symmetric/"
	elif [[ "$1" =~ .*"road-europe".* ]]; then
		echo "/scratch/06398/hochan/inputs/katana/road-europe.w.symmetric/"
		#echo "gs://katana-demo-datasets/rdg-datasets/24b308/clueweb12"
		#echo "s3://property-graphs/katana/clueweb12_symmetric/"
		#echo "gs://katana-demo-datasets/rdg-datasets/v3/clueweb12"
	else
		echo "Input should be provided."
		exit 1
	fi
}

EXEC_NAME=${1}
INPUT=${2}
PART=${3}
NUM_HOSTS=${4}
THREADS=${5}
ALGO=${6}
POSTFIX=${7}
POSTFIX_FLAG=${8}

execdir=$(pwd)
EXEC=${execdir}/binaries/${EXEC_NAME}
OUTPUT=${execdir}/${EXEC_NAME}

outname=${OUTPUT}_${INPUT}_${PART}_${NUM_HOSTS}_${SLURM_JOB_ID}${POSTFIX}.out
basename=${EXEC_NAME}_${INPUT}_${PART}_${SLURM_NNODES}_${SLURM_JOB_ID}${POSTFIX}

if [[ $EXEC_NAME =~ .*"connected-component".* ]]; then
	if [[ ${ALGO} =~ .*"DistPush".* ]]; then
		basename+="_LabelProp"
	elif [[ ${ALGO} =~ .*"DistShortcuttingKVStore".* ]]; then
		basename+="_ShortcuttingKVStore"
	elif [[ ${ALGO} =~ .*"DistShortcuttingNodeKVStore".* ]]; then
		basename+="_ShortcuttingNodeKVStore"
	elif [[ ${ALGO} =~ .*"DistLabelPropKVStore".* ]]; then
		basename+="_LabelPropKVStore"
	elif [[ ${ALGO} =~ .*"DistPointerJumpingKVStore".* ]]; then
		basename+="_PointerJumpingKVStore"
	elif [[ ${ALGO} =~ .*"DistPointerJumpingNodeKVStore".* ]]; then
		basename+="_PointerJumpingNodeKVStore"
	elif [[ ${ALGO} =~ .*"DistSVKVStore".* ]]; then
		basename+="_SV"
	elif [[ ${ALGO} =~ .*"DistSVNodeKVStore".* ]]; then
		basename+="_SVNodeKVStore"
	elif [[ ${ALGO} =~ .*"DistAfforest".* ]]; then
		basename+="_Afforest"
	fi
fi

statname=outputs/${basename}.stats
inputpath=$(GET_INPUT_PATH $INPUT)
FLAGS+=" "$inputpath
FLAGS+=" --symmetricGraph"
FLAGS+=" "$ALGO
FLAGS+=$POSTFIX_FLAG
FLAGS+=" -partition=${PART}"
if [[ $EXEC_NAME =~ .*"louvain-clustering".* ]]; then
	FLAGS+=" --modularity_threshold_per_round=0.0001"
	FLAGS+=" --modularity_threshold_total=0.0001"
	FLAGS+=" --max_iterations=1000"
	FLAGS+=" --min_graph_size=100"
elif [[ $EXEC_NAME =~ .*"connected-component".* ]]; then
	FLAGS+=" -exec=Sync"
fi
#elif [[ $EXEC_NAME =~ .*"maximal".* ]]; then
#  FLAGS+=" --commit"
#fi
if [[ $INPUT =~ .*"friendster".* || $INPUT =~ .*"twitter".* || $INPUT =~ .*"road-europe".* ]]; then
	FLAGS+=" -edgePropertyName=value"
fi

#FLAGS+=" --variant_type=1"

RUN=mpirun
mpirun --version

#LD_PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision`

set -x #echo on
#        echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var1.1 --variant_type=1
#        PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS  -statFile=${execdir}/${statname}.var1.1 --variant_type=1
#        echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var1.2 --variant_type=1
#        PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS  -statFile=${execdir}/${statname}.var1.2 --variant_type=1
#        echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var1.3 --variant_type=1
#        PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS  -statFile=${execdir}/${statname}.var1.3 --variant_type=1

echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.1 --variant_type=2
PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.1 --variant_type=2
echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.2 --variant_type=2
PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.2 --variant_type=2
echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.3 --variant_type=2
PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var2.3 --variant_type=2

#echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var3.1 --variant_type=3
#PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS  -statFile=${execdir}/${statname}.var3.1 --variant_type=3
#echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var3.2 --variant_type=3
#PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS  -statFile=${execdir}/${statname}.var3.2 --variant_type=3
#echo PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS -statFile=${execdir}/${statname}.var3.3 --variant_type=3
#PRINT_PER_HOST_STATS=1 GALOIS_DO_NOT_BIND_THREADS=1 $RUN --bind-to none -np $NUM_HOSTS $EXEC -t=$THREADS $FLAGS  -statFile=${execdir}/${statname}.var3.3 --variant_type=3

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
