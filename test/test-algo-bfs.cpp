#include <catch2/catch_test_macros.hpp>

#include "galois/Galois.h"
#include "scea/graph/morph.hpp"
#include "scea/algo/bfs.hpp"

TEST_CASE("SSSP_BFS correctly computes shortest paths", "[bfs]") {
  galois::SharedMemSys sys;
  galois::setActiveThreads(1);

  SECTION("small graph") {
    scea::graph::MorphGraph graph(7);

    /*
     * 0 - 1
     * | x |
     * 2 - 3
     *     | \
     *     4 - 5 - 6
     */

    REQUIRE(!graph.add_edges(0, {1, 2, 3}));
    REQUIRE(!graph.add_edges(1, {0, 2, 3}));
    REQUIRE(!graph.add_edges(2, {0, 1, 3}));
    REQUIRE(!graph.add_edges(3, {0, 1, 2, 4, 5}));
    REQUIRE(!graph.add_edges(4, {3, 5}));
    REQUIRE(!graph.add_edges(5, {3, 4, 6}));
    REQUIRE(!graph.add_edges(6, {5}));

    {
      auto result = scea::algo::SSSP_BFS::compute(graph, 1);
      REQUIRE(result[0] == 1);
      REQUIRE(result[1] == 0);
      REQUIRE(result[2] == 1);
      REQUIRE(result[3] == 1);
      REQUIRE(result[4] == 2);
      REQUIRE(result[5] == 2);
      REQUIRE(result[6] == 3);
    }
  }
}