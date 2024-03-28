// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include "galois/Galois.h"
#include "scea/graph/morph.hpp"
#include "scea/algo/tc.hpp"

TEST(TC, Tiny) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(3);

  /*
   * 0 - 1
   * | /
   * 2
   */

  graph.add_edges(0, {1, 2});
  graph.add_edges(1, {0, 2});
  graph.add_edges(2, {0, 1});

  auto tc_operator = scea::algo::TriangleCounting();
  auto result      = tc_operator.compute(graph);
  EXPECT_EQ(result, 1);
}

TEST(TC, Small) {
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

  graph.add_edges(0, {1, 2, 3});
  graph.add_edges(1, {0, 2, 3});
  graph.add_edges(2, {0, 1, 3});
  graph.add_edges(3, {0, 1, 2, 4, 5});
  graph.add_edges(4, {3, 5});
  graph.add_edges(5, {3, 4, 6});
  graph.add_edges(6, {5});

  auto tc_operator = scea::algo::TriangleCounting();
  auto result      = tc_operator.compute(graph);
  EXPECT_EQ(result, 5);
}
