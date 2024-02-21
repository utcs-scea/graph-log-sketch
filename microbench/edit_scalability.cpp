#include "llvm/Support/CommandLine.h"
namespace cl = llvm::cl;

#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <mutex>
#include <utility>
using namespace std;

#include <boost/algorithm/string/predicate.hpp>

#include "galois/Galois.h"
#include "graph.hpp"
#include "algo.hpp"

static const char* name = "Edit Scalability Benchmarking Suite";
static const char* desc = "Creates graphs from files in order to see how"
                          " edits impact algorithm performance.";

/*
 * Command-line args
 */

cl::opt<string> input_file_path(cl::Positional, cl::desc("[input file]"),
                                cl::init("-"));

cl::opt<size_t>
    ingest_threads("ingest_threads",
                   cl::desc("number of threads used for ingesting edges"),
                   cl::init(1));

cl::opt<size_t> algo_threads("algo_threads",
                             cl::desc("number of threads used for algorithm"),
                             cl::init(1));

cl::opt<uint64_t> num_vertices("num-vertices",
                               cl::desc("number of vertices in the graph"),
                               cl::Required);

cl::opt<string> graph_type("graph,g",
                           cl::desc("graph representation (lscsr, ...)"),
                           cl::init("lscsr"));

/*
 * Algo args
 */

cl::opt<string> algo_name("algo", cl::desc("algorithm to run (nop, sssp_bfs)"),
                          cl::init("nop"));

cl::OptionCategory sssp_bfs_category("SSSP_BFS Algorithm Options");

cl::opt<uint64_t> sssp_bfs_src("sssp-bfs-src", cl::desc("the source vertex"),
                               cl::cat(sssp_bfs_category));

int main(int argc, char const* argv[]) {
  // parse args
  cl::ParseCommandLineOptions(argc, argv);

  galois::SharedMemSys sys;

  // validate args
  if (ingest_threads == 0)
    throw runtime_error("ingest threads must be greater than zero");
  if (algo_threads == 0)
    throw runtime_error("algo threads must be greater than zero");

  unique_ptr<scea::Graph> graph;
  if (boost::iequals(graph_type, "lscsr"))
    graph = make_unique<scea::LS_CSR>(num_vertices);
  else
    throw runtime_error("unknown graph type: expected one of `lscsr`, ...");

  unique_ptr<scea::Algo> algo;
  if (boost::iequals(algo_name, "nop"))
    algo = make_unique<scea::Nop>();
  else if (boost::iequals(algo_name, "bfs"))
    algo = make_unique<scea::SSSP_BFS>(sssp_bfs_src);
  else
    throw runtime_error("unknown algo: expected one of `nop`, `bfs`");

  istream* in = &cin;
  ifstream input_file;
  if (input_file_path != "-") {
    input_file.open(input_file_path);
    in = &input_file;
  }

#ifndef NDEBUG
  once_flag warn_ignored_edges;
  auto const validate_vertex = [&warn_ignored_edges](uint64_t vertex) {
    if (vertex >= num_vertices) {
      call_once(warn_ignored_edges, []() {
        cerr << "warning: some edges were ignored because at least one vertex "
                "was out of range"
             << endl;
      });
      return false;
    }
    return true;
  };
#endif

  while (!in->eof()) { // for each batch
    /*
     * each line is a parallel insertion, of the form:
     * ```
     * src dst1 dst2 dst3 ...
     * ```
     */

    // parse the insertions
    vector<pair<uint64_t, vector<uint64_t>>> insertions;

    string batch_raw;
    while (getline(*in, batch_raw)) {
      if (batch_raw.length() == 0)
        break;

      istringstream batch(batch_raw);

      uint64_t src;
      batch >> src;
#ifndef NDEBUG
      if (!validate_vertex(src))
        break;
#endif

      vector<uint64_t> dsts;
      dsts.reserve(1);
      while (!batch.eof()) {
        uint64_t tmp;
        batch >> tmp;
#ifndef NDEBUG
        if (!validate_vertex(tmp))
          continue;
#endif
        dsts.emplace_back(tmp);
      }

      if (dsts.empty())
        throw runtime_error("operation must include destination edges");

      insertions.emplace_back(src, move(dsts));
    }

    // execute the insertions
    {
      // todo: benchmark this scope
      galois::setActiveThreads(ingest_threads);
      galois::do_all(galois::iterate(insertions.begin(), insertions.end()),
                     [&](pair<uint64_t, vector<uint64_t>> const& operation) {
                       graph->add_edges(operation.first, operation.second);
                     });
    }

    // execute the algorithm
    {
      // todo: benchmark this scope
      galois::setActiveThreads(algo_threads);
      (*algo)(*graph);
    }
  }

  return 0;
}