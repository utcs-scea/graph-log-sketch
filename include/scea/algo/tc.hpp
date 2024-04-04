// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <atomic>
#include <limits>
#include <memory>
#include <utility>

#include "algo_interface.hpp"

namespace scea::algo {

class TriangleCounting : public Algo {
  galois::GAccumulator<uint64_t> numTriangles;

public:
  uint64_t compute(scea::graph::MutableGraph& g) {
    numTriangles.reset();

    galois::do_all(galois::iterate((uint64_t)0, g.size()),
                   [&](uint64_t const& v0) {
                     g.sort_edges(v0);
                     g.for_each_edge(v0, [&](uint64_t const& v1) {
                       if (v0 >= v1)
                         return;
                       g.for_each_edge(v1, [&](uint64_t const& v2) {
                         // Check (v0, v2) exists?
                         if (v1 >= v2)
                           return;
                         if (g.find_edge_sorted(v0, v2))
                           numTriangles += 1;
                       });
                     });
                   });

    return numTriangles.reduce();
  }

  void operator()(scea::graph::MutableGraph& g) override { compute(g); }
};

} // namespace scea::algo
