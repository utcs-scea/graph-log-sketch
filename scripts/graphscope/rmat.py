#!/usr/bin/python3

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import graphscope
import sys
import time

# command parameters:
# <graph-dir> <number of partitions>
#

graph_dir = sys.argv[1]
partitions = int(sys.argv[2])

sess = graphscope.session(cluster_type='hosts')
graph = sess.g(directed=False)

for i in range(0, partitions):
  start = time.time()
  if i == 0:
    graph = graph.add_vertices(graph_dir + "/nodes_" + str(i) + ".csv", label="src")
  graph = graph.add_edges(graph_dir + "/edges_" + str(i) + ".csv", label="_")
  end = time.time()
  print("Running on partition: " + str(i + 1) + "/" + str(partitions))
  print("Ingest Time: " + str(end - start) + "s")

  start = time.time()
  pr_res = graphscope.pagerank(graph, delta=0.85, max_round=10)
  end = time.time()
  print("Pagerank Time: " + str(end - start) + "s")

  start = time.time()
  bfs_res = graphscope.bfs(graph, src=0)
  end = time.time()
  print("BFS Time: " + str(end - start) + "s")

  start = time.time()
  tc_res = graphscope.triangles(graph)
  end = time.time()
  print("Triangle Counting Time: " + str(end - start) + "s")

  start = time.time()
  dc_res = graphscope.degree_centrality(graph, centrality_type="both")
  end = time.time()
  print("Degree Centrality Time: " + str(end - start) + "s")

  start = time.time()
  cc_res = graphscope.wcc(graph)
  end = time.time()
  print("Weak Connected Components Time: " + str(end - start) + "s")
  print("Finished partition: " + str(i + 1) + "/" + str(partitions) + "\n\n")
sess.close()
