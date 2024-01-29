/*
 * This file belongs to the Galois project, a C++ library for exploiting
 * parallelism. The code is being released under the terms of the 3-Clause BSD
 * License (a copy is located in LICENSE.txt at the top-level directory).
 *
 * Copyright (C) 2018, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 */

#include "galois/Galois.h"
#include "galois/gstl.h"
#include "galois/Reduction.h"
#include "galois/Timer.h"
#include "galois/graphs/LS_LC_CSR_64_Graph.h"
#include "galois/graphs/LC_CSR_64_Graph.h"
#include "galois/graphs/LC_CSR_Graph.h"
#include "galois/graphs/MorphGraph.h"
#include "galois/graphs/TypeTraits.h"
#include "Lonestar/BoilerPlate.h"
#include "Lonestar/BFS_SSSP.h"

#include <iostream>
#include <deque>
#include <queue>
#include <type_traits>

#include <benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

static const char* name = "Breadth-first Search";

static const char* desc =
    "Computes the shortest path from a source node to all nodes in a directed "
    "graph using a modified Bellman-Ford algorithm";

static const char* url = "breadth_first_search";

enum Exec { SERIAL, PARALLEL };

enum Algo { AsyncTile = 0, Async, SyncTile, Sync };

const char* const ALGO_NAMES[] = {"AsyncTile", "Async", "SyncTile", "Sync"};

constexpr static const bool TRACK_WORK          = false;
constexpr static const unsigned CHUNK_SIZE      = 256U;
constexpr static const ptrdiff_t EDGE_TILE_SIZE = 256;

template <typename G>
struct BFS_Algo {
  using Graph = G;
  using GNode = typename Graph::GraphNode;
  using BFS   = BFS_SSSP<Graph, unsigned int, false, EDGE_TILE_SIZE>;

  using UpdateRequest       = typename BFS::UpdateRequest;
  using Dist                = typename BFS::Dist;
  using SrcEdgeTile         = typename BFS::SrcEdgeTile;
  using SrcEdgeTileMaker    = typename BFS::SrcEdgeTileMaker;
  using SrcEdgeTilePushWrap = typename BFS::SrcEdgeTilePushWrap;
  using ReqPushWrap         = typename BFS::ReqPushWrap;
  using OutEdgeRangeFn      = typename BFS::OutEdgeRangeFn;
  using TileRangeFn         = typename BFS::TileRangeFn;

  struct EdgeTile {
    typename Graph::edge_iterator beg;
    typename Graph::edge_iterator end;
  };

  struct EdgeTileMaker {
    EdgeTile operator()(typename Graph::edge_iterator beg,
                        typename Graph::edge_iterator end) const {
      return EdgeTile{beg, end};
    }
  };

  struct NodePushWrap {
    template <typename C>
    void operator()(C& cont, const GNode& n, const char* const) const {
      (*this)(cont, n);
    }

    template <typename C>
    void operator()(C& cont, const GNode& n) const {
      cont.push(n);
    }
  };

  struct EdgeTilePushWrap {
    Graph& graph;

    template <typename C>
    void operator()(C& cont, const GNode& n, const char* const) const {
      BFS::pushEdgeTilesParallel(cont, graph, n, EdgeTileMaker{});
    }

    template <typename C>
    void operator()(C& cont, const GNode& n) const {
      BFS::pushEdgeTiles(cont, graph, n, EdgeTileMaker{});
    }
  };

  struct OneTilePushWrap {
    Graph& graph;

    template <typename C>
    void operator()(C& cont, const GNode& n, const char* const) const {
      (*this)(cont, n);
    }

    template <typename C>
    void operator()(C& cont, const GNode& n) const {
      EdgeTile t{graph.edge_begin(n, galois::MethodFlag::UNPROTECTED),
                 graph.edge_end(n, galois::MethodFlag::UNPROTECTED)};

      cont.push(t);
    }
  };

