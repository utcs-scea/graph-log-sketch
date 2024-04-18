#!/usr/bin/python3

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2023. University of Texas at Austin. All rights reserved.

import sys
import time
import graphscope

print(graphscope.__version__)

# command parameters:
# <graph-dir> <number of partitions>
#

with graphscope.session(
            cluster_type='k8s',
            k8s_namespace='default',
            preemptive=False,
            k8s_deploy_mode="eager",
            k8s_client_config="/home/pkenney/.kube/scea-config",
            num_workers=2,
            k8s_vineyard_cpu=.5,
            k8s_vineyard_mem="4Gi",
            k8s_engine_cpu=5,
            k8s_engine_mem="16Gi",
            k8s_coordinator_cpu=.5,
            k8s_coordinator_mem="1Gi",
            vineyard_shared_mem="8Gi",
            k8s_volumes={
                "local-data" : {
                    "type": "hostPath", "field": {
                        "path": "/var/local/graphs/rmat20", "type": "Directory"
                    }, "mounts": [
                        {
                            "mountPath": "/tmp/data"
                        }
                    ]
                }
            }) as sess:
    #graph = load_ogbn_mag(sess, '/dataset/ogbn_mag_small')
    graph_dir = "/tmp/data"
    graph = sess.g(directed=False)
    graph = graph.add_vertices(graph_dir + "/nodes.csv", label="src")
    graph = graph.add_edges(graph_dir + "/edges_0.csv", label="_")
    print("finished import")
    print(graph.schema)
    start = time.time()
    #pr_res = graphscope.pagerank(graph, delta=0.85, max_round=10)
    #bfs_res = graphscope.bfs(graph, 0)
    tc_res = graphscope.triangles(graph)
    print(tc_res)
    end = time.time()
    print("Triangle Counting Time: " + str(end - start) + "s")

'''
graph_dir = sys.argv[1]
partitions = int(sys.argv[2])

for algo in range(0, 5):
  sess = graphscope.session(cluster_type='hosts')
  graph = sess.g(directed=False)

  algo_start = time.time()
  for i in range(0, partitions):
    start = time.time()
    if i == 0:
      graph = graph.add_vertices(graph_dir + "/nodes.csv", label="src")
    graph = graph.add_edges(graph_dir + "/edges_" + str(i) + ".csv", label="_")
    end = time.time()
    print("Running on partition: " + str(i + 1) + "/" + str(partitions))
    print("Ingest Time: " + str(end - start) + "s")

    if algo == 0:
      start = time.time()
      pr_res = graphscope.pagerank(graph, delta=0.85, max_round=10)
      end = time.time()
      print("Pagerank Time: " + str(end - start) + "s")

    if algo == 1:
      start = time.time()
      bfs_res = graphscope.bfs(graph, src=0)
      end = time.time()
      print("BFS Time: " + str(end - start) + "s")

    if algo == 2:
      start = time.time()
      tc_res = graphscope.triangles(graph)
      end = time.time()
      print("Triangle Counting Time: " + str(end - start) + "s")

    if algo == 3:
      start = time.time()
      dc_res = graphscope.degree_centrality(graph, centrality_type="both")
      end = time.time()
      print("Degree Centrality Time: " + str(end - start) + "s")

    if algo == 4:
      start = time.time()
      cc_res = graphscope.wcc(graph)
      end = time.time()
      print("Weak Connected Components Time: " + str(end - start) + "s")
    print("Finished partition: " + str(i + 1) + "/" + str(partitions) + "\n")
  algo_end = time.time()
  print("Total time across partitions: " + str(algo_end - algo_start) + "s\n\n")
  sess.close()
'''
