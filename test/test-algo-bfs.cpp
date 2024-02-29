// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include "galois/Galois.h"
#include "scea/graph/morph.hpp"
#include "scea/algo/bfs.hpp"

TEST(BFS, Small) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(7);

  /*
   * 0 - 1
   * | x |
   * 2 - 3
   *     | \
   *     4 - 5 - 6
   */

  EXPECT_EQ(graph.add_edges(0, {1, 2, 3}), 0);
  EXPECT_EQ(graph.add_edges(1, {0, 2, 3}), 0);
  EXPECT_EQ(graph.add_edges(2, {0, 1, 3}), 0);
  EXPECT_EQ(graph.add_edges(3, {0, 1, 2, 4, 5}), 0);
  EXPECT_EQ(graph.add_edges(4, {3, 5}), 0);
  EXPECT_EQ(graph.add_edges(5, {3, 4, 6}), 0);
  EXPECT_EQ(graph.add_edges(6, {5}), 0);

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
