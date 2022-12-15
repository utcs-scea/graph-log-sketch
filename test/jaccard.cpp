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
#include <type_traits>
#include <queue>

#include <benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

struct JaccardStatistics
{
  /// The maximum similarity excluding the comparison node.
  double max_similarity;

  /// The minimum similarity
  double min_similarity;

  /// The average similarity excluding the comparison node.
  double average_similarity;

  /// Print the statistics in a human readable form.
  void print(std::ostream& os = std::cout)
  {
    os << "Maximum similarity = " << max_similarity << std::endl;
    os << "Minimum similarity = " << min_similarity << std::endl;
    os << "Average similarity = " << average_similarity << std::endl;
  }
};

template<typename Graph>
struct Jaccard_Algo
{

  using GNode = typename Graph::GraphNode;

  JaccardStatistics compute(Graph& graph, GNode compare_node)
  {
    galois::GReduceMax<double>    max_similarity;
    galois::GReduceMin<double>    min_similarity;
    galois::GAccumulator<double>  total_similarity;
    galois::do_all(
        galois::iterate(graph),
        [&](const GNode& n)
        {
          double similarity = graph.getData(n);
          if (n != compare_node)
          {
            max_similarity.update(similarity);
            min_similarity.update(similarity);
            total_similarity += similarity;
          }
        }, galois::loopname("Jaccard Stats"));
    return JaccardStatistics {
      max_similarity.reduce(), min_similarity.reduce(),
      total_similarity.reduce() /(graph.size() - 1)};
  }

  struct IntersectWithSortedEdgeList
  {
  private:
    const GNode base_;
    Graph& graph_;

  public:
    IntersectWithSortedEdgeList(Graph& graph, GNode base)
        : base_(base), graph_(graph) {}

    const uint64_t operator() (GNode n2) {
      uint64_t intersection_size = 0;
      // Iterate over the edges of both n2 and base in sync, based on the
      // assumption that edges lists are sorted.
      auto edges_n2_iter = graph_.out_edges(n2).begin();
      auto edges_n2_end  = graph_.out_edges(n2).end();
      auto edges_base_iter = graph_.out_edges(base_).begin();
      auto edges_base_end = graph_.out_edges(base_).end();
      while (edges_n2_iter != edges_n2_end && edges_base_iter != edges_base_end) {
        auto edge_n2_dst = graph_.getEdgeDst(*edges_n2_iter);
        auto edge_base_dst = graph_.getEdgeDst(*edges_base_iter);
        if (edge_n2_dst == edge_base_dst) {
          intersection_size++;
          edges_n2_iter++;
          edges_base_iter++;
        } else if (edge_n2_dst > edge_base_dst) {
          edges_base_iter++;
        } else if (edge_n2_dst < edge_base_dst) {
          edges_n2_iter++;
        }
      }
      return intersection_size;
    }
  };

  template <typename IntersectAlgorithm>
  void JaccardImpl(Graph& graph, uint64_t node_num)
  {
    auto it = graph.begin();
    std::advance(it, node_num);
    GNode base = *it;

    uint32_t base_size = graph.getDegree(base);

    IntersectAlgorithm intersect_with_base{graph, base};

    // Compute the similarity for each node
    galois::do_all(galois::iterate(graph), [&](const GNode& n2) {
      uint64_t n2_size = graph.getDegree(n2);
      // Count the number of neighbors of n2 and the number that are shared
      // with base
      uint64_t intersection_size = intersect_with_base(n2);
      // Compute the similarity
      uint64_t union_size = base_size + n2_size - intersection_size;
      double similarity =
          union_size > 0 ? (double)intersection_size / union_size : 1;
      // Store the similarity back into the graph.
      graph.getData(n2) = similarity;
    });

  }

  void run_jaccard_all_nodes(const std::string& fn, pa c, const std::string& msg)
  {
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_edge_list(fn, num_nodes, num_edges);
    Graph graph = Graph((uint32_t) num_nodes, num_edges, [ret](uint32_t n){return ret[n].size();},
        [ret](uint32_t n, uint64_t e) {return (uint32_t) ret[n][e];}, [](uint32_t n, uint64_t e){ return 0; });
    delete[] ret;
    run_test_and_benchmark<uint64_t>(msg, c, 8,
      [&graph]{return 0;},
      [&graph,this](uint64_t v)
      {
        for(uint64_t i = 0; i < graph.size(); i++) this->JaccardImpl<IntersectWithSortedEdgeList>(graph, i);
      },
      [&graph](uint64_t v) {},[](uint64_t v){});
  }

