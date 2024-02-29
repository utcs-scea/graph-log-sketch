// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

#include "llvm/Support/CommandLine.h"
#include "scea/algo/bfs.hpp"
#include "scea/algo/nop.hpp"
#include "scea/graph/lscsr.hpp"
#include "scea/graph/morph.hpp"
#include "scea/perf.hpp"

static const char* name = "Edit Scalability Benchmarking Suite";
static const char* desc = "Creates graphs from files in order to see how"
                          " edits impact algorithm performance.";
namespace cl            = llvm::cl;

/*
 * Command-line args
 */

cl::opt<std::string> input_file_path(cl::Positional, cl::desc("[input file]"),
                                     cl::init("-"));

cl::opt<size_t>
    ingest_threads("ingest-threads",
                   cl::desc("number of threads used for ingesting edges"),
                   cl::init(1));

cl::opt<size_t> algo_threads("algo-threads",
                             cl::desc("number of threads used for algorithm"),
                             cl::init(1));

cl::opt<uint64_t> num_vertices("num-vertices",
                               cl::desc("number of vertices in the graph"),
                               cl::Required);

enum GraphType { lscsr, morph };

cl::opt<GraphType>
    graph_type("graph", cl::desc("Choose graph representation:"),
               cl::init(lscsr),
               cl::values(clEnumValN(lscsr, "lscsr", "log-structured CSR"),
                          clEnumValN(morph, "morph", "Galois MorphGraph")));
cl::alias graph_type_alias("g", cl::desc("Alias for --graph"),
                           cl::aliasopt(graph_type));

/*
 * Algo args
 */

enum AlgoName { nop, sssp_bfs };

cl::opt<AlgoName> algo_name(
    "algo", cl::desc("Choose algorithm to run:"), cl::init(nop),
    cl::values(
        clEnumVal(nop, "do nothing"),
        clEnumValN(
            sssp_bfs, "bfs",
            "compute single-source shortest path using a parallel BFS")));
cl::alias algo_name_alias("a", cl::desc("Alias for --algo"),
                          cl::aliasopt(algo_name));

cl::OptionCategory sssp_bfs_category("Options specific to BFS algorithm:");

cl::opt<uint64_t> sssp_bfs_src("bfs-src", cl::desc("the source vertex"),
                               cl::cat(sssp_bfs_category));

int main(int argc, char const* argv[]) {
  // parse args
  cl::ParseCommandLineOptions(argc, argv);

  galois::SharedMemSys sys;

  // validate args
  if (ingest_threads == 0)
    throw std::runtime_error("ingest threads must be greater than zero");
  if (algo_threads == 0)
    throw std::runtime_error("algo threads must be greater than zero");

  std::unique_ptr<scea::graph::MutableGraph> graph;
  switch (graph_type) {
  case GraphType::lscsr: {
    graph = std::make_unique<scea::graph::LS_CSR>(num_vertices);
    break;
  }
  case GraphType::morph: {
    graph = std::make_unique<scea::graph::MorphGraph>(num_vertices);
    break;
  }
  default:
    throw std::runtime_error("unknown graph_type");
  }

  std::unique_ptr<scea::algo::Algo> algo;
  switch (algo_name) {
  case AlgoName::nop:
    algo = std::make_unique<scea::algo::Nop>();
    break;
  case AlgoName::sssp_bfs:
    algo = std::make_unique<scea::algo::SSSP_BFS>(sssp_bfs_src);
    break;
  default:
    throw std::runtime_error("unknown algorithm");
  }

  std::istream* in = &std::cin;
  std::ifstream input_file;
  if (input_file_path != "-") {
    input_file.open(input_file_path);
    in = &input_file;
  }

#ifndef NDEBUG
  std::once_flag warn_ignored_edges;
  auto const validate_vertex = [&warn_ignored_edges](uint64_t vertex) {
    if (vertex >= num_vertices) {
      std::call_once(warn_ignored_edges, []() {
        std::cerr << "warning: some edges were ignored because at least one "
                     "vertex "
                     "was out of range"
                  << std::endl;
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
    std::vector<std::pair<uint64_t, std::vector<uint64_t>>> insertions;

    std::string batch_raw;
    while (std::getline(*in, batch_raw)) {
      if (batch_raw.length() == 0)
        break;

      std::istringstream batch(batch_raw);

      uint64_t src;
      batch >> src;
#ifndef NDEBUG
      if (!validate_vertex(src))
        break;
#endif

      std::vector<uint64_t> dsts;
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
        throw std::runtime_error("operation must include destination edges");

      insertions.emplace_back(src, std::move(dsts));
    }

    // execute the insertions
    if (!insertions.empty()) {
      BENCHMARK_SCOPE("Ingestion");

      galois::setActiveThreads(ingest_threads);
      galois::do_all(
          galois::iterate(insertions.begin(), insertions.end()),
          [&](std::pair<uint64_t, std::vector<uint64_t>> const& operation) {
            graph->add_edges(operation.first, operation.second);
          });
    }

    // execute the algorithm
    {
      BENCHMARK_SCOPE("Algorithm");

      galois::setActiveThreads(algo_threads);
      (*algo)(*graph);
    }
  }

  return 0;
}
