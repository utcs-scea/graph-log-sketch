// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <queue>
#include <string>
#include <limits>

#include "galois/AtomicHelpers.h"
#include "galois/LargeArray.h"
#include "galois/gstl.h"

#include "algo_interface.hpp"
#include "utils.hpp"

namespace scea::algo {

class BetweennessCentrality : public Algo {
  using LevelWorklistType = galois::InsertBag<uint64_t, 4096>;
  constexpr static const uint64_t infinity =
      std::numeric_limits<uint64_t>::max() / 4;
  constexpr static const unsigned LEVEL_CHUNK_SIZE = 256u;

  uint64_t numOfSources;
  std::string sourceFile;

public:
  BetweennessCentrality(uint64_t _numOfSources  = 0,
                        std::string _sourceFile = "")
      : numOfSources(_numOfSources), sourceFile(_sourceFile) {}

  static galois::LargeArray<double> compute(scea::graph::MutableGraph& g,
                                            uint64_t numOfSources  = 0,
                                            std::string sourceFile = "") {
    std::vector<size_t> sources;
    if (numOfSources == 0) {
      if (sourceFile == "") {
        sources.resize(g.size());
        std::iota(sources.begin(), sources.end(), 0);
      } else {
        sources = sampleFromFile(sourceFile);
      }
    } else {
      sources = sampleGen(0, g.size(), numOfSources, sourceFile);
    }

    galois::LargeArray<uint64_t> distance;
    galois::LargeArray<std::atomic<uint64_t>> shortestPathCount;
    galois::LargeArray<double> dependency;
    galois::LargeArray<double> centralityScores;
    distance.create(g.size(), infinity);
    shortestPathCount.create(g.size());
    dependency.create(g.size(), 0.0);
    centralityScores.create(g.size(), 0.0);

    for (uint64_t s : sources) {
      galois::do_all(
          galois::iterate(0ul, g.size()),
          [&](uint64_t n) {
            distance[n]          = infinity;
            shortestPathCount[n] = 0;
            dependency[n]        = 0;
          },
          galois::no_stats(), galois::loopname("InitializeGraph"));

      galois::gstl::Vector<LevelWorklistType> stackOfWorklists =
          levelSSSP(g, s, distance, shortestPathCount);

      // minus 3 because last one is empty, one after is leaf nodes, and one
      // to correct indexing to 0 index
      if (stackOfWorklists.size() >= 3) {
        size_t currentLevel = stackOfWorklists.size() - 3;

        // last level is ignored since it's just the source
        while (currentLevel > 0) {
          LevelWorklistType& currentWorklist = stackOfWorklists[currentLevel];
          size_t succLevel                   = currentLevel + 1;

          galois::do_all(
              galois::iterate(currentWorklist),
              [&](uint64_t n) {
                GALOIS_ASSERT(distance[n] == currentLevel, "wrong level");

                g.for_each_edge(n, [&](uint64_t dest) {
                  if (distance[dest] == succLevel) {
                    // grab dependency, add to self
                    double contrib =
                        (static_cast<double>(1) + dependency[dest]) /
                        shortestPathCount[dest];
                    dependency[n] += contrib;
                  }
                });

                // multiply at end to get final dependency value
                dependency[n] *= shortestPathCount[n];
                // accumulate dependency into bc
                centralityScores[n] += dependency[n];
              },
              galois::steal(), galois::chunk_size<LEVEL_CHUNK_SIZE>(),
              galois::no_stats(), galois::loopname("Brandes"));

          // move on to next level lower
          currentLevel--;
        }
      }
    }

    return centralityScores;
  }

