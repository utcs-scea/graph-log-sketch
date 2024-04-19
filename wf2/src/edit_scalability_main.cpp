// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include "galois/DistGalois.h"
#include "galois/graphs/GluonSubstrate.h"
#include "galois/runtime/DataCommMode.h"
#include "galois/graphs/DistributedLocalGraph.h"
#include "galois/wmd/WMDPartitioner.h"
#include "galois/graphs/GenericPartitioners.h"
#include "galois/DTerminationDetector.h"
#include "galois/DReducible.h"
#include "galois/gstl.h"
#include "galois/runtime/SyncStructures.h"
#include "galois/runtime/Tracer.h"
#include "galois/runtime/GraphUpdateManager.h"

#include "galois/wmd/schema.h"
#include "galois/wmd/graph.h"
#include "galois/shad/DataTypes.h"
#include "galois/wmd/graphTypes.h"
#include "graph_ds.hpp"
#include "import.hpp"
#include "pattern.hpp"

#define DBG_PRINT(x)                                                           \
  { std::cout << "[WF2-DEBUG] " << x << std::endl; }

std::string ZeroPadNumber(int num) {
  std::ostringstream ss;
  ss << std::setw(2) << std::setfill('0') << num;
  return ss.str();
}

struct NodeData {};

void parse(std::string) {}

std::vector<std::vector<uint64_t>>
genMirrorNodes(wf2::Graph& hg, std::string filename, int batch,
               wf2::Wf2WMDParser<wf2::Vertex, wf2::Edge> parser) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::vector<std::vector<uint64_t>> delta_mirrors(net.Num);

  for (uint32_t i = 0; i < net.Num; i++) {
    std::string dynamicFile = filename + "_" + ZeroPadNumber(net.ID) + "_" +
                              ZeroPadNumber(i) + ".csv";
    std::ifstream file(dynamicFile);
    std::string line;
    while (std::getline(file, line)) {
      std::vector<std::string> tokens =
          parser.SplitLine(line.c_str(), line.size(), ',', 10);
      std::uint64_t src = 0;
      std::uint64_t dst = 0;
      if (tokens[0] == "Sale") {
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[2]);
      } else if (tokens[0] == "Purchase") {
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[2]);
      } else if (tokens[0] == "Author") {
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
        if (tokens[3] != "")
          dst =
              shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
        else if (tokens[4] != "")
          dst =
              shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
        else if (tokens[5] != "")
          dst =
              shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      } else if (tokens[0] == "Includes") {
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      } else if (tokens[0] == "HasTopic") {
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
        if (tokens[3] != "")
          src =
              shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
        else if (tokens[4] != "")
          src =
              shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
        else if (tokens[5] != "")
          src =
              shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      } else if (tokens[0] == "HasOrg") {
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      }
      if (src != 0 && dst != 0) {
        if ((hg.isOwned(src)) && (!hg.isLocal(dst))) {
          uint32_t h = hg.getHostID(dst);
          delta_mirrors[h].push_back(dst);
        }
        if ((hg.isOwned(dst)) && (!hg.isLocal(src))) {
          uint32_t h = hg.getHostID(src);
          delta_mirrors[h].push_back(src);
        }
      }
    }
  }
  return delta_mirrors;
}

wf2::Graph* g;
std::unique_ptr<galois::graphs::GluonSubstrate<wf2::Graph>> sync_substrate;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Expected graph file name as argument\n";
    return 1;
  }
  int num_threads = 1;
  if (argc == 3) {
    num_threads = std::stoi(argv[2]);
  }
  std::string filename(argv[1]);
  DBG_PRINT("Reading from file: " << filename);

  galois::DistMemSys G; // init galois memory
  galois::setActiveThreads(num_threads);
  auto& net = galois::runtime::getSystemNetworkInterface();

  if (net.ID == 0) {
    DBG_PRINT("Num Hosts: " << net.Num << ", Active Threads Per Hosts: "
                            << galois::getActiveThreads() << "\n");
  }

  g = wf2::ImportGraph(filename);
  galois::graphs::WMDGraph<wf2::Vertex, wf2::Edge, OECPolicy>* wg =
      dynamic_cast<
          galois::graphs::WMDGraph<wf2::Vertex, wf2::Edge, OECPolicy>*>(g);

  assert(g != nullptr);
  sync_substrate = std::make_unique<galois::graphs::GluonSubstrate<wf2::Graph>>(
      *g, net.ID, net.Num, g->isTransposed(), g->cartesianGrid());

  galois::runtime::getHostBarrier().wait();
  int num_batches = 25;

  for (int i = 0; i < num_batches; i++) {
    // PrintMasterMirrorNodes(*hg, net.ID);

    std::vector<std::string> edit_files;
    std::string dynFile = "./graphs/data.01";
    std::string dynamicFile =
        dynFile + "_" + ZeroPadNumber(net.ID) + "_" + ZeroPadNumber(i) + ".csv";
    std::cout << dynamicFile << std::endl;
    edit_files.emplace_back(dynamicFile);
    // IMPORTANT: CAll genMirrorNodes before creating the
    // graphUpdateManager!!!!!!!!
    wf2::Wf2WMDParser<wf2::Vertex, wf2::Edge> parser(1, edit_files);
    std::vector<std::vector<uint64_t>> delta_mirrors =
        genMirrorNodes(*g, dynFile, i, parser);
    graphUpdateManager<wf2::Vertex, wf2::Edge> GUM(
        std::make_unique<wf2::Wf2WMDParser<wf2::Vertex, wf2::Edge>>(1,
                                                                    edit_files),
        100, g);
    GUM.start();
    /*while (!GUM.stop()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(GUM.getPeriod()));
    }
    galois::runtime::getHostBarrier().wait();
    GUM.stop2();


    syncSubstrate->addDeltaMirrors(delta_mirrors);
    PrintMasterMirrorNodes(*hg, net.ID);
    galois::runtime::getHostBarrier().wait();*/
  }

  // wf2::MatchPattern(*g, net);

  return 0;
}
