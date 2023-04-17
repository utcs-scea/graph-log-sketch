#include <catch2/catch_test_macros.hpp>
#include <test_bench.hpp>
#include "galois/graphs/LS_LC_CSR_64_Graph.h"
#include "galois/graphs/LC_CSR_64_Graph.h"
#include <graph_benchmark_style.hpp>

#include "ego_graph.h"
#include <tuple>

using LS_CSR = galois::graphs::LS_LC_CSR_64_Graph<VertexType<(size_t)TYPES::NONE>, EdgeType>::with_out_of_line_lockable<true>::type;
using GNode = LS_CSR::GraphNode;

enum class TYPES
{
  PERSON,
  FORUMEVENT,
  FORUM,
  PUBLICATION,
  TOPIC,
  PURCHASE,
  SALE,
  AUTHOR,
  WRITTENBY,
  INCLUDES,
  INCLUDEDIN,
  HASTOPIC,
  TOPICIN,
  HASORG,
  ORGIN,
  NONE
};

template<size_t N>
struct VertexType
{
  TYPES type;
  std::array<uint64_t, N> arr_1_hop{0};
  std::array<uint64_t, N> arr_2_hop{0};
};

struct EdgeType
{
  TYPES type;

  EdgeType (TYPES _type) :type(_type){}
};


template<typename Graph>
void edge_list_to_graph(Graph** g, size_t num_nodes, std::vector<TYPES> nodesTypes,
    std::vector<std::tuple<uint64_t, uint64_t, TYPES>> vec)
{
  std::vector<std::pair<uint64_t, TYPES>>* edge_list
    = new std::vector<std::pair<uint64_t,TYPES>>[num_nodes]();
  for(auto p : vec)
  {
    edge_list[std::get<0>(p)].emplace_back(std::get<1>(p), std::get<2>(p));
  }
  *g = new Graph(num_nodes, vec.size(), [edge_list](uint64_t n){ return edge_list[n].size();},
      [edge_list](uint64_t v, uint64_t e){ return edge_list[v][e].first; },
      [edge_list](uint64_t v, uint64_t e){ return EdgeType(edge_list[v][e].second); });

  galois::do_all(galois::iterate(**g),
      [&](auto& vertex)
      {
        (*g)->getData(vertex).type = nodesTypes[vertex];
      }, galois::steal());

}

TEST_CASE( "Simple Feature_Vector", "[wf1]")
{
  galois::SharedMemSys SMS;
  galois::setActiveThreads(1);

  SECTION( "Add Two Edges" )
  {
    LS_CSR* g;
    edge_list_to_graph(&g, 2, {TYPES::PUBLICATION, TYPES::PERSON}, {{0,1, TYPES::WRITTENBY}, {1,0, TYPES::WRITTENBY}});

    gen_2_hop_features<LS_CSR, (size_t)TYPES::NONE>(g);

    uint64_t arr_1_hop_0[(size_t) TYPES::NONE] = {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0};
    uint64_t arr_1_hop_1[(size_t) TYPES::NONE] = {0,0,0,1,0,0,0,0,1,0,0,0,0,0,0};

    auto& arr0_hop1 = g->getData(0).arr_1_hop;
    auto& arr1_hop1 = g->getData(1).arr_1_hop;
    auto& arr0_hop2 = g->getData(0).arr_2_hop;
    auto& arr1_hop2 = g->getData(1).arr_2_hop;
    for(uint64_t i = 0; i < (size_t) TYPES::NONE; i++)
    {
      REQUIRE( arr_1_hop_0[i] == arr0_hop1[i]);
      REQUIRE( arr_1_hop_1[i] == arr1_hop1[i]);
      REQUIRE( arr_1_hop_1[i] == arr0_hop2[i]);
      REQUIRE( arr_1_hop_0[i] == arr1_hop2[i]);
    }

    //delete g;
  }
}