  static galois::gstl::Vector<LevelWorklistType>
  levelSSSP(scea::graph::MutableGraph& g, uint64_t s,
            galois::LargeArray<uint64_t>& distance,
            galois::LargeArray<std::atomic<uint64_t>>& shortestPathCount) {
    galois::gstl::Vector<LevelWorklistType> stackOfWorklists;
    uint64_t currentLevel = 0;

    // construct first level worklist which consists only of source
    stackOfWorklists.emplace_back();
    stackOfWorklists[0].emplace(s);
    distance[s]          = 0;
    shortestPathCount[s] = 1;

    // loop as long as current level's worklist is non-empty
    while (!stackOfWorklists[currentLevel].empty()) {
      // create worklist for next level
      stackOfWorklists.emplace_back();
      uint64_t nextLevel = currentLevel + 1;

      galois::do_all(
          galois::iterate(stackOfWorklists[currentLevel]),
          [&](uint64_t n) {
            GALOIS_ASSERT(distance[n] == currentLevel, "wrong level");

            g.for_each_edge(n, [&](uint64_t dest) {
              if (distance[dest] == infinity) {
                uint64_t oldVal = __sync_val_compare_and_swap(
                    &(distance[dest]), infinity, nextLevel);
                // only 1 thread should add to worklist
                if (oldVal == infinity) {
                  stackOfWorklists[nextLevel].emplace(dest);
                }

                galois::atomicAdd(shortestPathCount[dest],
                                  shortestPathCount[n].load());
              } else if (distance[dest] == nextLevel) {
                galois::atomicAdd(shortestPathCount[dest],
                                  shortestPathCount[n].load());
              }
            });
          },
          galois::steal(), galois::chunk_size<LEVEL_CHUNK_SIZE>(),
          galois::no_stats(), galois::loopname("SSSP"));

      // move on to next level
      currentLevel++;
    }
    return stackOfWorklists;
  }

  void operator()(scea::graph::MutableGraph& g) override {
    compute(g, numOfSources, sourceFile);
  }
};

class SerialBetweennessCentrality : public Algo {
public:
  SerialBetweennessCentrality() = default;

  static std::vector<double> compute(scea::graph::MutableGraph& g) {
    std::vector<double> centralityScores(g.size(), 0.0);

    for (uint64_t s = 0; s < g.size(); ++s) {
      std::vector<std::vector<uint64_t>> predecessors(g.size());
      std::vector<int> shortestPathCount(g.size(), 0);
      std::vector<int> distance(g.size(), std::numeric_limits<int>::max());

      int maxLevel =
          bfsShortestPaths(g, s, predecessors, shortestPathCount, distance);

      std::vector<double> dependency(g.size(), 0.0);

      while (maxLevel >= 0) {
        for (uint64_t i = 0; i < g.size(); i++) {
          if (distance[i] != maxLevel)
            continue;
          for (uint64_t pred : predecessors[i]) {
            double ratio = static_cast<double>(shortestPathCount[pred]) /
                           shortestPathCount[i];
            dependency[pred] += (1 + dependency[i]) * ratio;
          }
          if (i != s) {
            centralityScores[i] += dependency[i];
          }
        }
        maxLevel--;
      }
    }

    return centralityScores;
  }

  static int bfsShortestPaths(scea::graph::MutableGraph& g, uint64_t s,
                              std::vector<std::vector<uint64_t>>& predecessors,
                              std::vector<int>& shortestPathCount,
                              std::vector<int>& distance) {
    int maxLevel = 0;
    std::queue<uint64_t> q;
    q.push(s);
    distance[s]          = 0;
    shortestPathCount[s] = 1;

    while (!q.empty()) {
      uint64_t u = q.front();
      q.pop();
      g.for_each_edge(u, [&](uint64_t v) {
        if (distance[v] == std::numeric_limits<int>::max()) {
          q.push(v);
          distance[v] = distance[u] + 1;
        }
        if (distance[v] == distance[u] + 1) {
          shortestPathCount[v] += shortestPathCount[u];
          predecessors[v].push_back(u);
        }
        if (distance[v] > maxLevel)
          maxLevel = distance[v];
      });
    }
    return maxLevel;
  }

  void operator()(scea::graph::MutableGraph& g) override { compute(g); }
};

} // namespace scea::algo
