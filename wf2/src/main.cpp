// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <iostream>

#include "galois/DistGalois.h"
#include "galois/graphs/GluonSubstrate.h"
#include "galois/runtime/DataCommMode.h"

#include "galois/wmd/schema.h"
#include "galois/wmd/graph.h"
#include "galois/wmd/WMDPartitioner.h"
#include "galois/shad/DataTypes.h"
#include "galois/wmd/graphTypes.h"
#include "galois/runtime/GraphUpdateManager.h"
#include "galois/graphs/GenericPartitioners.h"
#include "graph_ds.hpp"
#include "import.hpp"
//#include "pattern.hpp"

#define DBG_PRINT(x)                                                           \
  { std::cout << "[WF2-DEBUG] " << x << std::endl; }

wf2::Graph* g;
std::unique_ptr<galois::graphs::GluonSubstrate<wf2::Graph>> sync_substrate;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Expected graph file name as argument\n";
    return 1;
  }
  std::string filename(argv[1]);
  DBG_PRINT("Reading from file: " << filename);

  galois::DistMemSys G; // init galois memory
  auto& net = galois::runtime::getSystemNetworkInterface();

  if (net.ID == 0) {
    DBG_PRINT("Num Hosts: " << net.Num << ", Active Threads Per Hosts: "
                            << galois::getActiveThreads() << "\n");
  }

  g = wf2::ImportGraph(filename);
  assert(g != nullptr);
  sync_substrate = std::make_unique<galois::graphs::GluonSubstrate<wf2::Graph>>(
      *g, net.ID, net.Num, g->isTransposed(), g->cartesianGrid());
  galois::do_all(
      galois::iterate(g->masterNodesRange().begin(),
                      g->masterNodesRange().end()),
      [&](const wf2::GlobalNodeID& lNode) {
        auto& node           = g->getData(lNode);
        const uint64_t gNode = g->getGID(lNode);

        for (auto e : g->edges(lNode)) {
          auto& edge_node = g->getData(g->getEdgeDst(e));
          auto& edge_data = g->getEdgeData(e);
          std::cout << node.id << " " << edge_node.id << " "
                    << int(edge_data.type) << std::endl;
        }
      },
      galois::steal());
  // wf2::MatchPattern(*g, net);

  return 0;
}
