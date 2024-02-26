#pragma once

#include <limits>
#include <atomic>
#include <memory>

#include "graph.hpp"
#include "galois/LargeArray.h"

namespace scea {

class Algo {
public:
  virtual void operator()(Graph& g) = 0;
};

class Nop : public Algo {
public:
  void operator()(Graph&) override {}
};

class SSSP_BFS : public Algo {
  const uint64_t m_src;

public:
  SSSP_BFS(uint64_t src) : m_src(src) {}

  static std::unique_ptr<galois::LargeArray<uint64_t>> compute(Graph& g,
                                                               uint64_t src) {
    using Cont = galois::InsertBag<uint64_t>;

    auto currSt = std::make_unique<Cont>();
    auto nextSt = std::make_unique<Cont>();

    galois::LargeArray<std::atomic_bool> visited;
    visited.create(g.size(), false);

    auto shortest_path = std::make_unique<galois::LargeArray<uint64_t>>();
    shortest_path->create(g.size(), std::numeric_limits<uint64_t>::max());

    nextSt->push(src);
    visited[src]           = true;
    shortest_path->at(src) = 0U;

    uint64_t level = 0U;

    while (!nextSt->empty()) {
      std::swap(currSt, nextSt);
      nextSt->clear();
      ++level;

      galois::do_all(
          galois::iterate(*currSt),
          [&](uint64_t const& vertex) {
            if (visited[vertex].exchange(true, std::memory_order_seq_cst))
              return;

            shortest_path->at(vertex) = level;

            // not previously visited, add all edges
            g.for_each_edge(vertex, [&](uint64_t const& neighbor) {
              if (!visited[neighbor].load(std::memory_order_relaxed))
                nextSt->push(neighbor);
            });
          },
          galois::steal());
    }
    return shortest_path;
  }

  void operator()(Graph& g) override { compute(g, m_src); }
};

} // namespace scea