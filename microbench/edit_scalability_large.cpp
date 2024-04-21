// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <linux/mman.h>
#include <parallel_hashmap/phmap.h>

#include <filesystem>
#include <string>
#include <atomic>
#include <algorithm>

#include <boost/program_options.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include "galois/Galois.h"

#include "scea/algo/algo_interface.hpp"
#include "scea/graph/mutable_graph_interface.hpp"
#include "scea/stats.hpp"
#include "scea/algo/nop.hpp"
#include "scea/algo/bfs.hpp"
#include "scea/algo/tc.hpp"
#include "scea/algo/pr.hpp"
#include "scea/algo/bc.hpp"
#include "scea/graph/lscsr.hpp"
#include "scea/graph/morph.hpp"
#include "scea/graph/adj.hpp"
#include "scea/graph/lccsr.hpp"
#include "scea/graph/csr.hpp"

namespace bip = boost::interprocess;
namespace po  = boost::program_options;

using VertexVector = std::vector<uint64_t>;

struct Edge {
  uint64_t src;
  uint64_t dst;
} __attribute__((packed));

static_assert(sizeof(Edge) == 16, "Edge must be 16 bytes.");

/*
 * Prepares a batched edit in parallel from a binary edge list. The returned
 * adjacency lists are not in any particular order.
 */
std::vector<std::pair<uint64_t, VertexVector>>
prepare_batch(bip::file_mapping& file, uint64_t start_index,
              uint64_t end_index) {
  // time this function
  galois::StatTimer timer("prepare_batch");

  if (end_index <= start_index)
    return std::vector<std::pair<uint64_t, VertexVector>>();

  galois::setActiveThreads(galois::substrate::getThreadPool().getMaxThreads());

  struct spinlock_t {
    std::atomic<bool> m_lock = ATOMIC_VAR_INIT(false);
    void lock() {
      while (m_lock.load(std::memory_order_relaxed) ||
             m_lock.exchange(true, std::memory_order_acquire)) {
      }
    }
    void unlock() { m_lock.store(false, std::memory_order_release); }
  };

  // parallel hashmap
  phmap::parallel_flat_hash_map<
      uint64_t, VertexVector, std::hash<uint64_t>, std::equal_to<uint64_t>,
      std::allocator<std::pair<uint64_t, VertexVector>>, 8, spinlock_t>
      adj_map;
  {
    // read edges from file -- unmap when done!
    bip::mapped_region mapped_rgn(file, bip::read_only,
                                  start_index * sizeof(Edge),
                                  (end_index - start_index) * sizeof(Edge));
    mapped_rgn.advise(bip::mapped_region::advice_willneed);

    GALOIS_ASSERT(mapped_rgn.get_size() / sizeof(Edge) ==
                      end_index - start_index,
                  "Mapped region does not span the requested range.");
    Edge const* const start = reinterpret_cast<Edge*>(mapped_rgn.get_address());
    Edge const* const end   = start + (end_index - start_index);

    galois::do_all(
        galois::iterate(start, end),
        [&](Edge const& edge) {
          adj_map.lazy_emplace_l(
              edge.src,
              [&edge](auto& pair) { pair.second.emplace_back(edge.dst); },
              [&edge](auto const& cons) {
                cons(edge.src, VertexVector({edge.dst}));
              });
        },
        galois::no_stats(), galois::steal(),
        galois::chunk_size(4096 / sizeof(Edge)));
  }

  return std::vector<std::pair<uint64_t, VertexVector>>(
      std::make_move_iterator(adj_map.begin()),
      std::make_move_iterator(adj_map.end()));
}

