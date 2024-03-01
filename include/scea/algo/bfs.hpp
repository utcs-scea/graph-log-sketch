// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <atomic>
#include <limits>
#include <memory>
#include <utility>
#include <optional>

#include "algo_interface.hpp"
#include "galois/LargeArray.h"

namespace scea::algo {

class SSSP_BFS : public Algo {
  const uint64_t m_src;
  const bool m_compact;

public:
  SSSP_BFS(uint64_t src, bool compact) : m_src(src), m_compact(compact) {}

  static galois::LargeArray<uint64_t>
  compute(scea::graph::MutableGraph& g, uint64_t src, bool compact = true) {
    using Cont = galois::InsertBag<std::optional<uint64_t>>;

    auto currSt = std::make_unique<Cont>();
    auto nextSt = std::make_unique<Cont>();

    galois::LargeArray<std::atomic_bool> visited;
    visited.create(g.size(), false);

    galois::LargeArray<uint64_t> shortest_path;
    shortest_path.create(g.size(), std::numeric_limits<uint64_t>::max());

    nextSt->push(std::optional<uint64_t>(src));
    shortest_path[src] = 0U;

    uint64_t level = 0U;

    while (!nextSt->empty()) {
      std::swap(currSt, nextSt);
      nextSt->clear();
      currSt->push(std::optional<uint64_t>());

      galois::do_all(
          galois::iterate(*currSt),
          [&](std::optional<uint64_t> const& work) {
            if (work.has_value()) {
              uint64_t const& vertex = work.value();
              if (visited[vertex].exchange(true, std::memory_order_seq_cst))
                return;

              shortest_path[vertex] = level;

              // not previously visited, add all edges
              g.for_each_edge(vertex, [&](uint64_t const& neighbor) {
                if (!visited[neighbor].load(std::memory_order_relaxed))
                  nextSt->push(neighbor);
              });
            } else if (compact) {
              g.compact();
            }
          },
          galois::steal());

      ++level;
    }
    return shortest_path;
  }

  void operator()(scea::graph::MutableGraph& g) override {
    compute(g, m_src, m_compact);
  }
};

} // namespace scea::algo
