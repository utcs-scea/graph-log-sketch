// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <utility>

#include "galois/ParallelSTL.h"
#include "galois/graphs/LS_LC_CSR_Graph.h"
#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class LS_CSR : public MutableGraph {
private:
  galois::graphs::LS_LC_CSR_Graph<void, void> graph;
  float const compact_threshold;

public:
  explicit LS_CSR(float compact_threshold)
      : graph(0), compact_threshold(compact_threshold) {}

  virtual ~LS_CSR() {}

  uint64_t size() noexcept override { return graph.size(); }

  uint64_t get_out_degree(uint64_t src) override {
    return graph.getDegree(src);
  }

  void ingest(
      std::vector<std::pair<uint64_t, std::vector<uint64_t>>> edges) override {
    // galois::ParallelSTL::sort(
    //     edges.begin(), edges.end(),
    //     [](auto const& a, auto const& b) { return a.first < b.first; });

    graph.addBatchTopologyOnly<false>(edges);
  }

  void post_ingest() override {
    size_t csr_usage    = graph.getCSRMemoryUsageBytes();
    size_t memory_usage = graph.getLogMemoryUsageBytes();
    if (memory_usage > compact_threshold * csr_usage) {
      std::cout << "compact (csr_usage = " << csr_usage
                << ", memory_usage = " << memory_usage << ")" << std::endl;
      graph.compact();
      std::cout << "compacted (csr_usage = " << graph.getCSRMemoryUsageBytes()
                << ", memory_usage = " << graph.getLogMemoryUsageBytes() << ")"
                << std::endl;
    }
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
