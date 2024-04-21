// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>
#include <unordered_map>

#include <boost/program_options.hpp>

#include "scea/algo/bfs.hpp"
#include "scea/algo/nop.hpp"
#include "scea/algo/tc.hpp"
#include "scea/algo/pr.hpp"
#include "scea/algo/bc.hpp"
#include "scea/graph/lscsr.hpp"
#include "scea/graph/morph.hpp"
#include "scea/graph/adj.hpp"
#include "scea/graph/lccsr.hpp"
#include "scea/graph/csr.hpp"
#include "scea/stats.hpp"

int main(int argc, char const* argv[]) {
  namespace po = boost::program_options;

  po::options_description generic("Edit Scalability Benchmarking Suite");
  generic.add_options()                                           //
      ("help,h", "Print help messages")                           //
      ("input-file", po::value<std::string>(), "Input file path") //
      ("ingest-threads", po::value<size_t>()->default_value(1),
       "Number of threads for ingesting edges") //
      ("algo-threads", po::value<size_t>()->default_value(1),
       "Number of threads for the algorithm") //
      ("num-vertices", po::value<uint64_t>()->required(),
       "Number of vertices in the graph") //
      ("graph,g", po::value<std::string>()->default_value("lscsr"),
       "Graph representation (lscsr: log-structured CSR, morph: Galois "
       "MorphGraph, lccsr: Galois CSR, csr: custom CSR, adj: adjacency "
       "vector)") //
      ("lscsr-compact-threshold", po::value<float>()->default_value(0.3),
       "Threshold at which LS_CSR performs a compaction") //
      ("algo", po::value<std::string>()->default_value("nop"),
       "Algorithm to run (nop: do nothing, bfs: compute "
       "single-source shortest path using BFS)");

  po::options_description bfs_opts("Options for BFS");
  bfs_opts.add_options() //
      ("bfs-src", po::value<uint64_t>(), "Source vertex.");

  po::options_description bc_opts("Options for Betweenness Centrality");
  bc_opts.add_options() //
      ("bc-rseed", po::value<unsigned int>()->default_value(0),
       "Random seed for sampling source vertices (for BC algorithm)") //
      ("bc-num-src", po::value<uint64_t>()->default_value(0),
       "Number of source vertices (for BC algorithm); 0 uses all vertices.");

  po::options_description cmdline_options;
  cmdline_options.add(generic).add(bfs_opts).add(bc_opts);

  po::variables_map vm;
  try {
    // Parse command line arguments
    po::store(
        po::command_line_parser(argc, argv).options(cmdline_options).run(), vm);
    po::notify(vm);
  } catch (po::error& e) {
    std::cout << e.what() << std::endl;
  }

  // Check for help option
  if (vm.count("help")) {
    std::cout << cmdline_options << std::endl;
    return 1;
  }

  std::string const input_file_path = vm["input-file"].as<std::string>();
  size_t const ingest_threads       = vm["ingest-threads"].as<size_t>();
  size_t const algo_threads         = vm["algo-threads"].as<size_t>();
  uint64_t const num_vertices       = vm["num-vertices"].as<uint64_t>();
  std::string const graph_type      = vm["graph"].as<std::string>();
  std::string const algo_name       = vm["algo"].as<std::string>();

  galois::SharedMemSys sys;

  // validate args
  if (ingest_threads == 0) {
    std::cerr << "ingest threads must be greater than zero" << std::endl;
    return 1;
  }

  if (algo_threads == 0) {
    std::cerr << "algo threads must be greater than zero" << std::endl;
    return 1;
  }

  std::unique_ptr<scea::graph::MutableGraph> graph;
  if (graph_type == "lscsr") {
    float thresh = vm["lscsr-compact-threshold"].as<float>();
    graph        = std::make_unique<scea::graph::LS_CSR>(thresh);
  } else if (graph_type == "lccsr") {
    graph = std::make_unique<scea::graph::LC_CSR>();
  } else if (graph_type == "csr") {
    graph = std::make_unique<scea::graph::CSR>();
  } else if (graph_type == "morph") {
    graph = std::make_unique<scea::graph::MorphGraph>();
  } else if (graph_type == "adj") {
    graph = std::make_unique<scea::graph::AdjGraph>();
  } else {
    std::cerr << "unknown graph type: " << graph_type << std::endl;
    return 1;
  }

  std::unique_ptr<scea::algo::Algo> algo;
  if (algo_name == "nop") {
    algo = std::make_unique<scea::algo::Nop>();
  } else if (algo_name == "bfs") {
    algo = std::make_unique<scea::algo::SSSP_BFS>(vm["bfs-src"].as<uint64_t>());
  } else if (algo_name == "tc") {
    algo = std::make_unique<scea::algo::TriangleCounting>();
  } else if (algo_name == "pr") {
    algo = std::make_unique<scea::algo::PageRank>();
  } else if (algo_name == "bc") {
    auto const rseed   = vm["bc-rseed"].as<unsigned int>();
    auto const num_src = vm["bc-num-src"].as<uint64_t>();
    algo = std::make_unique<scea::algo::BetweennessCentrality>(rseed, num_src);
  } else {
    std::cerr << "unknown algorithm: " << algo_name << std::endl;
    return 1;
  }

  std::istream* in = &std::cin;
  std::ifstream input_file;
  if (input_file_path != "-") {
    input_file.open(input_file_path);
    in = &input_file;
  }

#ifndef NDEBUG
  uint64_t current_line = 0;
  std::once_flag warn_ignored_edges;
  auto const validate_vertex = [&warn_ignored_edges, &current_line,
                                num_vertices](uint64_t vertex) {
    if (vertex >= num_vertices) {
      std::call_once(warn_ignored_edges, [&current_line]() {
        std::cerr << "warning on input line " << current_line
                  << ": some edges were ignored because at least one "
                     "vertex was out of range"
                  << std::endl;
      });
      return false;
    }
    return true;
  };
#endif
  int batch = 0;

  while (!in->eof()) { // for each batch
    /*
     * each line is a parallel insertion, of the form:
     * ```
     * src dst1 dst2 dst3 ...
     * ```
     */

    // parse the insertions
    std::unordered_map<uint64_t, std::vector<uint64_t>> insertions;
    uint64_t num_edges = 0;

    std::string batch_raw;
    while (std::getline(*in, batch_raw)) {
#ifndef NDEBUG
      ++current_line;
#endif
      if (batch_raw.length() == 0)
        break;

      std::istringstream batch(batch_raw);

      uint64_t src;
      batch >> src;
#ifndef NDEBUG
      if (!validate_vertex(src))
        break;
#endif

      while (!batch.eof()) {
        uint64_t tmp;
        batch >> tmp;
#ifndef NDEBUG
        if (!validate_vertex(tmp))
          continue;
#endif
        ++num_edges;
        insertions[src].emplace_back(tmp);
      }
    }

    std::vector<std::pair<uint64_t, std::vector<uint64_t>>> insertions_vec(
        std::make_move_iterator(insertions.begin()),
        std::make_move_iterator(insertions.end()));
    std::cout << "batch has " << insertions_vec.size()
              << " modified vertices and " << num_edges << " total edges"
              << std::endl;
    if (!insertions_vec.empty()) {
      // ingest
      {
        BENCHMARK_SCOPE("Ingestion for Batch " + std::to_string(batch));
        galois::setActiveThreads(ingest_threads);
        graph->ingest(std::move(insertions_vec));
        std::cout << "graph size after ingest: " << graph->size() << std::endl;
      }
      // post-ingest
      {
        BENCHMARK_SCOPE("Post-ingest for Batch " + std::to_string(batch));

        graph->post_ingest();
      }
    }

    // execute the algorithm
    {
      BENCHMARK_SCOPE("Algorithm for Batch " + std::to_string(batch));

      galois::setActiveThreads(algo_threads);
      (*algo)(*graph);
    }
    batch++;
  }

  return 0;
}
