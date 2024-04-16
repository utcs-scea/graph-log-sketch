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
  float const compact_threshold;
  bool first_ingest = true;

public:
  LS_CSR(uint64_t num_vertices, float compact_threshold)
      : graph(num_vertices), compact_threshold(compact_threshold) {}

  virtual ~LS_CSR() {}

  uint64_t size() noexcept override { return graph.size(); }

  uint64_t get_out_degree(uint64_t src) override {
    return graph.getDegree(src);
  }

  void add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    graph.addEdgesTopologyOnly(src, dsts);
  }

  void post_ingest() override {
    float holes_usage  = graph.getLogHolesMemoryUsageBytes();
    float memory_usage = graph.getLogMemoryUsageBytes();
    if (first_ingest || ((memory_usage > 0.0) &&
                         ((holes_usage / memory_usage) > compact_threshold))) {
      std::cout << "compact (holes_usage = " << holes_usage
                << ", memory_usage = " << memory_usage << ")" << std::endl;
      graph.compact();
    }
    first_ingest = false;
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    graph.for_each_edge(src, callback);
  }

  void sort_edges(uint64_t src) override { graph.sortEdges(src); }

  bool find_edge_sorted(uint64_t src, uint64_t dst) override {
    return graph.findEdgeSorted(src, dst);
  }
};

} // namespace scea::graph
