#ifndef EGO_GRAPH_H_
#define EGO_GRAPH_H_

#include "galois/Galois.h"

#include <array>
#include "torch/torch.h"

// enum class TYPES
// {
//   PERSON,
//   FORUMEVENT,
//   FORUM,
//   PUBLICATION,
//   TOPIC,
//   PURCHASE,
//   SALE,
//   AUTHOR,
//   WRITTENBY,
//   INCLUDES,
//   INCLUDEDIN,
//   HASTOPIC,
//   TOPICIN,
//   HASORG,
//   ORGIN,
//   NONE
// };

// template<size_t N>
// struct VertexType
// {
//   TYPES type;
//   std::array<uint64_t, N> arr_1_hop{0};
//   std::array<uint64_t, N> arr_2_hop{0};
// };

// struct EdgeType
// {
//   TYPES type;

//   EdgeType (TYPES _type) :type(_type){}
// };

template<typename Graph, typename GNode>
void gen_1_hop_features(Graph* graph, GNode vertex)
{
  auto& arr = graph->getData(vertex).arr_1_hop;
  for(auto it = graph->edge_begin(vertex); it != graph->edge_end(vertex); it++)
  {
    arr[(size_t)graph->getEdgeData(it).type]++;
    auto dst = graph->getEdgeDst(it);
    arr[(size_t)graph->getData(dst).type]++;
  }
}

