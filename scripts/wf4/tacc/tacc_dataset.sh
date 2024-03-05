#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

HOSTS="${HOSTS:-1}"
PROCD=$(echo "1" | bc)
PROCS="${PROCS:-${PROCD}}"
TIME="${TIME:-0:15:00}"
QUEUE="${QUEUE:-normal}"
SRC="${SRC:-/scratch/09601/pkenney/pando-workflow4-galois}"
SCRIPTS="${SCRIPTS:-/scratch/09601/pkenney/pando-workflow4-galois/scripts/generate}"
DATA="${DATA:-/scratch/09601/pkenney/pando-workflow4-galois/data}"
JOBS="${JOBN:-wf4-dataset}"
OUTS="${OUTS:-wf4}"

GRAPH_NAME="${GRAPH_NAME:-graph}"
COMMERCIAL_PARAMETERS="${COMMERCIAL_PARAMETERS:-2000000 100 1000 10000 200000 30 50 70 0}"
CYBER_PARAMETERS="${CYBER_PARAMETERS:-500000 2}"
SOCIAL_PARAMETERS="${SOCIAL_PARAMETERS:-2000000 3}"
USES_PARAMETERS="${USES_PARAMETERS:-2000000 500000 4}"

JOBN=${DATA}/${JOBS}_${GRAPH_NAME}
OUTF=${DATA}/${OUTS}_${GRAPH_NAME}
ENV=/scratch/09601/pkenney/pando-workflow4-galois/scripts/tacc_env.sh

echo $HOSTS
echo $PROCS
echo $TIME
echo $DATA
echo $QUEUE
echo $JOBN
echo $OUTF

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

module purge
. ${ENV}

ln -s ${DATA}/workflow4-dataset-main/wmd.data.csv ${SRC}/wmd.data.csv
ln -s ${DATA}/workflow4-dataset-main/netflow_day_3.csv ${SRC}/netflow_day_3.csv
python ${SCRIPTS}/commercial.py ${COMMERCIAL_PARAMETERS}
python ${SCRIPTS}/cyber.py ${CYBER_PARAMETERS}
python ${SCRIPTS}/social.py ${SOCIAL_PARAMETERS}
python ${SCRIPTS}/uses.py ${USES_PARAMETERS}
python ${SCRIPTS}/extract_nodes.py ${SRC} nodes.csv

mkdir -p ${DATA}/${GRAPH_NAME}
mv commercial.csv ${DATA}/${GRAPH_NAME}
mv cyber.csv ${DATA}/${GRAPH_NAME}
mv social.csv ${DATA}/${GRAPH_NAME}
mv uses.csv ${DATA}/${GRAPH_NAME}
mv nodes.csv ${DATA}/${GRAPH_NAME}

EOT
