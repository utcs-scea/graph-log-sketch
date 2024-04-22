// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include "galois/Galois.h"
#include "scea/graph/morph.hpp"
#include "scea/algo/bfs.hpp"

using el = std::pair<uint64_t, std::vector<uint64_t>>;

TEST(BFS, Small) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph;

  /*
   * 0 - 1
   * | x |
   * 2 - 3
   *     | \
   *     4 - 5 - 6
   */

  graph.ingest({el(0, {1, 2, 3}), el(1, {0, 2, 3}), el(2, {0, 1, 3}),
                el(3, {0, 1, 2, 4, 5}), el(4, {3, 5}), el(5, {3, 4, 6}),
                el(6, {5})});

  {
    auto result = scea::algo::SSSP_BFS::compute(graph, 1);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 0);
    EXPECT_EQ(result[2], 1);
    EXPECT_EQ(result[3], 1);
    EXPECT_EQ(result[4], 2);
    EXPECT_EQ(result[5], 2);
    EXPECT_EQ(result[6], 3);
  }
}
