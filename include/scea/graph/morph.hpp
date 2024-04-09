// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>

#include "galois/graphs/MorphGraph.h"
#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class MorphGraph : public MutableGraph {
private:
  using Graph = galois::graphs::MorphGraph<uint64_t, void, true>;

  Graph graph;
  std::vector<Graph::GraphNode> vertices;

public:
  explicit MorphGraph(uint64_t num_vertices) : vertices(num_vertices) {
    for (uint64_t i = 0; i < num_vertices; ++i) {
      auto n = graph.createNode(i);
      graph.addNode(n);
      vertices[i] = n;
    }
  }

  virtual ~MorphGraph() {}

  uint64_t size() noexcept override { return graph.size(); }

  uint64_t get_out_degree(uint64_t src) override {
    return std::distance(graph.edge_begin(vertices[src]),
                         graph.edge_end(vertices[src]));
  }

  void add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    for (auto dst : dsts)
      graph.addEdge(vertices[src], vertices[dst]);
  }

  void post_ingest() override {}

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& edge : graph.edges(vertices[src]))
      callback(graph.getData(graph.getEdgeDst(edge)));
  }

  void sort_edges(uint64_t src) override {
    graph.sortEdgesByDst(vertices[src]);
  }

  bool find_edge_sorted(uint64_t src, uint64_t dst) override {
    return graph.findEdgeSortedByDst(vertices[src], vertices[dst]) !=
           graph.edge_end(vertices[src]);
  }
};

} // namespace scea::graph
