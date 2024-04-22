// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include "galois/Galois.h"
#include "scea/graph/morph.hpp"
#include "scea/algo/bc.hpp"

using el = std::pair<uint64_t, std::vector<uint64_t>>;

TEST(BC, Short) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph;

  /*
   * 0 - 1 - 2
   */

  graph.ingest({el(0, {1}), el(1, {0, 2}), el(2, {1})});

  auto result = scea::algo::BetweennessCentrality::compute(graph);
  {
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 0);
  }
}

TEST(BC, Long) {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  scea::graph::MorphGraph graph;

  /*
   * 0 - 1 - 2 - 3
   */

  graph.ingest({el(0, {1}), el(1, {0, 2}), el(2, {1, 3}), el(3, {2})});

  auto result = scea::algo::BetweennessCentrality::compute(graph);
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

  scea::graph::MorphGraph graph;

  /*
   * 0 - 1
   * |   |
   * 2 - 3
   */

  graph.ingest({el(0, {1, 2}), el(1, {0, 3}), el(2, {0, 3}), el(3, {1, 2})});

  auto result = scea::algo::BetweennessCentrality::compute(graph);
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

  scea::graph::MorphGraph graph;

  /*
   * 0 - 1
   * | \
   * 2   3
   */

  graph.ingest({el(0, {1, 2, 3}), el(1, {0}), el(2, {0}), el(3, {0})});

  auto result = scea::algo::BetweennessCentrality::compute(graph);
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

  scea::graph::MorphGraph graph;

  /*
   *    0
   *  / | \
   * 1  2  3
   *  \ | /
   *    4
   */

  graph.ingest({el(0, {1, 2, 3}), el(1, {0, 4}), el(2, {0, 4}), el(3, {0, 4}),
                el(4, {1, 2, 3})});

  auto result = scea::algo::BetweennessCentrality::compute(graph);
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

  scea::graph::MorphGraph graph;

  /*
   *    0
   *  / | \
   * 1  2  3
   *  \ | /
   *    4
   */

  graph.ingest({el(0, {1, 2, 3}), el(1, {0, 4}), el(2, {0, 4}), el(3, {0, 4}),
                el(4, {1, 2, 3})});

  auto result = scea::algo::BetweennessCentrality::compute(graph);
  {
    EXPECT_EQ(result[0], 3);
    EXPECT_EQ(result[1], 2 / 3.);
    EXPECT_EQ(result[2], 2 / 3.);
    EXPECT_EQ(result[3], 2 / 3.);
    EXPECT_EQ(result[4], 3);
  }
}
