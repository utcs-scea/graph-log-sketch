#include "jaccard.hpp"
#include "llvm/Support/CommandLine.h"

namespace cll = llvm::cl;

static const char* name = "Edit Scalability";

static const char* desc =
  "Creates graphs from files in order to see how long it takes.";

static const char* url = "edit_scalability";

static const cll::opt<std::string> inputFile(
  cll::Positional, cll::desc("<input file>"), cll::Required);

static const cll::opt<std::string> statsFile(cll::Positional, cll::desc("<stat file>"), cll::Required);

static const cll::opt<std::uint64_t> numGThreads(cll::Positional, cll::desc("<num threads>"), cll::Required);

enum GraphType : uint8_t {LS_CSR,LS_CPR,CSR_64,CSR_32,MOR_GR};
cll::opt<GraphType> gtype(
    "g",
    cll::desc("Choose LS_CSR, LS_CPR, CSR_64, CSR_32, or MOR_GR as the target (default value LS_CSR)"),
    cll::values(clEnumVal(LS_CSR, "LS_CSR"),clEnumVal(LS_CPR, "LS_CPR"),clEnumVal(CSR_64, "CSR_64"),clEnumVal(CSR_32, "CSR_32"),clEnumVal(MOR_GR, "MOR_GR")),
    cll::init(LS_CSR));

static const cll::opt<uint64_t> rseed_vec(
    "seed",
    cll::desc("Choose a random seed for the random vector"),
    cll::init(rseed));

using LS_CSR_LOCK_OUT = galois::graphs::LS_LC_CSR_64_Graph<void, void>::with_no_lockable<true>::type;
using LC_CSR_64_GRAPH = galois::graphs::LC_CSR_64_Graph<void, void>::with_no_lockable<true>::type;
using LC_CSR_32_GRAPH = galois::graphs::LC_CSR_Graph<void, void>::with_no_lockable<true>::type;
using MORPH_GRAPH     = galois::graphs::MorphGraph<void, void, true>::with_no_lockable<true>::type;

auto lclo_evo = add_edges_per_node<LS_CSR_LOCK_OUT>;
auto lcpo_evo = add_edges_group_insert_sort<LS_CSR_LOCK_OUT>;
auto lc6g_evo = regen_graph_sorted<LC_CSR_64_GRAPH>;
auto lc3g_evo = regen_graph_sorted<LC_CSR_32_GRAPH>;
auto morg_evo = add_edges_per_edge<false,MORPH_GRAPH>;

int main(int argc, char** argv)
{
  cll::ParseCommandLineOptions(argc, argv);
  std::string statsfn = statsFile;
  galois::SharedMemSys G;
  galois::setActiveThreads(numGThreads);
  pa c = create_counters();
  std::chrono::high_resolution_clock::time_point t0;
  std::chrono::high_resolution_clock::time_point t1;

  uint64_t num_nodes;
  uint64_t num_edges;

  t0 = std::chrono::high_resolution_clock::now();
  auto edge_list = el_file_to_rand_vec_edge(inputFile, num_nodes, num_edges);
  t1 = std::chrono::high_resolution_clock::now();
  auto diff = std::chrono::duration<uint64_t, std::nano>(t1-t0).count();
  std::cout << "File Read Time (ns):\t" << diff << std::endl;

  t0 = std::chrono::high_resolution_clock::now();
  switch(gtype)
  {
      case LS_CPR:
        run_p_evolve<LS_CSR_LOCK_OUT>(statsfn, "LS_CPR " + std::to_string(numGThreads), c, 8, num_nodes, num_edges, edge_list,
          lcpo_evo);
      case LS_CSR:
        run_p_evolve<LS_CSR_LOCK_OUT>(statsfn, "LS_CSR " + std::to_string(numGThreads), c, 8, num_nodes, num_edges, edge_list,
          lclo_evo);
        break;
      case CSR_64:
        run_p_evolve<LC_CSR_64_GRAPH>(statsfn, "CSR_64 " + std::to_string(numGThreads), c, 8, num_nodes, num_edges, edge_list,
          lc6g_evo);
        break;
      case CSR_32:
        run_p_evolve<LC_CSR_32_GRAPH>(statsfn, "CSR_32 " + std::to_string(numGThreads), c, 8, num_nodes, num_edges, edge_list,
          lc3g_evo);
        break;
      case MOR_GR:
        run_p_evolve<MORPH_GRAPH>(statsfn, "MOR_GR " + std::to_string(numGThreads), c, 8, num_nodes, num_edges, edge_list,
          morg_evo);
        break;
  }
  t1 = std::chrono::high_resolution_clock::now();
  diff = std::chrono::duration<uint64_t, std::nano>(t1-t0).count();
  std::cout << "Approx Runtime (ns):\t" << diff << std::endl;

}
