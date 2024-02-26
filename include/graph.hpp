#pragma once

#include <functional>

#include "galois/Galois.h"
#include "galois/graphs/MorphGraph.h"
#include "galois/graphs/LS_LC_CSR_Graph.h"

namespace scea {

/* Contains the minimal functionalities that are benchmarked. */
class Graph {
public:
  virtual int add_edges(uint64_t src, const std::vector<uint64_t> dsts) = 0;
  virtual uint64_t size() noexcept                                      = 0;

  virtual void for_each_edge(uint64_t src,
                             std::function<void(uint64_t const&)> callback) = 0;
};

class LS_CSR : public Graph {
private:
  galois::graphs::LS_LC_CSR_Graph<> graph;

public:
  LS_CSR(uint64_t num_vertices) : graph(num_vertices) {}

  virtual ~LS_CSR() {}

  uint64_t size() noexcept override { return graph.size(); }

  int add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    return graph.addEdgesTopologyOnly(src, dsts);
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& dst : graph.edges(src))
      callback(dst);
  }
};

class MorphGraph : public Graph {
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

} // namespace scea