// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <llvm/Support/CommandLine.h>

#include <iostream>

#include "galois/DistGalois.h"
#include "galois/runtime/DataCommMode.h"
#include "import.hpp"
#include "influencer.hpp"
#include "quiesce.hpp"

namespace cll = llvm::cl;

static const char* name = "Network of Networks";

static const cll::opt<uint64_t> num_threads("t", cll::desc("<num threads>"),
                                            cll::init(16));

static const cll::opt<uint64_t> k("k", cll::desc("<num influential nodes>"),
                                  cll::init(100));

static const cll::opt<uint64_t> seed("seed", cll::desc("<seed>"),
                                     cll::init(98011089));

static const cll::opt<uint64_t> rrr("rrr", cll::desc("<num rrr sets>"),
                                    cll::init(100000));

static const cll::opt<uint64_t>
    epochs("epochs", cll::desc("<num epochs to generate rrr sets>"),
           cll::init(100));

static const cll::opt<std::string>
    input_directory("input-dir", cll::desc("<input directory with all 5 csvs>"),
                    cll::init(""));

static const cll::opt<std::string>
    commercial_input_file("commercial-file",
                          cll::desc("<commercial input file>"), cll::init(""));
static const cll::opt<std::string>
    cyber_input_file("cyber-file", cll::desc("<cyber input file>"),
                     cll::init(""));
static const cll::opt<std::string>
    social_input_file("social-file", cll::desc("<social input file>"),
                      cll::init(""));
static const cll::opt<std::string>
    uses_input_file("uses-file", cll::desc("<uses input file>"), cll::init(""));
static const cll::opt<std::string>
    nodes_input_file("nodes-file", cll::desc("<nodes input file>"),
                     cll::init(""));

static const cll::opt<std::string> graph_name("name", cll::desc("<graph name>"),
                                              cll::init("commercial"));

static const cll::opt<uint64_t> influential_node_threshold(
    "influential-threshold",
    cll::desc(
        "<debug argument for verifying correctness on artificial graphs>"),
    cll::init(0));

galois::DynamicBitSet bitset_bought_;
galois::DynamicBitSet bitset_sold_;

namespace {

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
    if (node.type_ == gls::wmd::TYPES::PERSON) {
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
        if (data.type == gls::wmd::TYPES::SALE ||
            data.type == gls::wmd::TYPES::PURCHASE) {
          if (data.topic = 8486 && data.amount_ > 0) {
            trading_edges++;

            if (node.type_ == gls::wmd::TYPES::PERSON) {
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
  cll::ParseCommandLineOptions(argc, argv);
  galois::DistMemSys G;
  galois::setActiveThreads(num_threads);
  auto& net = galois::runtime::getSystemNetworkInterface();

  wf4::InputFiles input_files(input_directory, commercial_input_file,
                              cyber_input_file, social_input_file,
                              uses_input_file, nodes_input_file);
  if (net.ID == 0) {
    input_files.Print();
  }

  std::unique_ptr<wf4::FullNetworkGraph> full_graph =
      wf4::ImportData(input_files);
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
      wf4::GetRandomReverseReachableSets(*projected_graph, rrr, seed, epochs);
  std::vector<wf4::GlobalNodeID> influencers =
      wf4::GetInfluentialNodes(*projected_graph, std::move(reachability_sets),
                               k, influential_node_threshold);

  wf4::CancelNodes(*projected_graph, *sync_substrate, influencers);
  wf4::QuiesceGraph(*projected_graph, *sync_substrate);
}