template<typename Graph, size_t N>
void gen_2_hop_features(Graph* graph)
{
  //Load from storage with 1_hop_neighbors
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

//std::unordered_set<int64_t>

torch::Tensor
export_edge_list_to_torch(std::pair<std::vector<int64_t>,std::vector<int64_t>> edge_list)
{
  int64_t num_edges = edge_list.first.size();
  assert(num_edges == edge_list.second.size());
  auto options = torch::TensorOptions().dtype(torch::kLong);
  auto result = torch::zeros({2, num_edges}, options);
  result.slice(0,0,1) =
    torch::from_blob(edge_list.first.data(), {num_edges}, options).clone();

  result.slice(0,1,2) =
    torch::from_blob(edge_list.second.data(), {num_edges}, options).clone();
  return result;
}


//Note this is a bad implementation do not use it unless you explicitly have to.
template<typename Graph, typename VertexType, typename EdgeType>
std::tuple<std::pair<std::vector<int64_t>,std::vector<int64_t>>, std::unordered_map<uint64_t, VertexType>>
_build_ego_graph_serial(const Graph* g, uint64_t start, uint64_t end, std::vector<uint64_t> levels = {5,3,2,1,0})
{
  uint64_t localID = 0;
  std::vector<uint64_t> frontier;
  std::unordered_map<uint64_t, VertexType> vertex_set;
  std::unordered_set<std::pair<uint64_t, uint64_t>> edges;

  for(int64_t root = start; root < end; root++)
  {
    VertexType V = g.getData((uint64_t) root);
    uint64_t V_localID = localID++;
    V.lid = V_localID;
    vertex_set[root] = V;
    edges.insert(std::make_pair(V_localID, V_localID));
  }

  uint64_t level = 0;
  auto next = frontier.begin();
  auto end_of_level = frontier.end();
  uint64_t added_neighbors = 0;
  uint64_t max_neighbors = levels[level];

  while(level < levels.size())
  {
    if(next == end_of_level) break;
    uint64_t glbID = *(next++);
    VertexType V = vertex_set[glbID];
    uint64_t V_localID = V.id;

    auto startEL = g.edge_begin(glbID);
    auto endEL = g.edge_end(glbID);

    for(auto currEL = startEL; startEL != endEL; currEL++)
    {
      uint64_t glbID = g.getEdgeDst(currEL);
      VertexType U = g.getData(glbID);

      if(vertex_set.find(glbID) == vertex_set.end())
      {
        if(max_neighbors <= added_neighbors) continue;

        added_neighbors++;
        uint64_t U_localID = localID++;
        U.id = U_localID;
        vertex_set[glbID] = U;
        frontier.push_back(glbID);

        //self, forwards, backwards
        edges.insert(std::make_pair(U_localID, U_localID));
        edges.insert(std::make_pair(V_localID, U_localID));
        edges.insert(std::make_pair(U_localID, V_localID));
      } else
      {
        uint64_t U_localID = vertex_set[glbID].id;

        //forwards, backwards
        edges.insert(std::make_pair(V_localID, U_localID));
        edges.insert(std::make_pair(U_localID, V_localID));
      }
    }

    if(next == end_of_level)
    {
      level++;
      added_neighbors = 0;
      max_neighbors = levels[level];
      end_of_level = frontier.end();
    }

  }

  uint64_t num_edges = edges.size();
  uint64_t num_verts = vertex_set.size();
  std::pair<std::vector<int64_t>, std::vector<int64_t>> edge_list;

  for(auto edge : edges)
  {
    edge_list.first.push_back((int64_t) edge.first);
    edge_list.second.push_back((int64_t) edge.second);
  }

  return std::tuple(export_edge_list_to_torch(edge_list), vertex_set);

}

size_t u64_u8_hash(uint64_t val)
{
  //create a hash using a random number of uint8_t
  uint8_t ret = 0;
  ret |= (val & 0x3F); //bottom 6
  ret |= ((val >> 16) & 0x3) <<6; // bits 17, 18
  return (size_t) ret;
}

template<uint64_t N, typename Key, size_t (*hash)(Key), typename Val, bool CONCURRENT=true>
struct bucketed_unordered_map
{
  std::array<galois::substrate::PaddedLock<CONCURRENT>, N> locks;
  std::array<std::unordered_map<Key, Val>, N> hash_table;

  //returns false if already inserted, returns true if not
  // Should do exactly 1 lookup into the hashtable
  template<typename T, typename F, bool use_lock = CONCURRENT>
  bool insert_exclusive_reference(Key k, Val& v, Val** ret, T t, F f)
  {
    bool inserted = false;
    size_t index = hash(k) % N;

    if(use_lock) locks[index].lock();
    auto search = hash_table[index].find(k);
    if(search == hash_table[index].end())
    {
      //has to be done in one line to avoid excess hash-lookups
      *ret = &(hash_table[index][k] = v); //This should copy
      inserted = true;
      t(*ret);
    }
    else { *ret = &(*search); f(*ret); } //iterator to pointer conversion
    if(use_lock) locks[index].unlock();
    return inserted;
  }

  template<bool use_lock = CONCURRENT>
  bool insert_exclusive_reference(Key k, Val& v, Val** ret)
  {return insert_exclusive_reference<use_lock>(k, v, ret, [](){}, [](){});}
};

template<typename Graph, typename VertexType, typename EdgeType>
std::tuple<torch::Tensor, bucketed_unordered_map<256, uint64_t, u64_u8_hash, VertexType>>
_build_ego_graph_parallel(const Graph * g, uint64_t start, uint64_t end, std::vector<uint64_t> levels = {5,3,2,1,0})
{
  std::atomic<uint64_t> localID = 0;
  std::atomic<uint64_t> edgeID  = 0;

  galois::InsertBag<std::pair<uint64_t, const VertexType&>> curr_front, next_front;
  galois::InsertBag<std::tuple<uint64_t,uint64_t,uint64_t>> edges;

  bucketed_unordered_map<256, uint64_t, u64_u8_hash, VertexType> vertex_set;

  //setup initials
  galois::do_all(galois::iterate(start, end),
    [&](uint64_t root)
    {
      VertexType V;
      bool inserted;
      inserted = vertex_set.insert_exclusive_reference(g.getData(root), &V);
      if(!inserted) exit(-2); //TODO proper error logging

      uint64_t V_localID = localID.fetch_add(1, std::memory_order_relaxed);
      V->lid = V_localID;

      next_front.emplace_back(root, *V);
      uint64_t eid = edgeID.fetch_add(1, std::memory_order_relaxed);
      edges.emplace_back(eid, V_localID, V_localID);
    });

  for(uint64_t level = 0; level < levels.size(); level++)
  {
    std::atomic<uint64_t> added_neighbors = 0;
    uint64_t max_neighbors = levels[level];
    std::swap(next_front, curr_front);

    galois::do_all(galois::iterate(curr_front),
      [&](std::pair<uint64_t, const VertexType&> src)
      {
        uint64_t glbID = src.first;
        uint64_t V_localID = src.second.lid;
        auto startEL = g.edge_begin(glbID);
        auto endEL = g.edge_end(glbID);
        for(auto currEL = startEL; startEL != endEL; currEL++)
        {
          uint64_t glbID = g.getEdgeDst(currEL);

          VertexType U;
          bool enough_space = false;
          //Enables everyone to get the value published
          auto t = [&enough_space, &added_neighbors, &max_neighbors, &localID](VertexType* U) {
                     if(added_neighbors.fetch_add(1, std::memory_order_relaxed) < max_neighbors)
                     {
                       enough_space = true;
                       U->lid = localID.fetch_add(1, std::memory_order_relaxed);
                     }
                   };
          bool inserted = vertex_set.insert_exclusive_reference(glbID, g.getData(glbID), &U, t, [](){});

          if(inserted)
          {
            if(!enough_space) continue;

            uint64_t U_localID = U->lid;
            next_front.emplace_back(glbID, *U);

            //self
            uint64_t eid = edgeID.fetch_add(1, std::memory_order_relaxed);
            edges.emplace_back(eid, U_localID, U_localID);

          }

          uint64_t U_localID = U->lid;

          //forwards, backwards
          uint64_t eid = edgeID.fetch_add(1, std::memory_order_relaxed);
          edges.emplace_back(eid, V_localID, U_localID);
          eid = edgeID.fetch_add(1, std::memory_order_relaxed);
          edges.emplace_back(eid, U_localID, V_localID);
        }
      }, galois::steal());
  }

  uint64_t num_edges = edgeID.load(std::memory_order_relaxed);
  std::pair<std::vector<int64_t>, std::vector<int64_t>> edge_list;
  edge_list.first.resize(num_edges);
  edge_list.second.resize(num_edges);
  galois::do_all(galois::iterate(edges),
      [&](std::tuple<uint64_t, uint64_t, uint64_t> tuple)
      {
        edge_list.first [std::get<0>(tuple)] = (int64_t) std::get<1>(tuple);
        edge_list.second[std::get<0>(tuple)] = (int64_t) std::get<2>(tuple);
      }, galois::steal());

  return std::tuple(export_edge_list_to_torch(edge_list), vertex_set);

}

template<typename Graph, typename VertexType, typename EdgeType>
std::tuple<torch::Tensor, std::unordered_map<uint64_t, VertexType>>
_build_ego_graph(const Graph* g, uint64_t start, uint64_t end, std::vector<uint64_t> levels = {5,3,2,1,0})
{
  auto tup = _build_ego_graph_serial<Graph, VertexType, EdgeType>(g, start, end, levels);
  return std::tuple(std::get<0>(tup),std::get<1>(tup));
}

#endif
