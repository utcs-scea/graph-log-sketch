// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include "import.hpp"
#include "galois/wmd/schema.h"
#include "galois/wmd/graph.h"
#include "galois/wmd/WMDPartitioner.h"
#include "galois/shad/DataTypes.h"
#include "galois/wmd/graphTypes.h"
#include "galois/runtime/GraphUpdateManager.h"
#include "galois/graphs/GenericPartitioners.h"
#include "graph_ds.hpp"

namespace wf2 {
wf2::Graph* ImportGraph(const std::string filename) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::vector<std::string> filenames;
  filenames.emplace_back(filename);
  std::vector<std::unique_ptr<galois::graphs::FileParser<
      agile::workflow1::Vertex, agile::workflow1::Edge>>>
      parsers;
  parsers.emplace_back(
      std::make_unique<galois::graphs::WMDParser<agile::workflow1::Vertex,
                                                 agile::workflow1::Edge>>(
          10, filenames));
  Graph* graph = new Graph(parsers, net.ID, net.Num, true, false,
                           galois::graphs::BALANCED_EDGES_OF_MASTERS);
  return graph;
}
} // namespace wf2
