#include <catch2/catch_test_macros.hpp>
#include "graph-mutable-va.hpp"
#include <test_bench.hpp>

#include <queue>
#include <unordered_set>
#include <thread>
#include <system_error>
#include <iostream>

using namespace std;

typedef pair<uint64_t, uint64_t> nodeDist;

//assume for now every edge has length 1.
template<typename T>
uint64_t shortest_length_dijkstras(uint64_t src, uint64_t dest, T* g)
{
  priority_queue<nodeDist, vector<nodeDist>, greater<nodeDist> > pq;
  unordered_set <uint64_t> visited_nodes;
  auto dist = 0;
  auto curr_node = g->get_node(src);
  visited_nodes.insert(src);
  curr_node->lock();
  uint64_t curr = src;
  while(curr != dest)
  {
    auto start = curr_node->start.get_value();
    auto stop  = curr_node->stop;
    for(uint64_t e = start; e < stop; e++)
    {
      //lock all edges
      g->get_edge(e)->lock();
    }
    //unlock the current node (should increase parrallelism
    curr_node->unlock();
    for(uint64_t e = start; e < stop; e++)
    {
      auto edge         = g->get_edge(e);
      auto edge_t       = edge->is_tomb();
      auto next         = edge->get_dest();
      auto next_node    = g->get_node(next);
      if(!edge_t)
      {
        //Check if we can lock this node
        if(visited_nodes.find(next) == visited_nodes.end())
        {
          auto next_node_t  = !next_node->lock();
          if(next_node_t)
          {
            edge->set_tomb();
            next_node->unlock();
          }
          else
          {
            visited_nodes.insert(next);
            //If we push we hold the lock
            pq.push(make_pair(dist+1, next));
          }
        }
      }
      edge->unlock();
    }
    if(pq.empty()) return UINT64_MAX;
    dist = pq.top().first;
    curr = pq.top().second;
    curr_node = g->get_node(curr);
    pq.pop();
  }
  assert(curr == dest);
  g->get_node(curr)->unlock();
  //Unlock Everything on the queue
  while(!pq.empty())
  {
    auto t = pq.top().second;
    pq.pop();
    g->get_node(t)->unlock();
  }
  return dist;
}

TEST_CASE( "Running Simple Dijkstras", "[bfs]")
{
  pa c = create_counters();

  SECTION( "Trivial Graph" )
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
          RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>();
          r->g->ingestNode();
          return r;
        },
        [](RetVal* r){ r->n = shortest_length_dijkstras(0,0,r->g); },
        [](RetVal* r){ REQUIRE( r->n == 0 ); },
        [](RetVal* r){ delete r->g; free(r); });
    };
    runner("Trivial Graph Dijkstras", f);
  }

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

  SECTION( "All Pairs of Nodes Citeseer" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        Graph<par, ehp>* g;
        uint64_t         n;
      };
      run_test_and_benchmark<RetVal*>(s,c,6,
        []{
          uint64_t end;
          RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>();
          r->g->ingestSubGraphFromELFile("../graphs/citeseer.el");
          return r;
        },
        [](RetVal* r){ for(uint64_t i = 0; i < r->g->get_num_nodes(); i++)
                         for(uint64_t j = 0; j < r->g->get_num_nodes(); j++)
                           r->n = shortest_length_dijkstras(i, j, r->g); },
        [](RetVal* r){ REQUIRE( r->n == 0 ); },
        [](RetVal* r){ delete r->g; free(r); });
    };

    runner("All Pairs of Nodes Citeseer Dijkstra", f);
  }

  SECTION( "All Pairs of Nodes Cora" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        Graph<par, ehp>* g;
        uint64_t         n;
      };
      run_test_and_benchmark<RetVal*>(s,c,6,
        []{
          uint64_t end;
          RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>();
          r->g->ingestSubGraphFromELFile("../graphs/cora.el");
          return r;
        },
        [](RetVal* r){ for(uint64_t i = 0; i < r->g->get_num_nodes(); i++)
                         for(uint64_t j = 0; j < r->g->get_num_nodes(); j++)
                           r->n = shortest_length_dijkstras(i, j, r->g); },
        [](RetVal* r){ REQUIRE( r->n == 0 ); },
        [](RetVal* r){ delete r->g; free(r); });
    };

    runner("All Pairs of Nodes Cora Dijkstra", f);
  }
}
