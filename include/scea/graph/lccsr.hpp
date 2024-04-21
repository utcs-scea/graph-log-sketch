// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <memory>
#include <atomic>
#include <algorithm>

#include "galois/graphs/LC_CSR_64_Graph.h"
#include "scea/graph/adj.hpp"

namespace scea::graph {

class LC_CSR : public AdjGraph {
private:
  using GraphTy =
      galois::graphs::LC_CSR_64_Graph<void, void>::with_no_lockable<true>::type;

  std::unique_ptr<GraphTy> m_graph;
  std::atomic<size_t> num_edges = ATOMIC_VAR_INIT(0);

public:
  virtual ~LC_CSR() {}

  uint64_t get_out_degree(uint64_t src) override {
    return m_graph->getDegree(src);
  }

  void post_ingest() override {
    galois::GAccumulator<uint64_t> num_edges_accum;
    num_edges_accum.reset();
    galois::do_all(
        galois::iterate(m_edges),                                  //
        [&](auto const& dsts) { num_edges_accum += dsts.size(); }, //
        galois::loopname("CountEdges"));

    m_graph = std::make_unique<GraphTy>(
        m_edges.size(), num_edges_accum.reduce(),
        [&](size_t const& src) { return m_edges[src].size(); },
        [&](size_t const& src, size_t const& idx) { return m_edges[src][idx]; },
        [&](size_t const& /*src*/, size_t const& /*idx*/) {
          /* no edge data*/
          return 0;
        });
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
