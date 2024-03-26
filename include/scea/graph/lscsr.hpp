// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>

#include "galois/graphs/LS_LC_CSR_Graph.h"
#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class LS_CSR : public MutableGraph {
private:
  galois::graphs::LS_LC_CSR_Graph<void, void> graph;

public:
  explicit LS_CSR(uint64_t num_vertices) : graph(num_vertices) {}

  virtual ~LS_CSR() {}

  uint64_t size() noexcept override { return graph.size(); }

  void add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    graph.addEdgesTopologyOnly(src, dsts);
  }

  void post_ingest() override {}

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& edge : graph.edges(src))
      callback(graph.getEdgeDst(edge));
  }
};

} // namespace scea::graph
