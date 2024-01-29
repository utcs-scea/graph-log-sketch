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
    upper_triangle((double*) mmap(NULL, sizeof(double) * n  * (n - 1) / 2, PROT_READ | PROT_WRITE,
          MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0))
    { assert(upper_triangle != MAP_FAILED);}

  ~JaccardRet() {munmap(upper_triangle, sizeof(double) * num_nodes  * (num_nodes - 1) / 2);}

  void reset() {std::fill(upper_triangle, upper_triangle + num_nodes*(num_nodes -1)/2, 0);}

  double get_val_unsafe(uint64_t n1, uint64_t n2)
  {
    return  (n1 > n2) ? upper_triangle[flatten_index(n2) + (n1 - n2)]:
            (n2 > n1) ? upper_triangle[flatten_index(n1) + (n2 - n1)]:
            1;
  }

  void insert(uint64_t n1, uint64_t n2, double val)
  {
    double* location =
            (n1 > n2) ? &upper_triangle[flatten_index(n2) + (n1 - n2)]:
            (n2 > n1) ? &upper_triangle[flatten_index(n1) + (n2 - n1)]:
            nullptr;

    assert(location != nullptr);

    *location = val;
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

  void insert(uint64_t node0, uint64_t node1, double val) {blah = val;}

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
    uint64_t count = 1;
    for(; it != graph.end(); it++, count++)
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
      ret.insert(node_num, node_num + count, similarity);
    }
  }

  template <typename IntersectAlgorithm, typename JRet>
  void JaccardImplSinglePair(Graph& graph, uint64_t node0, uint64_t node1, JRet& ret)
  {
    uint64_t sz = graph.size();

    auto it = graph.begin();
    std::advance(it, node0);
    GNode base0 = *it;

    uint64_t base_size = graph.getDegree(base0);

    IntersectAlgorithm intersect_with_base{graph, base0};

    it = graph.begin();
    std::advance(it, node1);
    const GNode& n1 = *it;
    uint64_t n1_size = graph.getDegree(n1);
    // Count the number of neighbors of n2 and the number that are shared
    // with base
    uint64_t intersection_size = intersect_with_base(n1);
    // Compute the similarity
    uint64_t union_size = base_size + n1_size - intersection_size;
    double similarity =
      union_size > 0 ? (double)intersection_size / union_size : 1;
    // Store the similarity back into the graph.
    ret.insert(node0, node1, similarity);
  }

  template<typename IntersectAlgorithm, typename JRet>
  void JaccardImplPair(Graph& graph, size_t node_samp, JRet& ret, std::vector<uint64_t>& samps)
  {
    auto it = graph.begin();
    uint64_t prev = samps[node_samp];
    node_samp++;
    std::advance(it, prev);
    GNode base = *it;

    uint64_t base_size = graph.getDegree(base);

    IntersectAlgorithm intersect_with_base{graph, base};

    for(; node_samp < samps.size(); node_samp++)
    {
      const uint64_t nprev = samps[node_samp];
      std::advance(it, nprev - prev);
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
      ret.add_next_unsafe(node_samp, similarity);
      prev = nprev;
    }
  }

  template <typename IntersectAlgorithm, typename JRet>
  void JaccardImpl(Graph& graph, JRet& ret)
  {
    using idx_it = boost::counting_iterator<uint64_t>;

    const uint64_t size = graph.size();
    galois::do_all(galois::iterate(idx_it(0), idx_it(size * (size - 1) / 2)),
      [&](const uint64_t linear_index)
      {
        // https://stackoverflow.com/a/27088560
        // convert linear index to (row, col) index:
        const double k = (double)linear_index;
        const double n = (double)size;
        const uint64_t i = size - 2 - uint64_t(floor(sqrt(-8.0L * k + 4.0L * n * (n - 1.0L) - 7.0L) / 2.0L - 0.5L));
        const uint64_t j = linear_index + i + 1 - size * (size - 1) / 2 + (size - i) * ((size - i) - 1) / 2;
        this->JaccardImplSinglePair<IntersectAlgorithm>(graph, i, j, ret);
      }, galois::steal());
  }

  template <typename IntersectAlgorithm, typename JRet>
  void JaccardImplRand(Graph& graph, JRet& ret, std::vector<uint64_t>& samps)
  {
    galois::do_all(galois::iterate((size_t)0, samps.size()),
      [&](size_t start_node)
      {
        this->JaccardImplPair<IntersectAlgorithm>(graph, start_node, ret, samps);
      }
    );

  }

};
