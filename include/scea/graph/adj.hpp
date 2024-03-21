// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <algorithm>
#include <iterator>

#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class AdjGraph : public MutableGraph {
private:
  std::vector<std::vector<uint64_t>> edges;

public:
  explicit AdjGraph(uint64_t num_vertices) : edges(num_vertices) {}

  virtual ~AdjGraph() {}

  uint64_t size() noexcept override { return edges.size(); }

  int add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    std::copy(dsts.begin(), dsts.end(), std::back_inserter(edges[src]));
    return 0;
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& dst : edges[src])
      callback(dst);
  }
};

} // namespace scea::graph
