// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <limits>

#include "algo_interface.hpp"

namespace scea::algo {

class BetweennessCentrality : public Algo {
public:
  BetweennessCentrality() = default;

  static std::vector<double> compute(scea::graph::MutableGraph& g) {
    std::vector<double> centralityScores(g.size(), 0.0);

    for (uint64_t s = 0; s < g.size(); ++s) {
      std::vector<std::vector<uint64_t>> predecessors(g.size());
      std::vector<int> shortestPathCount(g.size(), 0);
      std::vector<int> distance(g.size(), std::numeric_limits<int>::max());

      bfsShortestPaths(g, s, predecessors, shortestPathCount, distance);

      std::vector<double> dependency(g.size(), 0.0);
      
      for (int i = g.size() - 1; i >= 0; --i) {
        for (uint64_t pred : predecessors[i]) {
          double ratio = (double)shortestPathCount[pred] / shortestPathCount[i];
          dependency[pred] += (1 + dependency[i]) * ratio;
        }
        if (i != s) {
          centralityScores[i] += dependency[i];
        }
      }
    }

    return centralityScores;
  }

  static void bfsShortestPaths(scea::graph::MutableGraph& g, uint64_t s,
                               std::vector<std::vector<uint64_t>>& predecessors,
                               std::vector<int>& shortestPathCount,
                               std::vector<int>& distance) {
    std::queue<uint64_t> q;
    q.push(s);
    distance[s] = 0;
    shortestPathCount[s] = 1;

    while (!q.empty()) {
      uint64_t u = q.front(); q.pop();
      g.for_each_edge(u, [&](uint64_t v) {
        if (distance[v] == std::numeric_limits<int>::max()) {
          q.push(v);
          distance[v] = distance[u] + 1;
        }
        if (distance[v] == distance[u] + 1) {
          shortestPathCount[v] += shortestPathCount[u];
          predecessors[v].push_back(u);
        }
      });
    }
  }

  void operator()(scea::graph::MutableGraph& g) override {
    compute(g);
  }
};

} // namespace scea::algo
