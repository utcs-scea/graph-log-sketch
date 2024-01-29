#include "jaccard.hpp"
#include "llvm/Support/CommandLine.h"

namespace cll = llvm::cl;

static const char* name = "Jaccard";

static const char* desc = "Computes the Jaccard coefficients of the out "
                          "neighborhood of all nodes in a directed graph.";

static const char* url = "jaccard_coefficients";

static const cll::opt<std::string>
    inputFile("inpfile", cll::desc("<input file>"), cll::init(""));

static const cll::opt<std::uint64_t>
    numGThreads("t", cll::desc("<num threads>"), cll::init(1));

static const cll::opt<std::string>
    statsFile(cll::Positional, cll::desc("<stat file>"), cll::Required);

enum OutType : uint8_t { OUT, NON };
cll::opt<OutType> out("out", cll::desc("Choose OUT or NON (default value OUT)"),
                      cll::values(clEnumVal(OUT, "OUT"), clEnumVal(NON, "NON")),
                      cll::init(OUT));

enum BenchStyle : uint8_t { STATIC, EVOLVE };
cll::opt<BenchStyle> style(
    "style",
    cll::desc("Choose STATIC or EVOLVING execution (default value STATIC)"),
    cll::values(clEnumVal(STATIC, "STATIC"), clEnumVal(EVOLVE, "EVOLVE")),
    cll::init(STATIC));

enum GraphType : uint8_t { LS_CSR, CSR_64, MOR_GR };
cll::opt<GraphType> gtype("g",
                          cll::desc("Choose LS_CSR, CSR_64, or MOR_GR as the "
                                    "target (default value LS_CSR)"),
                          cll::values(clEnumVal(LS_CSR, "LS_CSR"),
                                      clEnumVal(CSR_64, "CSR_64"),
                                      clEnumVal(MOR_GR, "MOR_GR")),
                          cll::init(LS_CSR));

static const cll::opt<uint64_t>
    num_n("n", cll::desc("Choose a number of nodes to generate"),
          cll::init(1 << 16));

static const cll::opt<uint64_t>
    num_props("m",
              cll::desc("Choose a number of properties for each node to have"),
              cll::init(1 << 16));

static const cll::opt<double>
    prob_prop("p", cll::desc("Choose a probability that a node has a property"),
              cll::init(0.5));

static const cll::opt<uint64_t>
    rseed_vec("seed", cll::desc("Choose a random seed for the random vector"),
              cll::init(rseed));

using LS_CSR_LOCK_OUT =
    galois::graphs::LS_LC_CSR_64_Graph<void,
                                       void>::with_no_lockable<true>::type;
using LC_CSR_64_GRAPH =
    galois::graphs::LC_CSR_64_Graph<void, void>::with_no_lockable<true>::type;
using LC_CSR_32_GRAPH =
    galois::graphs::LC_CSR_Graph<void, void>::with_no_lockable<true>::type;
using MORPH_GRAPH =
    galois::graphs::MorphGraph<void, void, true>::with_no_lockable<true>::type;

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
auto morg_evo = add_edges_per_edge<true, MORPH_GRAPH>;

