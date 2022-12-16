#include "jaccard.hpp"
#include <counters.hpp>

#include "llvm/Support/CommandLine.h"

namespace cll = llvm::cl;

static const char* name = "Jaccard"

static const char* desc =
  "Computes the Jaccard coefficients of the out neighborhood of all nodes in a directed graph.";

static const char* url = "jaccard_coefficients";

static cll::opt<std::string> inputFile(
  cll::Positional, cll::desc("<input file>"), cll::Required);

static cll::opt<std::string> statFile("c", cl::desc("<stat file>"));

enum BenchStyle : uint8_t {STATIC, EVOLVE};
static cll::opt<BenchStyle> style(
    "style",
    cll::desc("Choose STATIC or EVOLVING execution (default value STATIC)"),
    cll::values(clEnumVal(STATIC, "STATIC"), clEnumVal(EVOLVE, "EVOLVE")),
    cll::init(STATIC));

enum GraphType : uint8_t {LS_CSR,CSR_64,CSR_32,MOR_GR}
static cll::opt<GraphType> gtype(
    "g",
    cll:desc("Choose LS_CSR, CSR_64, CSR_32, or MOR_GR as the target (default value LS_CSR)"),
    cll::values(clEnumVal(LS_CSR, "LS_CSR"),clEnumVal(CSR_64, "CSR_64"),clEnumVal(CSR_32, "CSR_32"),clEnumVal(MOR_GR, "MOR_GR")),
    cll::init(LS_CSR));

using LS_CSR_LOCK_OUT = galois::graphs::LS_LC_CSR_64_Graph<double*, void>::with_no_lockable<true>::type;
using LC_CSR_64_GRAPH = galois::graphs::LC_CSR_64_Graph<double*, void>::with_no_lockable<true>::type;
using LC_CSR_32_GRAPH = galois::graphs::LC_CSR_Graph<double*, void>::with_no_lockable<true>::type;
using MORPH_GRAPH     = galois::graphs::MorphGraph<double*, void, true>::with_no_lockable<true>::type;
Jaccard_Algo<LS_CSR_LOCK_OUT> lclo;
Jaccard_Algo<LC_CSR_64_GRAPH> lc6g;
Jaccard_Algo<LC_CSR_32_GRAPH> lc3g;
Jaccard_Algo<MORPH_GRAPH>     morg;

auto lclo_evo = add_edges_per_node<LC_CSR_LOCK_OUT>;
auto lc6g_evo = regen_graph_sorted<LC_CSR_64_GRAPH>;
auto lc3g_evo = regen_graph_sorted<LC_CSR_32_GRAPH>;
auto morg_evo = add_edges_per_edge<true,MORPH_GRAPH>;

int main(int argc, char** argv)
{
  cll::ParseCommandLineOptions(argc, argv);
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  uint64_t num_nodes;
  uint64_t num_edges;

  double** ret = (double**) malloc(sizeof(double*) * (num_nodes - 1));
  ret[0] = nullptr;
  for(uint64_t i = 1; i < num_nodes; i++) ret[i] = (double*) malloc(sizeof(double) * i);

  auto edge_list = el_file_to_rand_vec_edge(inputFile, num_nodes, num_edges);

  if(style == EVOLVE)
  {
    switch(gtype)
    {
      case LS_CSR:
        run_algo_evolve("LS_CSR EVOLVE", c, 2, 8, num_nodes, num_edges, edge_list,
          lclo_evo,
          [ret](LS_CSR_LOCK_OUT* graph){lclo.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;
      case CSR_64:
        run_algo_evolve("CSR_64 EVOLVE", c, 3, 8, num_nodes, num_edges, edge_list,
          lc6g_evo,
          [ret](LC_CSR_64_Graph* graph){lc6g.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;
      case CSR_32:
        run_algo_evolve("CSR_32 EVOLVE", c, 4, 8, num_nodes, num_edges, edge_list,
          lc6g_evo,
          [ret](LC_CSR_32_GRAPH* graph){lc3g.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;
      case MOR_GR:
        run_algo_evolve("MOR_GR EVOLVE", c, 5, 8, num_nodes, num_edges, edge_list,
          morg_evo,
          [ret](MORPH_GRAPH* graph){morg.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;

    }
  }
  else if(style == STATIC)
  {
    switch(gtype)
    {
      case LS_CSR:
        run_algo_static("LS_CSR STATIC", c, 3, 8, num_nodes, num_edges, edge_list,
          lclo_evo,
          [ret](LS_CSR_LOCK_OUT* graph){lclo.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;
      case CSR_64:
        run_algo_static("CSR_64 STATIC", c, 4, 8, num_nodes, num_edges, edge_list,
          lc6g_evo,
          [ret](LC_CSR_64_Graph* graph){lc6g.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;
      case CSR_32:
        run_algo_static("CSR_32 STATIC", c, 5, 8, num_nodes, num_edges, edge_list,
          lc6g_evo,
          [ret](LC_CSR_32_GRAPH* graph){lc3g.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;
      case MOR_GR:
        run_algo_static("MOR_GR STATIC", c, 6, 8, num_nodes, num_edges, edge_list,
          [ret](MORPH_GRAPH* graph){morg.JaccardImpl<IntersectWithSortedEdgeList>(graph, ret);});
        break;
    }

  } else {
    std::cerr << "You need to pick either STATIC or EVOLVE";
    exit(-1);
  }

  delete[] edge_list;

  for(uint64_t i = 0; i < num_nodes - 1; i++)
    for(uint64_t j = 1; j < num_nodes; j++)
      std::cout << ret[i][j] << std::endl;

  for(uint64_t i = 1; i < num_nodes; i++) free(ret[i]);
  free(ret);
}
