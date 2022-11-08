#include <catch2/catch_test_macros.hpp>
#include <benchmark.hpp>

#include <queue>
#include <unordered_set>
#include <thread>
#include <system_error>
#include <iostream>
#include "galois/graphs/LC_CSR_Graph.h"
#include "galois/substrate/SharedMem.h"

using namespace std;

typedef pair<uint64_t, uint64_t> nodeDist;
typedef galois::graphs::LC_CSR_Graph<void, void> Graph;

template<typename T>
uint64_t shortest_length_dijkstras(uint64_t src, uint64_t dest, T* g)
{
  priority_queue<nodeDist, vector<nodeDist>, greater<nodeDist> > pq;
  unordered_set <uint64_t> visited_nodes;
  auto dist = 0;
  visited_nodes.insert(src);
  uint64_t curr = src;
  while(curr != dest)
  {
    for (Graph::edge_iterator edge : g->out_edges(curr))
    {
      auto next = g->getEdgeDst(edge);
      if(visited_nodes.find(next) == visited_nodes.end())
      {
        visited_nodes.insert(next);
        pq.push(make_pair(dist+1, next));
      }
    }
    if(pq.empty()) return UINT64_MAX;
    dist = pq.top().first;
    curr = pq.top().second;
    pq.pop();
  }
  assert(curr == dest);

  return dist;
}

TEST_CASE( "Compare Our Version to Galois", "[bfs]")
{
  pa c = create_counters();

  auto shmem = galois::substrate::SharedMem();

  SECTION( "All Pairs of Nodes Galois Citeseer" )
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_edge_list("../graphs/citeseer.el", num_nodes, num_edges);

    Graph g = Graph((uint32_t) num_nodes, num_edges, [ret](uint32_t n){return ret[n].size();},
        [ret](uint32_t n, uint64_t e) {return (uint32_t) ret[n][e];}, [](uint32_t n, uint64_t e){ return 0; });

    delete[] ret;

    auto f = [&g, c](std::string s)
    {
      struct RetVal { uint64_t n; };
      run_test_and_benchmark<RetVal*>(s, c, 8,
        []{ return (RetVal*) malloc(sizeof(RetVal)); },
        [&g](RetVal* r){ for(uint64_t i = 0; i < g.size(); i++)
                           for(uint64_t j = 0; j < g.size(); j++)
                             r->n = shortest_length_dijkstras(i, j, &g); },
        [](RetVal* r){},
        [](RetVal* r){ free(r); });
    };

    f("All Pairs of Nodes Galois Citeseer");
  }

  SECTION( "All Pairs of Nodes Galois Cora" )
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_edge_list("../graphs/cora.el", num_nodes, num_edges);

    Graph g = Graph((uint32_t) num_nodes, num_edges, [ret](uint32_t n){return ret[n].size();},
        [ret](uint32_t n, uint64_t e) {return (uint32_t) ret[n][e];}, [](uint32_t n, uint64_t e){ return 0; });

    delete[] ret;

    auto f = [&g, c](std::string s)
    {
      struct RetVal { uint64_t n; };
      run_test_and_benchmark<RetVal*>(s, c, 9,
        []{ return (RetVal*) malloc(sizeof(RetVal)); },
        [&g](RetVal* r){ for(uint64_t i = 0; i < g.size(); i++)
                           for(uint64_t j = 0; j < g.size(); j++)
                             r->n = shortest_length_dijkstras(i, j, &g); },
        [](RetVal* r){},
        [](RetVal* r){ free(r); });
    };

    f("All Pairs of Nodes Galois Cora");

  }

  SECTION( "All Pairs of Nodes Galois Yelp" )
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_edge_list("../graphs/yelp.el", num_nodes, num_edges);

    Graph g = Graph((uint32_t) num_nodes, num_edges, [ret](uint32_t n){return ret[n].size();},
        [ret](uint32_t n, uint64_t e) {return (uint32_t) ret[n][e];}, [](uint32_t n, uint64_t e){ return 0; });

    delete[] ret;

    auto f = [&g, c](std::string s)
    {
      struct RetVal { uint64_t n; };
      run_test_and_benchmark<RetVal*>(s, c, 9,
        []{ return (RetVal*) malloc(sizeof(RetVal)); },
        [&g](RetVal* r){ for(uint64_t i = 0; i < g.size(); i++)
                           for(uint64_t j = 0; j < g.size(); j++)
                             r->n = shortest_length_dijkstras(i, j, &g); },
        [](RetVal* r){},
        [](RetVal* r){ free(r); });
    };

    f("All Pairs of Nodes Galois Yelp");
  }
}