int main(int argc, char** argv) {
  cll::ParseCommandLineOptions(argc, argv);
  std::string statsfn = statsFile;
  galois::SharedMemSys G;
  galois::setActiveThreads(numGThreads);
  pa c = create_counters();
  std::chrono::high_resolution_clock::time_point t0;
  std::chrono::high_resolution_clock::time_point t1;

  uint64_t num_nodes = num_n;
  uint64_t num_edges;

  if (style == EVOLVE) {
    std::vector<std::pair<uint64_t, uint64_t>> edge_list;
    t0 = std::chrono::high_resolution_clock::now();

    if (inputFile.empty())
      edge_list =
          gen_random_graph_edges(num_nodes, num_props, prob_prop, num_edges);
    else
      edge_list = el_file_to_rand_vec_edge(inputFile, num_nodes, num_edges);

    t1        = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration<uint64_t, std::nano>(t1 - t0).count();
    std::cout << "File Read Time (ns):\t" << diff << std::endl;

    if (out == OUT) {
      JaccardRet jutr = JaccardRet(num_nodes);
      switch (gtype) {
      case LS_CSR:
        run_algo_p_evolve<LS_CSR_LOCK_OUT>(
            statsfn, "LS_CSR EVOLVE", c, 8, num_nodes, num_edges, edge_list,
            lclo_evo, [&jutr](LS_CSR_LOCK_OUT& graph) { jutr.reset(); },
            [&jutr](LS_CSR_LOCK_OUT& graph) {
              lclo.JaccardImpl<LSJA::IntersectWithSortedEdgeList, JaccardRet>(
                  graph, jutr);
            });
        break;
      case CSR_64:
        run_algo_p_evolve<LC_CSR_64_GRAPH>(
            statsfn, "CSR_64 EVOLVE", c, 8, num_nodes, num_edges, edge_list,
            lc6g_evo, [&jutr](LC_CSR_64_GRAPH& graph) { jutr.reset(); },
            [&jutr](LC_CSR_64_GRAPH& graph) {
              lc6g.JaccardImpl<LC6A::IntersectWithSortedEdgeList, JaccardRet>(
                  graph, jutr);
            });
        break;
      case MOR_GR:
        run_algo_p_evolve<MORPH_GRAPH>(
            statsfn, "MOR_GR EVOLVE", c, 8, num_nodes, num_edges, edge_list,
            morg_evo, [&jutr](MORPH_GRAPH& graph) { jutr.reset(); },
            [&jutr](MORPH_GRAPH& graph) {
              morg.JaccardImpl<MORG::IntersectWithSortedEdgeList, JaccardRet>(
                  graph, jutr);
            });
        break;
      }
      jutr.print(std::cout);
    } else {
      JaccardNoRet jutr = JaccardNoRet(num_nodes);
      switch (gtype) {
      case LS_CSR:
        run_algo_p_evolve<LS_CSR_LOCK_OUT>(
            statsfn, "LS_CSR EVOLVE", c, 8, num_nodes, num_edges, edge_list,
            lclo_evo, [&jutr](LS_CSR_LOCK_OUT& graph) { jutr.reset(); },
            [&jutr](LS_CSR_LOCK_OUT& graph) {
              lclo.JaccardImpl<LSJA::IntersectWithSortedEdgeList, JaccardNoRet>(
                  graph, jutr);
            });
        break;
      case CSR_64:
        run_algo_p_evolve<LC_CSR_64_GRAPH>(
            statsfn, "CSR_64 EVOLVE", c, 8, num_nodes, num_edges, edge_list,
            lc6g_evo, [&jutr](LC_CSR_64_GRAPH& graph) { jutr.reset(); },
            [&jutr](LC_CSR_64_GRAPH& graph) {
              lc6g.JaccardImpl<LC6A::IntersectWithSortedEdgeList, JaccardNoRet>(
                  graph, jutr);
            });
        break;
      case MOR_GR:
        run_algo_p_evolve<MORPH_GRAPH>(
            statsfn, "MOR_GR EVOLVE", c, 8, num_nodes, num_edges, edge_list,
            morg_evo, [&jutr](MORPH_GRAPH& graph) { jutr.reset(); },
            [&jutr](MORPH_GRAPH& graph) {
              morg.JaccardImpl<MORG::IntersectWithSortedEdgeList, JaccardNoRet>(
                  graph, jutr);
            });
        break;
      }
      jutr.print(std::cout);
    }
  }

  else if (style == STATIC) {
    std::vector<uint64_t>* edge_list;
    t0 = std::chrono::high_resolution_clock::now();

    if (inputFile.empty())
      edge_list =
          gen_random_to_edge_list(num_nodes, num_props, prob_prop, num_edges);
    else
      edge_list = el_file_to_edge_list(inputFile, num_nodes, num_edges);

    t1        = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration<uint64_t, std::nano>(t1 - t0).count();
    std::cout << "File Read Time (ns):\t" << diff << std::endl;

    if (out == OUT) {
      JaccardRet jutr = JaccardRet(num_nodes);
      switch (gtype) {
      case LS_CSR:
        run_algo_p_static<LS_CSR_LOCK_OUT>(
            statsfn, "LS_CSR STATIC", c, num_nodes, num_edges, edge_list,
            [&jutr](LS_CSR_LOCK_OUT& graph) { jutr.reset(); },
            [&jutr](LS_CSR_LOCK_OUT& graph) {
              lclo.JaccardImpl<LSJA::IntersectWithSortedEdgeList, JaccardRet>(
                  graph, jutr);
            });
        break;
      case CSR_64:
        run_algo_p_static<LC_CSR_64_GRAPH>(
            statsfn, "CSR_64 STATIC", c, num_nodes, num_edges, edge_list,
            [&jutr](LC_CSR_64_GRAPH& graph) { jutr.reset(); },
            [&jutr](LC_CSR_64_GRAPH& graph) {
              lc6g.JaccardImpl<LC6A::IntersectWithSortedEdgeList, JaccardRet>(
                  graph, jutr);
            });
        break;
      case MOR_GR:
        run_algo_p_static<MORPH_GRAPH>(
            statsfn, "MOR_GR STATIC", c, num_nodes, num_edges, edge_list,
            [&jutr](MORPH_GRAPH& graph) { jutr.reset(); },
            [&jutr](MORPH_GRAPH& graph) {
              morg.JaccardImpl<MORG::IntersectWithSortedEdgeList, JaccardRet>(
                  graph, jutr);
            });
        break;
      }
      jutr.print(std::cout);
    } else {
      JaccardNoRet jutr = JaccardNoRet(num_nodes);
      switch (gtype) {
      case LS_CSR:
        run_algo_p_static<LS_CSR_LOCK_OUT>(
            statsfn, "LS_CSR STATIC", c, num_nodes, num_edges, edge_list,
            [&jutr](LS_CSR_LOCK_OUT& graph) { jutr.reset(); },
            [&jutr](LS_CSR_LOCK_OUT& graph) {
              lclo.JaccardImpl<LSJA::IntersectWithSortedEdgeList, JaccardNoRet>(
                  graph, jutr);
            });
        break;
      case CSR_64:
        run_algo_p_static<LC_CSR_64_GRAPH>(
            statsfn, "CSR_64 STATIC", c, num_nodes, num_edges, edge_list,
            [&jutr](LC_CSR_64_GRAPH& graph) { jutr.reset(); },
            [&jutr](LC_CSR_64_GRAPH& graph) {
              lc6g.JaccardImpl<LC6A::IntersectWithSortedEdgeList, JaccardNoRet>(
                  graph, jutr);
            });
        break;
      case MOR_GR:
        run_algo_p_static<MORPH_GRAPH>(
            statsfn, "MOR_GR STATIC", c, num_nodes, num_edges, edge_list,
            [&jutr](MORPH_GRAPH& graph) { jutr.reset(); },
            [&jutr](MORPH_GRAPH& graph) {
              morg.JaccardImpl<MORG::IntersectWithSortedEdgeList, JaccardNoRet>(
                  graph, jutr);
            });
        break;
      }
      jutr.print(std::cout);
    }
    delete[] edge_list;

  } else {
    std::cerr << "You need to pick either STATIC or EVOLVE";
    exit(-1);
  }
}
