#!/bin/bash

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

echo "Make sure this is run from the root of the source directory"
echo "Must be run serially since these scripts suck and will conflict"

export TIME="0:15:00"
export GRAPH_NAME="scale1"
export COMMERCIAL_PARAMETERS="2000000 100 1000 10000 200000 30 50 70 0"
export CYBER_PARAMETERS="500000 2"
export SOCIAL_PARAMETERS="2000000 3"
export USES_PARAMETERS="2000000 500000 4"
#bash scripts/tacc_dataset.sh

export TIME="0:15:00"
export GRAPH_NAME="scale2"
export COMMERCIAL_PARAMETERS="4000000 200 2000 20000 400000 30 50 70 0"
export CYBER_PARAMETERS="1000000 2"
export SOCIAL_PARAMETERS="4000000 3"
export USES_PARAMETERS="4000000 1000000 4"
bash scripts/tacc_dataset.sh

export TIME="0:30:00"
export GRAPH_NAME="scale3"
export COMMERCIAL_PARAMETERS="8000000 400 4000 40000 800000 30 50 70 0"
export CYBER_PARAMETERS="2000000 2"
export SOCIAL_PARAMETERS="8000000 3"
export USES_PARAMETERS="8000000 2000000 4"
#bash scripts/tacc_dataset.sh
