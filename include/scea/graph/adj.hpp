// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <algorithm>
#include <iterator>
#include <utility>

#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class AdjGraph : public MutableGraph {
protected:
  std::vector<std::vector<uint64_t>> m_edges;

public:
  virtual ~AdjGraph() {}

  uint64_t size() noexcept override { return m_edges.size(); }

  uint64_t get_out_degree(uint64_t src) override { return m_edges[src].size(); }

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
    m_edges.resize(std::max(m_edges.size(), max_vertex_id.reduce() + 1));

    galois::do_all(
        galois::iterate(edges),
        [&](auto const& pair) {
          auto const& [src, dsts] = pair;
          std::copy(dsts.begin(), dsts.end(), std::back_inserter(m_edges[src]));
        },
        galois::steal(), galois::loopname("Ingest"));
  }

  void post_ingest() override {}

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto const& dst : m_edges[src])
      callback(dst);
  }

  void sort_edges(uint64_t src) {
    std::sort(m_edges[src].begin(), m_edges[src].end());
  }

  bool find_edge_sorted(uint64_t src, uint64_t dst) {
    return std::binary_search(m_edges[src].begin(), m_edges[src].end(), dst);
  }
};

} // namespace scea::graph
