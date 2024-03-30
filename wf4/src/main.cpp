// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <iostream>

#include "galois/DistGalois.h"
#include "galois/runtime/DataCommMode.h"
#include "wf4/import.hpp"
#include "wf4/influencer.hpp"
#include "wf4/quiesce.hpp"

static const char* name = "Network of Networks";

galois::DynamicBitSet bitset_bought_;
galois::DynamicBitSet bitset_sold_;

namespace {

void printUsageExit(char* argv0) {
  std::printf("Usage: %s "
              "[-t <num threads>] "
              "[-k <num-influential-nodes>] "
              "[-r <number-reverse-reachable-sets>] "
              "[-s <random-seed>] "
              "[-e <num epochs to generate rrr sets>] "
              "[-d <input directory with all 5 csvs>] "
              "[-c <commercial-path>] "
              "[-y <cyber-path>] "
              "[-o <social-path>] "
              "[-u <uses-path>] "
              "[-n <nodes-path>]\n",
              argv0);
  std::exit(EXIT_FAILURE);
}

struct ProgramOptions {
  ProgramOptions() = default;

  void Parse(int argc, char** argv) {
    // Other libraries may have called getopt before, so we reset optind for
    // correctness
    optind = 0;

    int opt;
    std::string file;
    while ((opt = getopt(argc, argv, "t:k:r:s:e:d:c:y:o:u:n:")) != -1) {
      switch (opt) {
      case 't':
        num_threads = strtoull(optarg, nullptr, 10);
        break;
      case 'k':
        k = strtoull(optarg, nullptr, 10);
        break;
      case 'r':
        rrr = strtoull(optarg, nullptr, 10);
        break;
      case 's':
        seed = strtoull(optarg, nullptr, 10);
        break;
      case 'e':
        epochs = strtoull(optarg, nullptr, 10);
        break;
      case 'd':
        file            = std::string(optarg);
        input_directory = file;
        break;
      case 'c':
        file                  = std::string(optarg);
        commercial_input_file = file;
        break;
      case 'y':
        file             = std::string(optarg);
        cyber_input_file = file;
        break;
      case 'o':
        file              = std::string(optarg);
        social_input_file = file;
        break;
      case 'u':
        file            = std::string(optarg);
        uses_input_file = file;
        break;
      case 'n':
        file             = std::string(optarg);
        nodes_input_file = file;
        break;
      default:
        printUsageExit(argv[0]);
        exit(-1);
      }
    }
    if (!Verify()) {
      printUsageExit(argv[0]);
      exit(-1);
    }
  }

  bool Verify() {
    if (k <= 0) {
      return false;
    }
    if (rrr <= 0) {
      return false;
    }
    if (input_directory.empty() && commercial_input_file.empty() &&
        cyber_input_file.empty() && social_input_file.empty() &&
        uses_input_file.empty() && nodes_input_file.empty()) {
      return false;
    }
    return true;
  }

  uint64_t num_threads = 16;
  uint64_t k           = 100;
  uint64_t rrr         = 100000;
  uint64_t seed        = 98011089;
  uint64_t epochs      = 100;

  std::string input_directory;

  std::string commercial_input_file;
  std::string cyber_input_file;
  std::string social_input_file;
  std::string uses_input_file;
  std::string nodes_input_file;

