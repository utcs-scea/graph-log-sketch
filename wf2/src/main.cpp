// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <iostream>

#include "galois/DistGalois.h"
#include "galois/runtime/DataCommMode.h"

#include "galois/wmd/schema.h"
#include "galois/wmd/graph.h"
#include "galois/wmd/WMDPartitioner.h"
#include "galois/shad/DataTypes.h"
#include "galois/wmd/graphTypes.h"
#include "galois/runtime/GraphUpdateManager.h"
#include "galois/graphs/GenericPartitioners.h"
#include "graph_ds.hpp"

#define DBG_PRINT(x) {std::cout << "[WF2-DEBUG] " << x << std::endl;}

typedef galois::graphs::WMDGraph<wf2::Vertex, wf2::Edge, OECPolicy> Graph;

int main(int argc, char* argv[]) {
  if(argc < 2) {
    std::cerr << "Expected graph file name as argument\n";
    return 1;
  }
  std::string filename(argv[1]);
  DBG_PRINT("Reading from file: " << filename);

  galois::DistMemSys G; // init galois memory
  auto& net = galois::runtime::getSystemNetworkInterface();

  if (net.ID == 0) {
    DBG_PRINT("Num Hosts: " << net.Num << 
                   ", Active Threads Per Hosts: " << galois::getActiveThreads() << 
                   "\n");
  }

  std::vector<std::string> filenames;
  filenames.emplace_back(filename);
  std::vector<std::unique_ptr<galois::graphs::FileParser<
      wf2::Vertex, wf2::Edge>>>
      parsers;
  parsers.emplace_back(
      std::make_unique<wf2::Wf2WMDParser<wf2::Vertex,
                                                 wf2::Edge>>(
          10, filenames));
  Graph* graph = new Graph(parsers, net.ID, net.Num, true, false,
                           galois::graphs::BALANCED_EDGES_OF_MASTERS);
  assert(graph != nullptr);


  return 0;
}
