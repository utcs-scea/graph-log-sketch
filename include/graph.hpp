#pragma once

#include <functional>

#include "galois/graphs/LS_LC_CSR_Graph.h"

namespace scea {

/* Contains the minimal functionalities that are benchmarked. */
class Graph {
public:
  virtual int add_edges(uint64_t src, const vector<uint64_t> dsts) = 0;
  virtual uint64_t size() const noexcept                           = 0;

  virtual void for_each_edge(uint64_t src,
                             std::function<void(uint64_t const&)> callback) = 0;
};

class LS_CSR : public Graph {
private:
  galois::graphs::LS_LC_CSR_Graph<> graph;

public:
  LS_CSR(uint64_t num_vertices) : graph(num_vertices) {}

  virtual ~LS_CSR() {}

  uint64_t size() const noexcept override { return graph.size(); }

  int add_edges(uint64_t src, const vector<uint64_t> dsts) override {
    return graph.addEdgesTopologyOnly(src, dsts);
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& dst : graph.edges(src))
      callback(dst);
  }
};

} // namespace scea