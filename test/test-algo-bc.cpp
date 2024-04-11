// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include "galois/Galois.h"
#include "scea/graph/morph.hpp"
#include "scea/algo/bc.hpp"

TEST(BC, Short) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(3);

  /*
   * 0 - 1 - 2
   */

  graph.add_edges(0, {1});
  graph.add_edges(1, {0, 2});
  graph.add_edges(2, {1});

  auto result = scea::algo::BetweennessCentrality::compute(graph, {0, 1, 2});
  {
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 0);
  }
}

TEST(BC, Long) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(4);

  /*
   * 0 - 1 - 2 - 3
   */

  graph.add_edges(0, {1});
  graph.add_edges(1, {0, 2});
  graph.add_edges(2, {1, 3});
  graph.add_edges(3, {2});

  auto result = scea::algo::BetweennessCentrality::compute(graph, {0, 1, 2, 3});
  {
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 4);
    EXPECT_EQ(result[2], 4);
    EXPECT_EQ(result[3], 0);
  }
}

TEST(BC, Square) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(4);

  /*
   * 0 - 1
   * |   |
   * 2 - 3
   */

  graph.add_edges(0, {1, 2});
  graph.add_edges(1, {0, 3});
  graph.add_edges(2, {0, 3});
  graph.add_edges(3, {1, 2});

  auto result = scea::algo::BetweennessCentrality::compute(graph, {0, 1, 2, 3});
  {
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 1);
    EXPECT_EQ(result[3], 1);
  }
}

TEST(BC, Star) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(4);

  /*
   * 0 - 1
   * | \
   * 2   3
   */

  graph.add_edges(0, {1, 2, 3});
  graph.add_edges(1, {0});
  graph.add_edges(2, {0});
  graph.add_edges(3, {0});

  auto result = scea::algo::BetweennessCentrality::compute(graph, {0, 1, 2, 3});
  {
    EXPECT_EQ(result[0], 6);
    EXPECT_EQ(result[1], 0);
    EXPECT_EQ(result[2], 0);
    EXPECT_EQ(result[3], 0);
  }
}

TEST(BC, Bipartite) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(5);

  /*
   *    0
   *  / | \
   * 1  2  3
   *  \ | /
   *    4
   */

  graph.add_edges(0, {1, 2, 3});
  graph.add_edges(1, {0, 4});
  graph.add_edges(2, {0, 4});
  graph.add_edges(3, {0, 4});
  graph.add_edges(4, {1, 2, 3});

  auto result =
      scea::algo::BetweennessCentrality::compute(graph, {0, 1, 2, 3, 4});
  {
    EXPECT_EQ(result[0], 3);
    EXPECT_EQ(result[1], 2 / 3.);
    EXPECT_EQ(result[2], 2 / 3.);
    EXPECT_EQ(result[3], 2 / 3.);
    EXPECT_EQ(result[4], 3);
  }
}

TEST(BC, IO) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph(5);

  /*
   *    0
   *  / | \
   * 1  2  3
   *  \ | /
   *    4
   */

  graph.add_edges(0, {1, 2, 3});
  graph.add_edges(1, {0, 4});
  graph.add_edges(2, {0, 4});
  graph.add_edges(3, {0, 4});
  graph.add_edges(4, {1, 2, 3});

  auto result =
      scea::algo::BetweennessCentrality::compute(graph, {0, 1, 2, 3, 4});
  {
    EXPECT_EQ(result[0], 3);
    EXPECT_EQ(result[1], 2 / 3.);
    EXPECT_EQ(result[2], 2 / 3.);
    EXPECT_EQ(result[3], 2 / 3.);
    EXPECT_EQ(result[4], 3);
  }
}
