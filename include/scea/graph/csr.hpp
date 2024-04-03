// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>

#include "galois/graphs/LC_CSR_64_Graph.h"
#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class LC_CSR : public MutableGraph {
private:
  using GraphTy = galois::graphs::LC_CSR_64_Graph<void, void>;
  std::vector<std::vector<uint64_t>> m_adj;
  std::unique_ptr<GraphTy> m_graph;
  std::atomic<size_t> num_edges = ATOMIC_VAR_INIT(0);

public:
  explicit LC_CSR(uint64_t num_vertices) : m_adj(num_vertices) {}

  virtual ~LC_CSR() {}

  uint64_t size() noexcept override { return m_adj.size(); }

  void add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    std::copy(dsts.begin(), dsts.end(), std::back_inserter(m_adj[src]));
    num_edges.fetch_add(dsts.size(), std::memory_order_relaxed);
  }

  void post_ingest() override {
    m_graph = std::make_unique<GraphTy>(
        m_adj.size(), num_edges.load(std::memory_order_acquire),
        [&](size_t const& src) { return m_adj[src].size(); },
        [&](size_t const& src, size_t const& idx) { return m_adj[src][idx]; },
        [&](size_t const& src, size_t const& idx) { return 0; });
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& edge : m_graph->edges(src))
      callback(m_graph->getEdgeDst(edge));
  }

  void sort_edges(uint64_t src) override { m_graph->sortEdgesByDst(src); }
  bool find_edge_sorted(uint64_t src, uint64_t dst) override {
    return m_graph->findEdgeSortedByDst(src, dst) != m_graph->edge_end(src);
  }
};

} // namespace scea::graph
