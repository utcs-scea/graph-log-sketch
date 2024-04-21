// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <utility>

#include "galois/graphs/MorphGraph.h"
#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class MorphGraph : public MutableGraph {
private:
  using Graph = galois::graphs::MorphGraph<uint64_t, void, true>;

  Graph m_graph;
  std::vector<Graph::GraphNode> m_vertices;

public:
  virtual ~MorphGraph() {}

  uint64_t size() noexcept override { return m_graph.size(); }

  uint64_t get_out_degree(uint64_t src) override {
    return std::distance(m_graph.edge_begin(m_vertices[src]),
                         m_graph.edge_end(m_vertices[src]));
  }

  void ingest(
      std::vector<std::pair<uint64_t, std::vector<uint64_t>>> edges) override {
    galois::GReduceMax<uint64_t> max_vertex_id;
    max_vertex_id.reset();
    galois::do_all(
        galois::iterate(edges), //
        [&](auto const& pair) {
          auto const& [src, dsts] = pair;
          max_vertex_id.update(src);
          // update with max element of dsts
          if (!dsts.empty())
            max_vertex_id.update(*std::max_element(dsts.begin(), dsts.end()));
        },
        galois::steal(), //
        galois::loopname("FindMaxVertex"));

    for (uint64_t i = m_vertices.size(); i < max_vertex_id.reduce() + 1; ++i) {
      auto n = m_graph.createNode(i);
      m_graph.addNode(n);
      m_vertices.emplace_back(n);
    }

    galois::do_all(
        galois::iterate(edges),
        [&](auto const& pair) {
          auto const& [src, dsts] = pair;
          for (auto dst : dsts)
            m_graph.addEdge(m_vertices[src], m_vertices[dst]);
        },
        galois::steal(), galois::loopname("AddEdgesToMorphGraph"));
  }

  void post_ingest() override {}

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& edge : m_graph.edges(m_vertices[src]))
      callback(m_graph.getData(m_graph.getEdgeDst(edge)));
  }

  void sort_edges(uint64_t src) override {
    m_graph.sortEdgesByDst(m_vertices[src]);
  }

  bool find_edge_sorted(uint64_t src, uint64_t dst) override {
    return m_graph.findEdgeSortedByDst(m_vertices[src], m_vertices[dst]) !=
           m_graph.edge_end(m_vertices[src]);
  }
};

} // namespace scea::graph
