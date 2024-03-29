#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

SCRIPTS="${SCRIPTS:-scripts/generate}"
DATA="${DATA:-data}"

GRAPH_NAME="${GRAPH_NAME:-graph}"
COMMERCIAL_PARAMETERS="${COMMERCIAL_PARAMETERS:-2000000 100 1000 10000 200000 30 50 70 0}"
CYBER_PARAMETERS="${CYBER_PARAMETERS:-500000 2}"
SOCIAL_PARAMETERS="${SOCIAL_PARAMETERS:-2000000 3}"
USES_PARAMETERS="${USES_PARAMETERS:-2000000 500000 4}"

ln -s ${DATA}/workflow4-dataset-main/wmd.data.csv wmd.data.csv
ln -s ${DATA}/workflow4-dataset-main/netflow_day_3.csv netflow_day_3.csv
python ${SCRIPTS}/commercial.py ${COMMERCIAL_PARAMETERS}
python ${SCRIPTS}/cyber.py ${CYBER_PARAMETERS}
python ${SCRIPTS}/social.py ${SOCIAL_PARAMETERS}
python ${SCRIPTS}/uses.py ${USES_PARAMETERS}
python ${SCRIPTS}/extract_nodes.py . nodes.csv

mkdir -p ${DATA}/${GRAPH_NAME}
mv commercial.csv ${DATA}/${GRAPH_NAME}
mv cyber.csv ${DATA}/${GRAPH_NAME}
mv social.csv ${DATA}/${GRAPH_NAME}
mv uses.csv ${DATA}/${GRAPH_NAME}
mv nodes.csv ${DATA}/${GRAPH_NAME}
