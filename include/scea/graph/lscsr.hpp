#pragma once

#include "scea/graph/mutable_graph_interface.hpp"

#include "galois/graphs/LS_LC_CSR_Graph.h"

#include <vector>

namespace scea::graph {

class LS_CSR : public MutableGraph {
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

} // namespace scea::graph