  template <bool CONCURRENT, typename T, typename P, typename R>
  void syncAlgo(Graph& graph, GNode source, const P& pushWrap,
                const R& edgeRange) {

    using Cont = typename std::conditional<CONCURRENT, galois::InsertBag<T>,
                                           galois::SerStack<T>>::type;
    using Loop = typename std::conditional<CONCURRENT, galois::DoAll,
                                           galois::StdForEach>::type;

    constexpr galois::MethodFlag flag = galois::MethodFlag::UNPROTECTED;

    Loop loop;

    auto curr = std::make_unique<Cont>();
    auto next = std::make_unique<Cont>();

    Dist nextLevel              = 0U;
    graph.getData(source, flag) = 0U;

    if (CONCURRENT) {
      pushWrap(*next, source, "parallel");
    } else {
      pushWrap(*next, source);
    }

    assert(!next->empty());

    while (!next->empty()) {

      std::swap(curr, next);
      next->clear();
      ++nextLevel;

      loop(
          galois::iterate(*curr),
          [&](const T& item) {
            for (auto e : edgeRange(item)) {
              auto dst      = graph.getEdgeDst(e);
              auto& dstData = graph.getData(dst, flag);

              if (dstData == BFS::DIST_INFINITY) {
                dstData = nextLevel;
                pushWrap(*next, dst);
              }
            }
          },
          galois::steal(), galois::chunk_size<CHUNK_SIZE>(),
          galois::loopname("Sync"));
    }
  }

  void run_sssp_all_nodes(const std::string& fn, pa c, const std::string& msg) {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_edge_list(fn, num_nodes, num_edges);
    Graph graph =
        Graph((uint32_t)num_nodes, num_edges,
              [ret](uint32_t n) { return ret[n].size(); },
              [ret](uint32_t n, uint64_t e) { return (uint32_t)ret[n][e]; },
              [](uint32_t n, uint64_t e) { return 0; });
    delete[] ret;
    run_test_and_benchmark<uint64_t>(
        msg, c, 6, [&graph] { return 0; },
        [&graph, this](uint64_t v) {
          for (GNode node : graph) {
            galois::do_all(galois::iterate(graph), [&graph](GNode n) {
              graph.getData(n) = BFS::DIST_INFINITY;
            });
            this->syncAlgo<false, GNode>(graph, node, NodePushWrap(),
                                         OutEdgeRangeFn{graph});
          }
        },
        [&graph](uint64_t v) {
          // GNode last; for(GNode n : graph) last = n;
          // bool veri = BFS::verify(graph, last); REQUIRE(veri == true);
        },
        [](uint64_t v) {});
  }

  template <typename T>
  void
  run_sssp_all_nodes_inc(uint64_t num_nodes, uint64_t num_edges,
                         const std::vector<std::pair<uint64_t, uint64_t>>& ret,
                         pa c, const std::string& msg, T add) {
    auto g = [num_nodes]() {
      return new Graph(
          num_nodes, 0, [](uint64_t n) { return 0; },
          [](uint64_t n, uint64_t e) { return 0; },
          [](uint64_t n, uint64_t e) { return 0; });
    };

    auto f = [ret, g, num_edges, msg, this, c, add](uint8_t i) {
      run_test_and_benchmark<Graph*>(
          msg + " Edit " + std::to_string(i), c, 8,
          [g, add, num_edges, i, ret] {
            Graph* graph = g();
            add(graph, 0, num_edges / 8 * (i - 1), num_edges / 8, ret);
            return graph;
          },
          [ret, num_edges, i, add](Graph* graph) {
            add(graph, num_edges / 8 * (i - 1), num_edges / 8, num_edges / 8,
                ret);
          },
          [](Graph* graph) {}, [](Graph* graph) { delete graph; });

      Graph* graph = g();
      add(graph, 0, num_edges / 8 * i, num_edges / 8, ret);

      run_test_and_benchmark<uint64_t>(
          msg + " Algo " + std::to_string(i), c, 8, [] { return 0; },
          [num_edges, this, graph](uint64_t v) {
            for (GNode node : *graph) {
              galois::do_all(galois::iterate(*graph), [graph](GNode n) {
                graph->getData(n) = BFS::DIST_INFINITY;
              });
              this->syncAlgo<false, GNode>(*graph, node, NodePushWrap(),
                                           OutEdgeRangeFn{*graph});
            }
          },
          [](uint64_t v) {}, [](uint64_t v) {});
      delete graph;
    };
    for (int i = 1; i <= 8; i++) {
      f(i);
    }
  }
};

