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

GraphIn  = open(el_file, "r")

graph = dict()

while True:
  line = GraphIn.readline()
  if line == "": break
  if line[0] == '#': continue
  fields = line[:-1].split('\t')
  if fields[0] not in graph:
    graph[fields[0]] = []
  if fields[1] not in graph:
    graph[fields[1]] = []
  graph[fields[0]].append(fields[1])
  # enable for symmetric directed graph
  # graph[fields[1]].append(fields[0])

NodeOut = []
EdgeOut = []
placed_nodes = set()

for i in range(0, partitions):
  NodeOut.append(open(out_dir + "/nodes_" + str(i) + ".csv", "w"))
  EdgeOut.append(open(out_dir + "/edges_" + str(i) + ".csv", "w"))
  NodeOut[-1].write("id\n")
  EdgeOut[-1].write("src,dst\n")

processed = 0
partition = -1
partition_size = int(len(graph) / partitions) + partitions + 1
for node, edges in graph.items():
  if processed % partition_size == 0:
    partition += 1
  if not node in placed_nodes:
    placed_nodes.add(node)
    NodeOut[partition].write(node + "\n")
  for dst in edges:
    if not dst in placed_nodes:
      placed_nodes.add(dst)
      NodeOut[partition].write(dst + "\n")
    EdgeOut[partition].write(node + "," + dst + "\n")
  processed += 1

for i in range(0, partitions):
  NodeOut[i].close()
  EdgeOut[i].close()
