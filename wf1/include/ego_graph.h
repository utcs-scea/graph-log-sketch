#include "galois/Galois.h"

#include <array>
//#include "torch/torch.h"

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

template<typename Graph, typename GNode>
void gen_1_hop_features(Graph* graph, GNode vertex)
{
  for(auto it = graph->edge_begin(vertex); it != graph->edge_end(vertex); it++)
  {
    auto& arr = graph->getData(vertex).arr_1_hop;
    arr[(size_t)graph->getEdgeData(it).type]++;
    auto dst = graph->getEdgeDst(it);
    arr[(size_t)graph->getData(dst).type]++;
  }
}

template<typename Graph, size_t N>
void gen_2_hop_features(Graph* graph)
{
  galois::do_all(galois::iterate(*graph),
      [&](auto& vertex)
      {
        gen_1_hop_features(graph, vertex);
      }, galois::steal()
      );
  galois::do_all(galois::iterate(*graph),
      [&](auto& vertex)
      {
        auto& arr = graph->getData(vertex).arr_2_hop;
        for(auto it = graph->edge_begin(vertex); it != graph->edge_end(vertex); it++)
        {
          auto  dst = graph->getEdgeDst(it);
          auto& hop_1 = graph->getData(dst).arr_1_hop;
          for(uint64_t i = 0; i < N; i++) arr[i] += hop_1[i];
        }
      }, galois::steal());
}
