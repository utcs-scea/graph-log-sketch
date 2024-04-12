// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <atomic>
#include <limits>
#include <memory>
#include <utility>

#include "algo_interface.hpp"
#include "galois/LargeArray.h"

namespace scea::algo {

class SSSP_BFS : public Algo {
  const uint64_t m_src;

public:
  explicit SSSP_BFS(uint64_t src) : m_src(src) {}

  static galois::LargeArray<uint64_t> compute(scea::graph::MutableGraph& g,
                                              uint64_t src) {
    using Cont = galois::InsertBag<uint64_t>;

    auto currSt = std::make_unique<Cont>();
    auto nextSt = std::make_unique<Cont>();

    constexpr uint64_t UNVISITED = std::numeric_limits<uint64_t>::max();
    galois::LargeArray<uint64_t> shortest_path;
    shortest_path.create(g.size(), UNVISITED);

    nextSt->push(src);

    uint64_t level = 0U;

    while (!nextSt->empty()) {
      std::swap(currSt, nextSt);
      nextSt->clear();

      galois::do_all(
          galois::iterate(*currSt),
          [&](uint64_t const& vertex) {
        if (shortest_path[vertex] != UNVISITED)
          return; // already visited

        // no sync needed since all threads would write the same level
        shortest_path[vertex] = level;

        // not previously visited, add all edges
        g.for_each_edge(vertex, [&](uint64_t const& neighbor) {
          // neighbor might be added multiple times, but that's fine
          if (shortest_path[neighbor] == UNVISITED)
            nextSt->push(neighbor);
        });
          },
          galois::steal(),
          galois::loopname("SSSP_BFS");

      ++level;
    }
    return shortest_path;
  }

  void operator()(scea::graph::MutableGraph& g) override { compute(g, m_src); }
};

} // namespace scea::algo
