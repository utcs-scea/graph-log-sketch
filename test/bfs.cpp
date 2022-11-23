#include <catch2/catch_test_macros.hpp>
#include "graph-mutable-va.hpp"
#include <test_bench.hpp>

#include <queue>
#include <unordered_set>
#include <thread>
#include <system_error>
#include <iostream>
#include "galois/gstl.h"
#include "galois/Loops.h"

using namespace std;

typedef pair<uint64_t, uint64_t> nodeDist;

//assume for now every edge has weight 1.
template<typename T>
void sssp_bfs(uint64_t src, T* g)
{
  using Cont = galois::SerStack<uint64_t>;
  using Loop = galois::StdForEach;
  //using Cont = vector<uint64_t>;
  Loop loop;
  auto currSt = std::make_unique<Cont>();
  auto nextSt = std::make_unique<Cont>();

  uint64_t level = 0U;

  auto& src_node = *g->get_node(src);
  src_node.lock();
  src_node.val  = 0U;

  nextSt->push(src);

  while(!nextSt->empty())
  {
    std::swap(currSt, nextSt);
    nextSt->clear();
    ++level;

    loop(galois::iterate(*currSt),
      [&](const uint64_t& curr)
      {
        auto& curr_node = *g->get_node(curr);
        auto start = curr_node.start.get_value();
        auto stop  = curr_node.stop;
        for(uint64_t e = start; e < stop; e++)
        {
          auto edge         = g->get_edge(e);
          edge->lock();
          auto edge_t       = edge->is_tomb();
          if(!edge_t)
          {
            auto next         = edge->get_dest();
            auto& next_n      = *g->get_node(next);
            if(next_n.val == UINT64_MAX)
            {
              next_n.lock();
              next_n.val = level;
              nextSt->push(next);
            }
          }
          edge->unlock();
        }
        curr_node.unlock();
      });
  }
}

TEST_CASE( "Running Simple SSSP_BFS", "[bfs]")
{
  pa c = create_counters();

  SECTION( "Trivial Graph" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      auto g = new Graph<par, ehp, uint64_t>();
      g->ingestNode();

      run_test_and_benchmark<uint64_t>(s,c,3,
        [g]
        {
          g->get_node(0)->val = UINT64_MAX;
          return 0;
        },
        [g](uint64_t v){ sssp_bfs(0,g); },
        [g](uint64_t v){ REQUIRE( g->get_node(v)->val == 0 ); },
        [g](uint64_t v){});
      delete g;
    };
    runner("Trivial Graph Dijkstras", f);

  }

  /*
  SECTION( "Trivial Queries" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        Graph<par, ehp>* g;
        uint64_t         n[100];
      };
      run_test_and_benchmark<RetVal*>(s,c,3,
        []
        {
          uint64_t end;
          RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>();
          r->g->ingestNodes(100, end);
          return r;
        },
        [](RetVal* r){ for(uint8_t i = 0; i < 100; i++) r->n[i] = shortest_length_dijkstras(i,i,r->g); },
        [](RetVal* r){ for(uint8_t i = 0; i < 100; i++) REQUIRE( r->n[i] == 0 ); },
        [](RetVal* r){ delete r->g; free(r); });
    };

    runner("Trivial Queries Dijkstras", f);
  }

  SECTION( "One Edge Queries" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        Graph<par, ehp>* g;
        uint64_t         n;
      };
      run_test_and_benchmark<RetVal*>(s,c,3,
        []
        {
          uint64_t end;
          RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>();
          r->g->ingestNodes(2, end);
          end = 1;
          r->g->ingestEdges(1, 0, &end);
          return r;
        },
        [](RetVal* r){ r->n = shortest_length_dijkstras(0,1,r->g); },
        [](RetVal* r){ REQUIRE( r->n == 1 ); },
        [](RetVal* r){ delete r->g; free(r); });
    };

    runner("One Edge Dijkstras", f);
  }

  SECTION( "Many One Edge Queries" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        Graph<par, ehp>* g;
        uint64_t         n[100];
      };
      run_test_and_benchmark<RetVal*>(s,c,3,
        []
        {
          uint64_t end;
          RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>();
          r->g->ingestNodes(101, end);
          for(uint64_t i = 1; i < 101; i++)
            r->g->ingestEdges(1, i-1, &i);
          return r;
        },
        [](RetVal* r){ for(uint8_t i = 0; i < 100; i++) r->n[i] = shortest_length_dijkstras(i,i+1,r->g); },
        [](RetVal* r){ for(uint8_t i = 0; i < 100; i++) REQUIRE( r->n[i] == 1 ); },
        [](RetVal* r){ delete r->g; free(r); });
    };

    runner("Many One Edge Dijkstras", f);
  }

  SECTION( "100 Node Simple Query" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        Graph<par, ehp>* g;
        uint64_t         n;
      };
      run_test_and_benchmark<RetVal*>(s,c,3,
        []{
          uint64_t end;
          RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>();
          r->g->ingestNodes(101, end);
          for(uint64_t i = 1; i < 101; i++)
            r->g->ingestEdges(1, i-1, &i);
          return r;
        },
        [](RetVal* r){ r->n = shortest_length_dijkstras(0, 100, r->g); },
        [](RetVal* r){ REQUIRE( r->n == 100 ); },
        [](RetVal* r){ delete r->g; free(r); });
    };

    runner("A Hundred Nodes Simple Dijkstra", f);
  }
  */

  SECTION( "All Nodes Citeseer SSSP_BFS" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      auto g = new Graph<par, ehp, uint64_t>();
      g->ingestSubGraphFromELFile("../graphs/citeseer.el");
      run_test_and_benchmark<uint64_t>(s,c,6,
        []{return 0;},
        [g](uint64_t v)
        {
          for(uint64_t i = 0; i < g->get_num_nodes(); i++)
          {
            for(uint64_t ni = 0; ni < g->get_num_nodes(); ni++) g->get_node(ni)->val = UINT64_MAX;
            sssp_bfs(i,g);
          }
        },
        [](uint64_t v){},
        [](uint64_t v){});
      delete g;
    };

    runner("All Nodes Citeseer SSSP_BFS", f);
  }

  SECTION( "All Nodes Cora SSSP_BFS" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      auto g = new Graph<par, ehp, uint64_t>();
      g->ingestSubGraphFromELFile("../graphs/cora.el");
      run_test_and_benchmark<uint64_t>(s,c,6,
        []{return 0;},
        [g](uint64_t v)
        {
          for(uint64_t i = 0; i < g->get_num_nodes(); i++)
          {
            for(uint64_t ni = 0; ni < g->get_num_nodes(); ni++) g->get_node(ni)->val = UINT64_MAX;
            sssp_bfs(i,g);
          }
        },
        [](uint64_t v){},
        [](uint64_t v){});
      delete g;
    };

    runner("All Nodes Cora SSSP_BFS", f);
  }

  /*
  SECTION( "All Nodes Yelp SSSP_BFS" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      auto g = new Graph<par, ehp, uint64_t>();
      g->ingestSubGraphFromELFile("../graphs/yelp.el");
      run_test_and_benchmark<uint64_t>(s,c,6,
        []{return 0;},
        [g](uint64_t v)
        {
          for(uint64_t i = 0; i < g->get_num_nodes(); i++)
          {
            for(uint64_t ni = 0; ni < g->get_num_nodes(); ni++) g->get_node(ni)->val = UINT64_MAX;
            sssp_bfs(i,g);
          }
        },
        [](uint64_t v){},
        [](uint64_t v){});
      delete g;
    };

    runner("All Nodes Yelp SSSP_BFS", f);
  }
  */
}