  template<typename T>
  void run_jaccard_all_nodes_inc(uint64_t num_nodes, uint64_t num_edges, const std::vector<std::pair<uint64_t,uint64_t>>& ret, pa c, const std::string& msg, T add)
  {
    auto g = [num_nodes]() {
      return new Graph(num_nodes, 0, [](uint64_t n) {return 0;},
          [](uint64_t n, uint64_t e) {return 0;}, [](uint64_t n, uint64_t e){ return 0;});
    };

    auto f = [ret,g,num_edges,msg,this,c,add](uint8_t i)
    {
      run_test_and_benchmark<Graph*>(msg + " Edit " + std::to_string(i), c, 8,
        [g,add,num_edges,i,ret]{ Graph* graph = g();
          add(graph, 0, num_edges/8 * (i -1), num_edges/8, ret);
          return graph;
        },
        [ret,num_edges,i,add](Graph* graph){
          add(graph, num_edges/8 *(i -1), num_edges/8, num_edges/8, ret);
        }, [](Graph* graph){}, [](Graph* graph){ delete graph; });

      Graph* graph = g();
      add(graph, 0, num_edges/8 * i, num_edges/8, ret);

      run_test_and_benchmark<uint64_t>(msg + " Algo " + std::to_string(i), c, 8,
        []{ return 0; },
        [num_edges,this,graph](uint64_t v){
            for(uint64_t i = 0; i < graph->size(); i++)
              this->JaccardImpl<IntersectWithSortedEdgeList>(*graph, i);
        }, [](uint64_t v){}, [](uint64_t v){});

      delete graph;
    };
    for(int i = 1; i <= 8; i ++)
    {
      f(i);
    }
  }

};

