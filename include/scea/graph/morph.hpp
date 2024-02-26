#pragma once

#include "scea/graph/mutable_graph_interface.hpp"

#include "galois/graphs/MorphGraph.h"

#include <vector>

namespace scea::graph {

class MorphGraph : public MutableGraph {
private:
  using Graph = galois::graphs::MorphGraph<uint64_t, void, true>;

  Graph graph;
  std::vector<Graph::GraphNode> vertices;

public:
  MorphGraph(uint64_t num_vertices) : vertices(num_vertices) {
    for (uint64_t i = 0; i < num_vertices; ++i) {
      auto n = graph.createNode(i);
      graph.addNode(n);
      vertices[i] = n;
    }
  }

  virtual ~MorphGraph() {}

  uint64_t size() noexcept override { return graph.size(); }

  int add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    for (auto dst : dsts)
      graph.addEdge(vertices[src], vertices[dst]);

    return 0;
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& edge : graph.edges(vertices[src]))
      callback(graph.getData(graph.getEdgeDst(edge)));
  }
};

} // namespace scea::graph