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
#include <graph_benchmark_style.hpp>
//#include "Lonestar/BFS_SSSP.h"

#include <iostream>
#include <deque>
#include <type_traits>
#include <queue>

#include <benchmark.hpp>

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

//basically behaves like a flat symmetric matrix
class JaccardRet
{
  const uint64_t num_nodes;
  uint64_t* curr_sz;
  double* upper_triangle;

  inline uint64_t flatten_index(uint64_t node) const
  {
    return (node) * (num_nodes - 1 + num_nodes - node) / 2;
  }

public:

  JaccardRet(uint64_t n) :
    num_nodes(n),
    curr_sz((uint64_t*) calloc(sizeof(uint64_t*), n)),
    upper_triangle((double*) mmap(NULL, sizeof(double) * n  * (n - 1) / 2, PROT_READ | PROT_WRITE,
          MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0))
    { assert(upper_triangle != MAP_FAILED);}

  ~JaccardRet() {free(curr_sz); munmap(upper_triangle, sizeof(double) * num_nodes  * (num_nodes - 1) / 2);}

  void reset() {std::fill(curr_sz, curr_sz + num_nodes - 1, 0);}

  void add_next_unsafe(uint64_t node, double val)
  {
    uint64_t& size = curr_sz[node];
    uint64_t index = flatten_index(node);
    upper_triangle[index + size] = val;
    size++;
  }

  int add_next(uint64_t node, double val)
  {
    if(curr_sz[node] < num_nodes - node - 1)
    {
      add_next_unsafe(node, val);
      return 0;
    }
    return -1;
  }

  double get_val_unsafe(uint64_t n1, uint64_t n2)
  {
    return  (n1 > n2) ? upper_triangle[flatten_index(n2) + (n1 - n2)]:
            (n2 > n1) ? upper_triangle[flatten_index(n1) + (n2 - n1)]:
            1;
  }

  template<typename O>
  void print(O& stream)
  {
    for(uint64_t i = 0; i < num_nodes - 1; i++)
      for(uint64_t j = i; j < num_nodes; j++)
        stream << this->get_val_unsafe(i,j) << std::endl;
  }

};

class JaccardNoRet
{
  double blah;

public:

  JaccardNoRet(uint64_t n) : blah(0) {}

  ~JaccardNoRet() {}

  void reset() {blah = 0;}

  void add_next_unsafe(uint64_t node, double val) {blah = val;}

  int add_next(uint64_t node, double val)
  {
    add_next_unsafe(node, val);
    return -1;
  }

  double get_val_unsafe(uint64_t n1, uint64_t n2)
  {
    return blah;
  }

  template<typename O>
  void print(O& stream) {}

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
            min_similarity.update(similarity); total_similarity += similarity;
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

  template <typename IntersectAlgorithm, typename JRet>
  void JaccardImplSingle(Graph& graph, uint64_t node_num, JRet& ret)
  {
    uint64_t sz = graph.size();

    auto it = graph.begin();
    std::advance(it, node_num);
    GNode base = *it;

    uint64_t base_size = graph.getDegree(base);

    IntersectAlgorithm intersect_with_base{graph, base};
    it++;

    // Compute the similarity for each node
    for(; it != graph.end(); it++)
    {
      const GNode& n2 = *it;

      uint64_t n2_size = graph.getDegree(n2);
      // Count the number of neighbors of n2 and the number that are shared
      // with base
      uint64_t intersection_size = intersect_with_base(n2);
      // Compute the similarity
      uint64_t union_size = base_size + n2_size - intersection_size;
      double similarity =
          union_size > 0 ? (double)intersection_size / union_size : 1;
      // Store the similarity back into the graph.
      ret.add_next_unsafe(node_num ,similarity);
    }
  }

  template <typename IntersectAlgorithm, typename JRet>
  void JaccardImpl(Graph& graph, JRet& ret)
  {
    const uint64_t size = graph.size();
    galois::do_all(galois::iterate((uint64_t)0, size),
      [&](uint64_t start_node)
      {
        this->JaccardImplSingle<IntersectAlgorithm>(graph, start_node, ret);
      }
    );
  }

};
