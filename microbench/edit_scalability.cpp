// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

#include <boost/program_options.hpp>

#include "scea/algo/bfs.hpp"
#include "scea/algo/nop.hpp"
#include "scea/graph/lscsr.hpp"
#include "scea/graph/morph.hpp"
#include "scea/perf.hpp"

enum GraphType { lscsr, morph };
enum AlgoName { nop, sssp_bfs };

std::istream& operator>>(std::istream& in, GraphType& type) {
  std::string name;
  in >> name;

  if (name == "lscsr") {
    type = lscsr;
  } else if (name == "morph") {
    type = morph;
  } else {
    // Handle invalid input (throw exception, print error message, etc.)
    in.setstate(std::ios_base::failbit);
  }

  return in;
}

std::istream& operator>>(std::istream& in, AlgoName& name) {
  std::string type;
  in >> type;

  if (type == "nop") {
    name = nop;
  } else if (type == "bfs") {
    name = sssp_bfs;
  } else {
    // Handle invalid input (throw exception, print error message, etc.)
    in.setstate(std::ios_base::failbit);
  }

  return in;
}

int main(int argc, char const* argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Edit Scalability Benchmarking Suite");
  desc.add_options()                                              //
      ("help,h", "Print help messages")                           //
      ("input-file", po::value<std::string>(), "Input file path") //
      ("ingest-threads", po::value<size_t>()->default_value(1),
       "Number of threads for ingesting edges") //
      ("algo-threads", po::value<size_t>()->default_value(1),
       "Number of threads for the algorithm") //
      ("num-vertices", po::value<uint64_t>()->required(),
       "Number of vertices in the graph") //
      ("graph,g", po::value<GraphType>()->default_value(lscsr),
       "Graph representation (lscsr: log-structured CSR, morph: Galois "
       "MorphGraph)") //
      ("algo", po::value<AlgoName>()->default_value(nop),
       "Algorithm to run (nop: do nothing, bfs: compute "
       "single-source shortest path using BFS)")                              //
      ("bfs-src", po::value<uint64_t>(), "Source vertex (for BFS algorithm)") //
      ;

  po::variables_map vm;
  try {
    // Parse command line arguments
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (po::error& e) {
    std::cout << e.what() << std::endl;
    std::cout << desc << std::endl;
  }

  // Check for help option
  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return 1;
  }

  std::string const input_file_path = vm["input-file"].as<std::string>();
  size_t const ingest_threads       = vm["ingest-threads"].as<size_t>();
  size_t const algo_threads         = vm["algo-threads"].as<size_t>();
  uint64_t const num_vertices       = vm["num-vertices"].as<uint64_t>();
  GraphType const graph_type        = vm["graph"].as<GraphType>();
  AlgoName const algo_name          = vm["algo"].as<AlgoName>();

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
  case AlgoName::nop: {
    algo = std::make_unique<scea::algo::Nop>();
    break;
  }
  case AlgoName::sssp_bfs: {
    algo = std::make_unique<scea::algo::SSSP_BFS>(vm["bfs-src"].as<uint64_t>());
    break;
  }
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
  auto const validate_vertex = [&warn_ignored_edges,
                                num_vertices](uint64_t vertex) {
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
