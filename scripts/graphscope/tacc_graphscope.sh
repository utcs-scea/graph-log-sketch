#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

HOSTS="${HOSTS:-1}"
PROCD=$(echo "2" | bc)
PROCS="${PROCS:-${PROCD}}"
TIME="${TIME:-1:00:00}"
QUEUE="${QUEUE:-normal}"
RUN_SCRIPT="${SRC:-/scratch/09601/pkenney/run.py}"
DATA="${DATA:-/scratch/09601/pkenney/friendster}"
JOBS="${JOBN:-graphscope-benchmark}"
OUTS="${OUTS:-graphscope}"

GRAPH_NAME="${GRAPH_NAME:-friendster}"
PARTITIONS="${PARTITIONS:-2}"

JOBN=${DATA}/${JOBS}_${GRAPH_NAME}

echo $HOSTS
echo $PROCS
echo $TIME
echo $DATA
echo $QUEUE
echo $JOBN

sbatch <<EOT
#!/bin/bash

#SBATCH -J ${JOBN}
#SBATCH -o ${JOBN}.out
#SBATCH -e ${JOBN}.err
#SBATCH -t ${TIME}
#SBATCH -N ${HOSTS}
#SBATCH -n ${PROCS}
#SBATCH --mail-type=all
#SBATCH --mail-user=pkenney@utexas.edu
#SBATCH -p ${QUEUE}

python3 ${RUN_SCRIPT} ${DATA} ${PARTITIONS}

EOT
