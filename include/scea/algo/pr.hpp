// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <cmath>
#include <atomic>
#include <algorithm>

#include "algo_interface.hpp"
#include "galois/LargeArray.h"
#include "galois/AtomicHelpers.h"
#include "galois/Reduction.h"

namespace scea::algo {

class PageRank : public Algo {
  static constexpr double DAMPING_FACTOR = 0.85;
  static constexpr int MAX_ITERATIONS    = 100;
  static constexpr double TOLERANCE      = 1.0e-4;

public:
  PageRank() = default;

  static void compute(scea::graph::MutableGraph& g) {
    const uint64_t numNodes = g.size();
    std::vector<std::atomic<double>> newRank(numNodes);
    std::vector<double> rank(numNodes, 1.0 / numNodes);
    std::vector<unsigned> out_degrees(numNodes, 0);

    galois::do_all(
        galois::iterate(0ul, numNodes),
        [&](uint64_t i) {
          g.for_each_edge(i, [&](uint64_t const&) { out_degrees[i]++; });
        },
        galois::no_stats(), galois::loopname("ComputeOutDegrees"),
        galois::steal());

    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
      galois::do_all(
          galois::iterate(size_t(0), newRank.size()),
          [&](size_t i) { newRank[i].store(0.0, std::memory_order_relaxed); },
          galois::no_stats(), galois::loopname("ResetNewRank"),
          galois::steal());

      galois::do_all(
          galois::iterate(0ul, numNodes),
          [&](uint64_t i) {
            g.for_each_edge(i, [&](uint64_t dst) {
              galois::atomicAdd(newRank[dst], rank[i] / out_degrees[i]);
            });
          },
          galois::no_stats(), galois::loopname("DistributeContributions"),
          galois::steal());

      galois::do_all(
          galois::iterate(0ul, numNodes),
          [&](uint64_t i) {
            newRank[i].store((1.0 - DAMPING_FACTOR) / numNodes +
                                 DAMPING_FACTOR *
                                     newRank[i].load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
          },
          galois::no_stats(), galois::loopname("ComputeNewRanks"),
          galois::steal());

      galois::GAccumulator<double> diff;
      galois::do_all(
          galois::iterate(0ul, numNodes),
          [&](uint64_t i) {
            diff +=
                std::abs(newRank[i].load(std::memory_order_relaxed) - rank[i]);
            rank[i] = newRank[i].load(std::memory_order_relaxed);
          },
          galois::no_stats(), galois::loopname("CheckConvergenceAndUpdate"),
          galois::steal());

      if (diff.reduce() < TOLERANCE) {
        break;
      }
    }
  }

  void operator()(scea::graph::MutableGraph& g) override { compute(g); }
};

} // namespace scea::algo