/*
TEST_CASE( "Running Galois SSSP_BFS", "[bfs]" )
{
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  using LS_CSR_LOCK_OUT = galois::graphs::LS_LC_CSR_64_Graph<unsigned,
void>::with_no_lockable<true>::type; using LC_CSR_64_GRAPH =
galois::graphs::LC_CSR_64_Graph<unsigned, void>::with_no_lockable<true>::type;
  using LC_CSR_32_GRAPH = galois::graphs::LC_CSR_Graph<unsigned,
void>::with_no_lockable<true>::type; using MORPH_GRAPH     =
galois::graphs::MorphGraph<unsigned, void, true>::with_no_lockable<true>::type;
  BFS_Algo<LS_CSR_LOCK_OUT> lclo;
  BFS_Algo<LC_CSR_64_GRAPH> lc6g;
  BFS_Algo<LC_CSR_32_GRAPH> lc3g;
  BFS_Algo<MORPH_GRAPH>     morg;

  SECTION( "All Nodes Citeseer Galois" )
  {
    lclo.run_sssp_all_nodes("../graphs/citeseer.el", c, "Citeseer
LS_CSR_LOCK_OUT"); lc6g.run_sssp_all_nodes("../graphs/citeseer.el", c, "Citeseer
LC_CSR_64_GRAPH"); lc3g.run_sssp_all_nodes("../graphs/citeseer.el", c, "Citeseer
LC_CSR_32_GRAPH"); morg.run_sssp_all_nodes("../graphs/citeseer.el", c, "Citeseer
MORPH_GRAPH");
  }

  SECTION( "All Nodes Cora Galois" )
  {
    lclo.run_sssp_all_nodes("../graphs/cora.el", c, "Cora LS_CSR_LOCK_OUT");
    lc6g.run_sssp_all_nodes("../graphs/cora.el", c, "Cora LC_CSR_64_GRAPH");
    lc3g.run_sssp_all_nodes("../graphs/cora.el", c, "Cora LC_CSR_32_GRAPH");
    morg.run_sssp_all_nodes("../graphs/cora.el", c, "Cora MORPH_GRAPH");
  }

  SECTION( "All Nodes Flickr Galois" )
  {
    lclo.run_sssp_all_nodes("../graphs/flickr.el", c, "Flickr LS_CSR_LOCK_OUT");
    lc6g.run_sssp_all_nodes("../graphs/flickr.el", c, "Flickr LC_CSR_64_GRAPH");
    lc3g.run_sssp_all_nodes("../graphs/flickr.el", c, "Flickr LC_CSR_32_GRAPH");
    morg.run_sssp_all_nodes("../graphs/flickr.el", c, "Flickr MORPH_GRAPH");
  }

  SECTION( "All Nodes Yelp Galois" )
  {
    lclo.run_sssp_all_nodes("../graphs/yelp.el", c, "Yelp LS_CSR_LOCK_OUT");
    lc6g.run_sssp_all_nodes("../graphs/yelp.el", c, "Yelp LC_CSR_64_GRAPH");
    lc3g.run_sssp_all_nodes("../graphs/yelp.el", c, "Yelp LC_CSR_32_GRAPH");
    morg.run_sssp_all_nodes("../graphs/yelp.el", c, "Yelp MORPH_GRAPH");
  }
}
*/

