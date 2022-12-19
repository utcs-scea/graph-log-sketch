#include "jaccard.hpp"
#include <counters.hpp>

#include "llvm/Support/CommandLine.h"

namespace cll = llvm::cl;

static const char* name = "Jaccard";

static const char* desc =
  "Computes the Jaccard coefficients of the out neighborhood of all nodes in a directed graph.";

static const char* url = "jaccard_coefficients";

static const cll::opt<std::string> inputFile(
  cll::Positional, cll::desc("<input file>"), cll::Required);

static const cll::opt<std::string> statsFile(cll::Positional, cll::desc("<stat file>"), cll::Required);

enum BenchStyle : uint8_t {STATIC, EVOLVE};
cll::opt<BenchStyle> style(
    "style",
    cll::desc("Choose STATIC or EVOLVING execution (default value STATIC)"),
    cll::values(clEnumVal(STATIC, "STATIC"), clEnumVal(EVOLVE, "EVOLVE")),
    cll::init(STATIC));

enum GraphType : uint8_t {LS_CSR,CSR_64,CSR_32,MOR_GR};
cll::opt<GraphType> gtype(
    "g",
    cll::desc("Choose LS_CSR, CSR_64, CSR_32, or MOR_GR as the target (default value LS_CSR)"),
    cll::values(clEnumVal(LS_CSR, "LS_CSR"),clEnumVal(CSR_64, "CSR_64"),clEnumVal(CSR_32, "CSR_32"),clEnumVal(MOR_GR, "MOR_GR")),
    cll::init(LS_CSR));

using LS_CSR_LOCK_OUT = galois::graphs::LS_LC_CSR_64_Graph<void, void>::with_no_lockable<true>::type;
using LC_CSR_64_GRAPH = galois::graphs::LC_CSR_64_Graph<void, void>::with_no_lockable<true>::type;
using LC_CSR_32_GRAPH = galois::graphs::LC_CSR_Graph<void, void>::with_no_lockable<true>::type;
using MORPH_GRAPH     = galois::graphs::MorphGraph<void, void, true>::with_no_lockable<true>::type;


using LSJA = Jaccard_Algo<LS_CSR_LOCK_OUT>;
using LC6A = Jaccard_Algo<LC_CSR_64_GRAPH>;
using LC3A = Jaccard_Algo<LC_CSR_32_GRAPH>;
using MORG = Jaccard_Algo<MORPH_GRAPH>;

LSJA lclo;
LC6A lc6g;
LC3A lc3g;
MORG morg;

auto lclo_evo = add_edges_per_node<LS_CSR_LOCK_OUT>;
auto lc6g_evo = regen_graph_sorted<LC_CSR_64_GRAPH>;
auto lc3g_evo = regen_graph_sorted<LC_CSR_32_GRAPH>;
auto morg_evo = add_edges_per_edge<true,MORPH_GRAPH>;

int main(int argc, char** argv)
{
  cll::ParseCommandLineOptions(argc, argv);
  std::string statsfn = statsFile;
  galois::SharedMemSys G;
  galois::setActiveThreads(1);
  pa c = create_counters();

  uint64_t num_nodes;
  uint64_t num_edges;

  if(style == EVOLVE)
  {
    auto edge_list = el_file_to_rand_vec_edge(inputFile, num_nodes, num_edges);
    JaccardRet jutr = JaccardRet(num_nodes);
    switch(gtype)
    {
      case LS_CSR:
        run_algo_evolve<LS_CSR_LOCK_OUT>(statsfn, "LS_CSR EVOLVE", c, 2, 8, num_nodes, num_edges, edge_list,
          lclo_evo, [&jutr](LS_CSR_LOCK_OUT& graph){jutr.reset();},
          [&jutr](LS_CSR_LOCK_OUT& graph){lclo.JaccardImpl<LSJA::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
      case CSR_64:
        run_algo_evolve<LC_CSR_64_GRAPH>(statsfn, "CSR_64 EVOLVE", c, 3, 8, num_nodes, num_edges, edge_list,
          lc6g_evo, [&jutr](LC_CSR_64_GRAPH& graph){jutr.reset();},
          [&jutr](LC_CSR_64_GRAPH& graph){lc6g.JaccardImpl<LC6A::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
      case CSR_32:
        run_algo_evolve<LC_CSR_32_GRAPH>(statsfn, "CSR_32 EVOLVE", c, 4, 8, num_nodes, num_edges, edge_list,
          lc3g_evo, [&jutr](LC_CSR_32_GRAPH& graph){jutr.reset();},
          [&jutr](LC_CSR_32_GRAPH& graph){lc3g.JaccardImpl<LC3A::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
      case MOR_GR:
        run_algo_evolve<MORPH_GRAPH>(statsfn, "MOR_GR EVOLVE", c, 5, 8, num_nodes, num_edges, edge_list,
          morg_evo, [&jutr](MORPH_GRAPH& graph){jutr.reset();},
          [&jutr](MORPH_GRAPH& graph){morg.JaccardImpl<MORG::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
    }
    jutr.print(std::cout);
  }
  else if(style == STATIC)
  {
    auto edge_list = el_file_to_edge_list(inputFile, num_nodes, num_edges);
    JaccardRet jutr = JaccardRet(num_nodes);
    switch(gtype)
    {
      case LS_CSR:
        run_algo_static<LS_CSR_LOCK_OUT>(statsfn, "LS_CSR STATIC", c, 3, num_nodes, num_edges, edge_list,
          [&jutr](LS_CSR_LOCK_OUT& graph){jutr.reset();},
          [&jutr](LS_CSR_LOCK_OUT& graph){lclo.JaccardImpl<LSJA::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
      case CSR_64:
        run_algo_static<LC_CSR_64_GRAPH>(statsfn, "CSR_64 STATIC", c, 4, num_nodes, num_edges, edge_list,
          [&jutr](LC_CSR_64_GRAPH& graph){jutr.reset();},
          [&jutr](LC_CSR_64_GRAPH& graph){lc6g.JaccardImpl<LC6A::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
      case CSR_32:
        run_algo_static<LC_CSR_32_GRAPH>(statsfn, "CSR_32 STATIC", c, 5, num_nodes, num_edges, edge_list,
          [&jutr](LC_CSR_32_GRAPH& graph){jutr.reset();},
          [&jutr](LC_CSR_32_GRAPH& graph){lc3g.JaccardImpl<LC3A::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
      case MOR_GR:
        run_algo_static<MORPH_GRAPH>(statsfn, "MOR_GR STATIC", c, 6, num_nodes, num_edges, edge_list,
          [&jutr](MORPH_GRAPH& graph){jutr.reset();},
          [&jutr](MORPH_GRAPH& graph){morg.JaccardImpl<MORG::IntersectWithSortedEdgeList>(graph, jutr);});
        break;
    }
    delete[] edge_list;

    jutr.print(std::cout);

  } else {
    std::cerr << "You need to pick either STATIC or EVOLVE";
    exit(-1);
  }


}
