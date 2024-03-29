# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import os
import sys
import random
import numpy as np

# command parameters: # <num_servers> <seed>
#
# 500000 2
#

CyberIn  = open("netflow_day_3.csv", "r")
CyberOut = open("cyber.csv", "w")

CyberOut.write(CyberIn.readline())     # copy first three lines
CyberOut.write(CyberIn.readline())
CyberOut.write(CyberIn.readline())

num_servers = int(sys.argv[1])
np.random.seed(int(sys.argv[2]))

########### PRINT SOCIAL RECORDS ##########

for S1 in range(0, num_servers):
  num_connections = int( np.random.exponential(20.0) )

  for i in range(0, num_connections):
    S2 = int( np.random.random() * num_servers )
    while S1 == S2: S2 = int( np.random.random() * num_servers )

    line = CyberIn.readline()                    # use next netflow record for properties
    fields = line[:-1].split(',')

    line = str(S1) + "," + str(S2)
    for field in fields[2:]: line += "," + field
    CyberOut.write(line + "\n")
