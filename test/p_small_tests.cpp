#include <catch2/catch_test_macros.hpp>
#include <test_bench.hpp>
#include "galois/graphs/LS_LC_CSR_64_Graph.h"
#include "galois/graphs/LC_CSR_64_Graph.h"

using LC_CSR = galois::graphs::LC_CSR_64_Graph<void, void>::with_no_lockable<true>::type;
using LS_CSR = galois::graphs::LS_LC_CSR_64_Graph<void, void>;

void test_edge_list(uint64_t num_nodes, std::vector<std::pair<uint64_t, uint64_t>> vec, uint64_t repeats = 1000)
{
  LC_CSR* refer;
  LS_CSR* in_quest;

  std::priority_queue<uint64_t,std::vector<uint64_t>,std::greater<uint64_t>>* sort_edge_list
    = new std::priority_queue<uint64_t,std::vector<uint64_t>,std::greater<uint64_t>>[num_nodes]();
  for(auto p : vec)
  {
    sort_edge_list[p.first].push(p.second);
  }

  refer = new LC_CSR(num_nodes, vec.size(), [sort_edge_list](uint64_t n){ return sort_edge_list[n].size(); },
      [sort_edge_list](uint64_t n, uint64_t e){ auto v = sort_edge_list[n].top(); sort_edge_list[n].pop(); return v; },
      [](uint64_t v, uint64_t e){ return 0; });

  for(uint64_t i = 0; i < repeats; i++)
  {
    in_quest = new LS_CSR(num_nodes, 0, [](uint64_t v){ return 0; }, [](uint64_t v, uint64_t e){ return 0; }, [](uint64_t v, uint64_t e){ return 0; });

    galois::for_each(
        galois::iterate(vec),
        [&] (std::pair<uint64_t, uint64_t> p, auto&)
        {
          std::priority_queue<uint64_t> pq;
          pq.push(p.second);
          in_quest->addEdges(p.first, pq);
        },galois::loopname("for_each_add_edges"),
        galois::no_pushes());

    for(uint64_t j = 0; j < in_quest->size(); j++)
    {
      REQUIRE( in_quest->getDegree(j) == refer->getDegree(j));
      auto in_quest_edge_iter = in_quest->out_edges(j).begin();
      auto in_quest_edge_end = in_quest->out_edges(j).end();
      auto refer_edge_iter = refer->out_edges(j).begin();
      auto refer_edge_end = refer->out_edges(j).end();

      while(in_quest_edge_iter != in_quest_edge_end && refer_edge_iter != refer_edge_end)
      {
        REQUIRE( in_quest->getEdgeDst(*in_quest_edge_iter) == refer->getEdgeDst(*refer_edge_iter) );
        in_quest_edge_iter++;
        refer_edge_iter++;
      }
      REQUIRE( in_quest_edge_iter == in_quest_edge_end );
      REQUIRE( refer_edge_iter == refer_edge_end );
    }
    delete in_quest;
  }
  delete refer;
}

TEST_CASE( "Parallel Edits Yield the Same Graph", "[parallel]" )
{
  galois::SharedMemSys SMS;

  SECTION( "Add Two Edges" )
  {
    test_edge_list(2, {{0,1}, {1,0}});
  }
  SECTION( "Add Four Edges")
  {
    test_edge_list(2, {{0,0}, {0,1}, {1,0}, {1,1}});
  }
  SECTION( "Add Citeseer")
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto blah = el_file_to_rand_vec_edge("../graphs/citeseer.el", num_nodes, num_edges);
    test_edge_list(num_nodes, blah);
  }
  SECTION( "Add Cora")
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto blah = el_file_to_rand_vec_edge("../graphs/cora.el", num_nodes, num_edges);
    test_edge_list(num_nodes, blah);
  }
  SECTION( "Add Flickr")
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto blah = el_file_to_rand_vec_edge("../graphs/flickr.el", num_nodes, num_edges);
    test_edge_list(num_nodes, blah);
  }
  SECTION( "Add Yelp")
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto blah = el_file_to_rand_vec_edge("../graphs/yelp.el", num_nodes, num_edges);
    test_edge_list(num_nodes, blah);
  }
}
