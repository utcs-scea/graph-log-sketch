#!/bin/bash

HOSTS="${HOSTS:-32}"
PROCD=$(echo "128 * ${HOSTS}" | bc)
PROCS="${PROCS:-${PROCD}}"
TIME="${TIME:-2:00:00}"
OUTS="${OUTS:-bfs}"
BUILD="${BUILD:-rbuild-gls}"
QUEUE="${QUEUE:-normal}"
RSEED="${RSEED:-0}"
SCALE="${SCALE:-20}"
EDGER="${EDGER:-16}"
RANDA="${RANDA:-57}"
RANDB="${RANDB:-19}"
RANDC="${RANDC:-19}"
JOBS="${JOBN:-job-bfs}"
JOBN=${JOBS}_${SCALE}_${EDGER}_${RANDA}_${RANDB}_${RANDC}
OUTF=${OUTS}_${SCALE}_${EDGER}_${RANDA}_${RANDB}_${RANDC}
ENV=~/Research/PANDO/pando-workflows/workflows/scripts/tacc_env.sh


echo $HOSTS
echo $PROCS
echo $TIME
echo $JOBN
echo $OUTF
echo $BUILD
echo $QUEUE
echo $RSEED
echo $SCALE
echo $EDGER
echo $RANDA
echo $RANDB
echo $RANDC

sbatch << EOT
#!/bin/bash

#SBATCH -J ${JOBN}
#SBATCH -o ${JOBN}.out
#SBATCH -e ${JOBN}.err
#SBATCH -t ${TIME}
#SBATCH -N ${HOSTS}
#SBATCH -n ${PROCS}
#SBATCH --mail-type=all
#SBATCH --mail-user=adityaatewari@utexas.edu
#SBATCH -p ${QUEUE}

module purge
. ${ENV}

ibrun ${BUILD}/gen-rand-large \
  ${RSEED} \
  ${SCALE} \
  ${EDGER} \
  ${RANDA} \
  ${RANDB} \
  ${RANDC} \
  ${OUTF}
EOT
