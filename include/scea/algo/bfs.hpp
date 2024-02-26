#pragma once

#include "algo_interface.hpp"

#include <limits>
#include <atomic>
#include <memory>

#include "galois/LargeArray.h"

namespace scea::algo {

class SSSP_BFS : public Algo {
  const uint64_t m_src;

public:
  SSSP_BFS(uint64_t src) : m_src(src) {}

  static galois::LargeArray<uint64_t> compute(scea::graph::MutableGraph& g,
                                              uint64_t src) {
    using Cont = galois::InsertBag<uint64_t>;

    auto currSt = std::make_unique<Cont>();
    auto nextSt = std::make_unique<Cont>();

    galois::LargeArray<std::atomic_bool> visited;
    visited.create(g.size(), false);

    galois::LargeArray<uint64_t> shortest_path;
    shortest_path.create(g.size(), std::numeric_limits<uint64_t>::max());

    nextSt->push(src);
    shortest_path[src] = 0U;

    uint64_t level = 0U;

    while (!nextSt->empty()) {
      std::swap(currSt, nextSt);
      nextSt->clear();

      galois::do_all(
          galois::iterate(*currSt),
          [&](uint64_t const& vertex) {
            if (visited[vertex].exchange(true, std::memory_order_seq_cst))
              return;

            shortest_path[vertex] = level;

            // not previously visited, add all edges
            g.for_each_edge(vertex, [&](uint64_t const& neighbor) {
              if (!visited[neighbor].load(std::memory_order_relaxed))
                nextSt->push(neighbor);
            });
          },
          galois::steal());

      ++level;
    }
    return shortest_path;
  }

  void operator()(scea::graph::MutableGraph& g) override { compute(g, m_src); }
};

} // namespace scea::algo