  // debug argument for verifying correctness on artificial graphs, unused
  uint64_t influential_node_threshold = 0;
} typedef ProgramOptions;

template <typename T>
uint64_t countEdges(T& graph) {
  uint64_t edges                = 0;
  uint64_t person_nodes         = 0;
  uint64_t trading_edges        = 0;
  uint64_t trading_person_edges = 0;

  for (auto src : graph.masterNodesRange()) {
    bool owned = graph.isOwned(graph.getGID(src));
    owned      = graph.isLocal(graph.getGID(src));
    auto& node = graph.getData(src);
    if (node.type_ == agile::workflow1::TYPES::PERSON) {
      // person_nodes++;
    }
    // node.id = 1;
    /*
    std::cout << "LID: " << src << ", GID: " << graph.getGID(src)
              << ", LID2: " << graph.getLID(graph.getGID(src))
              << ", UID: " << graph.getData(src).id << std::endl;
              */
    edges += std::distance(graph.edges(src).begin(), graph.edges(src).end());
    bool has_valid_edges = false;

    for (auto edge : graph.edges(src)) {
      try {
        auto data     = graph.getEdgeData(edge);
        auto dst_lid  = graph.getEdgeDst(edge);
        auto gid      = graph.getGID(graph.getEdgeDst(edge));
        owned         = graph.isOwned(gid);
        owned         = graph.isLocal(gid);
        auto dst_node = graph.getData(dst_lid);
        if (data.type == agile::workflow1::TYPES::SALE ||
            data.type == agile::workflow1::TYPES::PURCHASE) {
          if (data.topic = 8486 && data.amount_ > 0) {
            trading_edges++;

            if (node.type_ == agile::workflow1::TYPES::PERSON) {
              has_valid_edges = true;
              trading_person_edges++;
            }
          }
        }
      } catch (std::out_of_range) {
        std::cout << "Overflow DST: "
                  << "NaN"
                  << ", SRC: " << src << ", SIZE: " << graph.size()
                  << std::endl;
      }
      // node.id = 1;
      /*
      std::cout << "SRC_LID: " << src << ", DST_LID: " << graph.getEdgeDst(edge)
                << ", DST_GID: " << graph.getGID(graph.getEdgeDst(edge))
                << ", DST_UID: " << graph.getData(graph.getEdgeDst(edge)).id
                << std::endl;
      std::cout << "EDGE: SRC_UID: " << data.src
                << ", SRC_GID: " << data.src_glbid << ", DST_UID: " << data.dst
                << ", DST_GID: " << data.dst_glbid
                << ", AMOUNT: " << data.amount_ << std::endl;
                */
    }
    if (has_valid_edges) {
      person_nodes++;
    }
  }
  std::cout << "Person nodes: " << person_nodes << std::endl;
  std::cout << "Trading edges: " << trading_edges << std::endl;
  std::cout << "Trading person edges: " << trading_person_edges << std::endl;
  return edges;
}

template <typename T>
void printGraphStatisticsDebug(T& graph) {
  auto num_edges = countEdges(graph);
  galois::DGAccumulator<uint64_t> global_nodes;
  global_nodes.reset();
  global_nodes += graph.numMasters();
  global_nodes.reduce();
  galois::DGAccumulator<uint64_t> global_edges;
  global_edges.reset();
  global_edges += num_edges;
  global_edges.reduce();
  std::cout << "Total Nodes: " << global_nodes.read() << std::endl;
  std::cout << "Total Edges: " << global_edges.read() << std::endl;
  std::cout << "Masters in graph: " << graph.numMasters() << std::endl;
  std::cout << "Local nodes in graph: " << graph.size() << std::endl;
  std::cout << "Local edges in graph: " << num_edges << std::endl;
}

} // end namespace

int main(int argc, char* argv[]) {
  ProgramOptions program_options;
  program_options.Parse(argc, argv);
  galois::DistMemSys G;
  galois::setActiveThreads(program_options.num_threads);
  auto& net = galois::runtime::getSystemNetworkInterface();

  wf4::InputFiles input_files(
      program_options.input_directory, program_options.commercial_input_file,
      program_options.cyber_input_file, program_options.social_input_file,
      program_options.uses_input_file, program_options.nodes_input_file);
  if (net.ID == 0) {
    input_files.Print();
  }

  std::unique_ptr<wf4::FullNetworkGraph> full_graph =
      wf4::ImportData(input_files);
  galois::runtime::getHostBarrier().wait();
  printGraphStatisticsDebug(*full_graph);
  std::unique_ptr<wf4::NetworkGraph> projected_graph =
      wf4::ProjectGraph(std::move(full_graph));
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      sync_substrate =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              *projected_graph, net.ID, net.Num,
              projected_graph->isTransposed(),
              projected_graph->cartesianGrid());

  // printGraphStatisticsDebug(*projected_graph);

  wf4::CalculateEdgeProbabilities(*projected_graph, *sync_substrate);
  wf4::ReverseReachableSet reachability_sets =
      wf4::GetRandomReverseReachableSets(*projected_graph, program_options.rrr,
                                         program_options.seed,
                                         program_options.epochs);
  std::vector<wf4::GlobalNodeID> influencers = wf4::GetInfluentialNodes(
      *projected_graph, std::move(reachability_sets), program_options.k,
      program_options.influential_node_threshold);

  wf4::CancelNodes(*projected_graph, *sync_substrate, influencers);
  wf4::QuiesceGraph(*projected_graph, *sync_substrate);
}
