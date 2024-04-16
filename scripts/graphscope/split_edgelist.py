#!/usr/bin/python3

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import sys

# command parameters:
# <edge list file> <out-dir> <number of partitions>
#

el_file = sys.argv[1]
out_dir = sys.argv[2]
partitions = int(sys.argv[3])
total_edges = int(sys.argv[4])

GraphIn  = open(el_file, "r")

EdgeOut = []
for i in range(0, partitions):
  EdgeOut.append(open(out_dir + "/edges_" + str(i) + ".csv", "w"))
  EdgeOut[-1].write("src,dst\n")

nodes = set()

processed = 0
partition = -1
partition_size = int(total_edges / partitions) + partitions + 1
while True:
  line = GraphIn.readline()
  if line == "": break
  if line[0] == '#': continue
  if processed % partition_size == 0:
    partition += 1
    if partition >= partitions:
      partition = partitions - 1
  fields = line[:-1].split()
  nodes.add(fields[0])
  nodes.add(fields[1])
  EdgeOut[partition].write(fields[0] + "," + fields[1] + "\n")
  processed += 1

for i in range(0, partitions):
  EdgeOut[i].close()

NodeOut = open(out_dir + "/nodes.csv", "w")
NodeOut.write("id\n")
for node in nodes:
  NodeOut.write(node + "\n")
NodeOut.close()