template <typename Graph>
void regen_graph(Graph* graph, uint64_t lower, uint64_t upper,
                 uint64_t batch_size,
                 const std::vector<std::pair<uint64_t, uint64_t>>& edges) {
  std::vector<uint64_t>* edge_list = new std::vector<uint64_t>[graph->size()];
  for (uint64_t i = 0; i < upper && i < edges.size(); i++) {
    edge_list[edges[i].first].emplace_back(edges[i].second);
  }
  Graph* graph2 = new Graph(
      graph->size(), upper,
      [edge_list](uint64_t n) { return edge_list[n].size(); },
      [edge_list](uint64_t n, uint64_t e) { return (uint64_t)edge_list[n][e]; },
      [](uint32_t n, uint64_t e) { return 0; });
  swap(*graph, *graph2);
  delete graph2;
  delete[] edge_list;
}

TEST_CASE("Galois Edits SSSP_BFS", "[bfs]") {
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  using LS_CSR_LOCK_OUT =
      galois::graphs::LS_LC_CSR_64_Graph<unsigned,
                                         void>::with_no_lockable<true>::type;
  using LC_CSR_64_GRAPH =
      galois::graphs::LC_CSR_64_Graph<unsigned,
                                      void>::with_no_lockable<true>::type;
  using LC_CSR_32_GRAPH =
      galois::graphs::LC_CSR_Graph<unsigned,
                                   void>::with_no_lockable<true>::type;
  using MORPH_GRAPH =
      galois::graphs::MorphGraph<unsigned, void,
                                 true>::with_no_lockable<true>::type;
  BFS_Algo<LS_CSR_LOCK_OUT> lclo;
  BFS_Algo<LC_CSR_64_GRAPH> lc6g;
  BFS_Algo<LC_CSR_32_GRAPH> lc3g;
  BFS_Algo<MORPH_GRAPH> morg;

  auto morg_add = [](MORPH_GRAPH* graph, uint64_t lower, uint64_t upper,
                     uint64_t batch_size,
                     const std::vector<std::pair<uint64_t, uint64_t>>& edges) {
    for (uint64_t j = lower; j < upper && j < edges.size(); j++)
      graph->addEdge(edges[j].first, edges[j].second);
  };
  auto lc6g_add = regen_graph<LC_CSR_64_GRAPH>;
  auto lc3g_add = regen_graph<LC_CSR_32_GRAPH>;
  auto lclo_add = [](LS_CSR_LOCK_OUT* graph, uint64_t lower, uint64_t upper,
                     uint64_t batch_size,
                     const std::vector<std::pair<uint64_t, uint64_t>>& edges) {
    for (uint64_t start = lower; start < upper; start += batch_size) {
      std::priority_queue<uint64_t, std::vector<uint64_t>,
                          std::greater<uint64_t>>* edge_list =
          new std::priority_queue<uint64_t, std::vector<uint64_t>,
                                  std::greater<uint64_t>>[graph->size()];
      for (uint64_t i = start; i < start + batch_size && i < edges.size();
           i++) {
        edge_list[edges[i].first].push(edges[i].second);
      }
      graph->addEdges(edge_list);
      delete[] edge_list;
    }
  };

  SECTION("All Nodes Citeseer Galois") {
    std::string bench = "Citeseer";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret =
        el_file_to_rand_vec_edge("../graphs/citeseer.el", num_nodes, num_edges);

    lclo.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " MORPH_GRAPH", morg_add);
  }
  SECTION("All Nodes Cora Galois") {
    std::string bench = "Cora";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret =
        el_file_to_rand_vec_edge("../graphs/cora.el", num_nodes, num_edges);

    lclo.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " MORPH_GRAPH", morg_add);
  }
  SECTION("All Nodes Flickr Galois") {
    std::string bench = "Flickr";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret =
        el_file_to_rand_vec_edge("../graphs/flickr.el", num_nodes, num_edges);

    lclo.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " MORPH_GRAPH", morg_add);
  }
  SECTION("All Nodes Yelp Galois") {
    std::string bench = "Yelp";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret =
        el_file_to_rand_vec_edge("../graphs/yelp.el", num_nodes, num_edges);

    lclo.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_sssp_all_nodes_inc(num_nodes, num_edges, ret, c,
                                bench + " MORPH_GRAPH", morg_add);
  }
}