template<typename Graph>
void regen_graph(Graph* graph, uint64_t lower, uint64_t upper, uint64_t batch_size, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
{
  auto edge_list = new std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>[graph->size()];
  for(uint64_t i = 0; i < upper && i < edges.size(); i++)
  {
    edge_list[edges[i].first].push(edges[i].second);
  }
  Graph* graph2 = new Graph(graph->size(), upper, [edge_list](uint64_t n){return edge_list[n].size();},
      [edge_list](uint64_t n, uint64_t e) {auto elem = edge_list[n].top(); edge_list[n].pop(); return elem;}, [](uint32_t n, uint64_t e){ return 0; });
  swap(*graph, *graph2);
  delete graph2;
  delete[] edge_list;
}

/*
TEST_CASE( "Running Galois Jaccard", "[jaccard]" )
{
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  using LS_CSR_LOCK_OUT = galois::graphs::LS_LC_CSR_64_Graph<unsigned, void>::with_no_lockable<true>::type;
  using LC_CSR_64_GRAPH = galois::graphs::LC_CSR_64_Graph<unsigned, void>::with_no_lockable<true>::type;
  using LC_CSR_32_GRAPH = galois::graphs::LC_CSR_Graph<unsigned, void>::with_no_lockable<true>::type;
  using MORPH_GRAPH     = galois::graphs::MorphGraph<unsigned, void, true>::with_no_lockable<true>::type;
  Jaccard_Algo<LS_CSR_LOCK_OUT> lclo;
  Jaccard_Algo<LC_CSR_64_GRAPH> lc6g;
  Jaccard_Algo<LC_CSR_32_GRAPH> lc3g;
  Jaccard_Algo<MORPH_GRAPH>     morg;

  SECTION( "All Nodes Citeseer Galois" )
  {
    lclo.run_jaccard_all_nodes("../graphs/citeseer.el", c, "Citeseer LS_CSR_LOCK_OUT");
    lc6g.run_jaccard_all_nodes("../graphs/citeseer.el", c, "Citeseer LC_CSR_64_GRAPH");
    lc3g.run_jaccard_all_nodes("../graphs/citeseer.el", c, "Citeseer LC_CSR_32_GRAPH");
    morg.run_jaccard_all_nodes("../graphs/citeseer.el", c, "Citeseer MORPH_GRAPH");
  }

  SECTION( "All Nodes Cora Galois" )
  {
    lclo.run_jaccard_all_nodes("../graphs/cora.el", c, "Cora LS_CSR_LOCK_OUT");
    lc6g.run_jaccard_all_nodes("../graphs/cora.el", c, "Cora LC_CSR_64_GRAPH");
    lc3g.run_jaccard_all_nodes("../graphs/cora.el", c, "Cora LC_CSR_32_GRAPH");
    morg.run_jaccard_all_nodes("../graphs/cora.el", c, "Cora MORPH_GRAPH");
  }

  SECTION( "All Nodes Flickr Galois" )
  {
    lclo.run_jaccard_all_nodes("../graphs/flickr.el", c, "Flickr LS_CSR_LOCK_OUT");
    lc6g.run_jaccard_all_nodes("../graphs/flickr.el", c, "Flickr LC_CSR_64_GRAPH");
    lc3g.run_jaccard_all_nodes("../graphs/flickr.el", c, "Flickr LC_CSR_32_GRAPH");
    morg.run_jaccard_all_nodes("../graphs/flickr.el", c, "Flickr MORPH_GRAPH");
  }

  SECTION( "All Nodes Yelp Galois" )
  {
    lclo.run_jaccard_all_nodes("../graphs/yelp.el", c, "Yelp LS_CSR_LOCK_OUT");
    lc6g.run_jaccard_all_nodes("../graphs/yelp.el", c, "Yelp LC_CSR_64_GRAPH");
    lc3g.run_jaccard_all_nodes("../graphs/yelp.el", c, "Yelp LC_CSR_32_GRAPH");
    morg.run_jaccard_all_nodes("../graphs/yelp.el", c, "Yelp MORPH_GRAPH");
  }
}
*/

TEST_CASE( "Galois Edits Jaccard", "[jaccard]")
{
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  using LS_CSR_LOCK_OUT = galois::graphs::LS_LC_CSR_64_Graph<unsigned, void>::with_no_lockable<true>::type;
  using LC_CSR_64_GRAPH = galois::graphs::LC_CSR_64_Graph<unsigned, void>::with_no_lockable<true>::type;
  using LC_CSR_32_GRAPH = galois::graphs::LC_CSR_Graph<unsigned, void>::with_no_lockable<true>::type;
  using MORPH_GRAPH     = galois::graphs::MorphGraph<unsigned, void, true>::with_no_lockable<true>::type;
  Jaccard_Algo<LS_CSR_LOCK_OUT> lclo;
  Jaccard_Algo<LC_CSR_64_GRAPH> lc6g;
  Jaccard_Algo<LC_CSR_32_GRAPH> lc3g;
  Jaccard_Algo<MORPH_GRAPH>     morg;

  auto morg_add = [](MORPH_GRAPH* graph, uint64_t lower, uint64_t upper, uint64_t batch_size, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
  {
    for(uint64_t j = lower; j < upper && j < edges.size(); j++)
      graph->addEdge(edges[j].first, edges[j].second);
    graph->sortAllEdgesByDst();
  };
  auto lc6g_add = regen_graph<LC_CSR_64_GRAPH>;
  auto lc3g_add = regen_graph<LC_CSR_32_GRAPH>;
  auto lclo_add = [](LS_CSR_LOCK_OUT* graph, uint64_t lower, uint64_t upper, uint64_t batch_size, const std::vector<std::pair<uint64_t, uint64_t>>& edges)
  {
    for(uint64_t start = lower; start < upper; start += batch_size)
    {
      std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>* edge_list
        = new std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>[graph->size()];
      for(uint64_t i = start; i < start + batch_size && i < edges.size(); i++)
      {
        edge_list[edges[i].first].push(edges[i].second);
      }
      graph->addEdges(edge_list);
      delete[] edge_list;

    }
  };

  SECTION( "All Nodes Citeseer Galois" )
  {
    std::string bench = "Citeseer";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_rand_vec_edge("../graphs/citeseer.el", num_nodes, num_edges);


    lclo.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " MORPH_GRAPH", morg_add);
  }

  SECTION( "All Nodes Cora Galois" )
  {
    std::string bench = "Cora";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_rand_vec_edge("../graphs/cora.el", num_nodes, num_edges);

    lclo.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " MORPH_GRAPH", morg_add);
  }

  SECTION( "All Nodes Flickr Galois" )
  {
    std::string bench = "Flickr";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_rand_vec_edge("../graphs/flickr.el", num_nodes, num_edges);

    lclo.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " MORPH_GRAPH", morg_add);
  }

  SECTION( "All Nodes Yelp Galois" )
  {
    std::string bench = "Yelp";
    uint64_t num_nodes;
    uint64_t num_edges;
    auto ret = el_file_to_rand_vec_edge("../graphs/yelp.el", num_nodes, num_edges);

    lclo.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_LOCK_OUT", lclo_add);
    lc6g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_64_GRAPH", lc6g_add);
    lc3g.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " LC_CSR_32_GRAPH", lc3g_add);
    morg.run_jaccard_all_nodes_inc(num_nodes, num_edges, ret, c, bench + " MORPH_GRAPH", morg_add);
  }

}
