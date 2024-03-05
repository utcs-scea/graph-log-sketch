# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import os
import sys
import random
import numpy as np

# command parameters: # <num_persons> <num_servers> <seed>
#
# 2000000 500000 4
#

UsesOut  = open("uses.csv", "w")

line1 = "#delimieter: ,\n"
line2 = "#columns: person,server\n"
line3 = "#types: UINT,UINT\n"

UsesOut.write(line1)
UsesOut.write(line2)
UsesOut.write(line3)

num_persons = int(sys.argv[1])
num_servers = int(sys.argv[2])
np.random.seed(int(sys.argv[3]))

########### PRINT SOCIAL RECORDS ##########

for person in range(0, num_persons):
  num_used = int( np.random.exponential(20.0) )
  if num_used > 20: num_used = int( num_used / 10 )     # let's not generate too many edges

  for i in range(0, num_used):
    server = int( np.random.random() * num_servers )
    UsesOut.write(str(person) + "," + str(server) + "\n")