int main(int argc, char const* argv[]) {
  galois::SharedMemSys sys;

  po::options_description generic(
      "Edit Scalability Benchmarking Suite (for Large Graphs)");
  generic.add_options()                 //
      ("help,h", "Print help messages") //
      ("file,f", po::value<std::string>(),
       "Input file in binary edge-list format (.bel).") //
      ("graph,g", po::value<std::string>()->default_value("lscsr"),
       "Graph representation (lscsr: log-structured CSR, morph: Galois "
       "MorphGraph, lccsr: Galois CSR, csr: custom CSR, adj: adjacency "
       "vector)") //
      ("algo,a", po::value<std::string>()->default_value("nop"),
       "Algorithm to run (nop: do nothing, bfs: compute single-source shortest "
       "path using BFS, pr: compute page rank, tc: "
       "compute triangle count)") //
      ("startup,s", po::value<float>()->default_value(0.0),
       "What fraction of the graph (between 0.0 and 1.0) is loaded before "
       "batching and benchmarking.") //
      ("num-batches,b", po::value<size_t>()->default_value(10),
       "How many batches to divide the input into.") //
      ("ingest-threads", po::value<size_t>()->default_value(1),
       "Number of threads for ingesting edges (0 for maximum)") //
      ("algo-threads", po::value<size_t>()->default_value(1),
       "Number of threads for the algorithm (0 for maximum)") //
      ("algo-output-file,o", po::value<std::string>(),
       "Output file for algorithm results (for verifying correctness).") //
      ("lscsr-compact-threshold", po::value<float>()->default_value(0.5),
       "Threshold at which LS_CSR performs a compaction"); //

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
    return 1;
  }

  // Check for help option
  if (vm.count("help")) {
    std::cout << cmdline_options << std::endl;
    return 1;
  }

  size_t ingest_threads = vm["ingest-threads"].as<size_t>();
  if (!ingest_threads)
    ingest_threads = galois::substrate::getThreadPool().getMaxThreads();
  size_t algo_threads = vm["algo-threads"].as<size_t>();
  if (!algo_threads)
    algo_threads = galois::substrate::getThreadPool().getMaxThreads();

  float const pre_ingest = vm["startup"].as<float>();
  GALOIS_ASSERT(pre_ingest >= 0.0 && pre_ingest <= 1.0,
                "startup fraction, if specified, must be in [0, 1].");

  std::string const graph_type = vm["graph"].as<std::string>();
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

  std::string const algo_name = vm["algo"].as<std::string>();
  std::unique_ptr<scea::algo::Algo> algo;
  if (algo_name == "nop") {
    algo = std::make_unique<scea::algo::Nop>();
  } else if (algo_name == "bfs") {
    if (!vm.count("bfs-src")) {
      std::cerr << "BFS requires a source vertex (specify with --bfs-src)"
                << std::endl;
      return 1;
    }
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

  std::string const input_file = vm["file"].as<std::string>();
  bip::file_mapping input_file_mapping(input_file.c_str(), bip::read_only);
  // determine how many edges there are
  uint64_t num_edges;
  {
    bip::mapped_region mapped_rgn(input_file_mapping, bip::read_only);
    mapped_rgn.advise(bip::mapped_region::advice_dontneed);
    num_edges = mapped_rgn.get_size() / sizeof(Edge);
    GALOIS_ASSERT(
        num_edges * sizeof(Edge) == mapped_rgn.get_size(),
        "Invalid binary edge list; size is not a multiple of 16 bytes.");
  }
  std::cout << "Input file has " << num_edges << " edges." << std::endl;

  // Prepare algorithm output file.
  std::ofstream algo_output;
  if (vm.count("algo-output-file")) {
    algo_output.open(vm["algo-output-file"].as<std::string>().c_str(),
                     std::ofstream::trunc);
  }

  uint64_t const num_edges_to_pre_ingest = floor(num_edges * pre_ingest);
  if (num_edges_to_pre_ingest > 0) {
    // pre-ingest could be large, so it is worth parallelizing
    uint64_t const num_edges_to_ingest = floor(num_edges * pre_ingest);
    std::cout << "Loading " << num_edges_to_ingest
              << " edges before batching..." << std::flush;
    auto batch = prepare_batch(input_file_mapping, 0, num_edges_to_ingest);
    std::cout << " Done." << std::endl;
    {
      BENCHMARK_SCOPE("Pre-batching ingestion");
      graph->ingest(std::move(batch));
    }
    {
      BENCHMARK_SCOPE("Post-batching post-ingest");
      graph->post_ingest();
    }
    std::cout << "Graph has " << graph->size() << " nodes." << std::endl;
  }

  uint64_t const num_batches = vm["num-batches"].as<uint64_t>();
  uint64_t const num_edges_per_batch =
      (num_edges - num_edges_to_pre_ingest + (num_batches - 1)) / num_batches;

  for (uint64_t batch_number = 0; batch_number < num_batches; ++batch_number) {
    auto const start_edge =
        std::min(num_edges,
                 num_edges_to_pre_ingest + batch_number * num_edges_per_batch);
    auto const end_edge =
        std::min(num_edges, num_edges_to_pre_ingest +
                                (batch_number + 1) * num_edges_per_batch);
    auto batch = prepare_batch(input_file_mapping, start_edge, end_edge);

    std::cout << "Batch " << batch_number << " has " << batch.size()
              << " modified vertices and " << end_edge - start_edge
              << " total edges" << std::endl;

    galois::setActiveThreads(
        ingest_threads ? ingest_threads
                       : galois::substrate::getThreadPool().getMaxThreads());
    {
      BENCHMARK_SCOPE("Ingestion for Batch " + std::to_string(batch_number));
      graph->ingest(std::move(batch));
    }
    {
      BENCHMARK_SCOPE("Post-ingest for Batch " + std::to_string(batch_number));
      graph->post_ingest();
    }

    galois::setActiveThreads(
        algo_threads ? algo_threads
                     : galois::substrate::getThreadPool().getMaxThreads());
    {
      if (algo_output.is_open()) {
      }

      BENCHMARK_SCOPE("Algorithm for Batch " + std::to_string(batch_number));
      if (algo_output.is_open()) {
        algo_output << "==== running " << algo_name << " after batch "
                    << batch_number << " ==== " << std::endl;
        (*algo)(*graph, algo_output);
      } else {
        (*algo)(*graph);
      }
    }
  }

  return 0;
}
