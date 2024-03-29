# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import os
import sys
import random
import numpy as np

# command parameters: # <num_persons> <seed>
#
# 2000000 3
#

SocialOut  = open("social.csv", "w")

line1 = "#delimieter: ,\n"
line2 = "#columns: person1,person2\n"
line3 = "#types: UINT,UINT\n"

SocialOut.write(line1)
SocialOut.write(line2)
SocialOut.write(line3)

num_persons = int(sys.argv[1])
np.random.seed(int(sys.argv[2]))

########### PRINT SOCIAL RECORDS ##########

for person in range(0, num_persons):
  num_friends = int( np.random.exponential(20.0) )
  if num_friends > 20: num_friends = int( num_friends / 10 )     # let's not generate too many edges

  for i in range(0, num_friends):
    friend = int( np.random.random() * num_persons )
    while friend == person: friend = int( np.random.random() * num_persons )

    SocialOut.write(str(person) + "," + str(friend) + "\n")
