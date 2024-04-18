// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <iostream>
#include "../include/importer.cpp"
#include "../include/scea/stats.hpp"
#include "galois/graphs/DistributedLocalGraph.h"
#include "galois/graphs/GluonSubstrate.h"
#include "galois/wmd/WMDPartitioner.h"
#include "galois/graphs/GenericPartitioners.h"
#include "galois/DTerminationDetector.h"
#include "galois/DistGalois.h"
#include "galois/DReducible.h"
#include "galois/gstl.h"
#include "galois/DistGalois.h"
#include "galois/runtime/SyncStructures.h"
#include "galois/DReducible.h"
#include "galois/DTerminationDetector.h"
#include "galois/gstl.h"
#include "galois/runtime/Tracer.h"
#include "galois/runtime/GraphUpdateManager.h"

#include <iostream>
#include <limits>
#include <unordered_map>
#include <string>
#include <fstream>
#include <sstream>

const uint32_t infinity = std::numeric_limits<uint32_t>::max() / 4;

struct NodeData {
  std::atomic<uint32_t> dist_current;
  uint32_t dist_old;

  NodeData() : dist_current(0), dist_old(0) {}
  NodeData(uint32_t cur_dist, uint32_t old_dist)
      : dist_current(cur_dist), dist_old(old_dist) {}

  // Copy constructor
  NodeData(const NodeData& other)
      : dist_current(other.dist_current.load()), dist_old(other.dist_old) {}

  // Copy assignment operator
  NodeData& operator=(const NodeData& other) {
    dist_current.store(other.dist_current.load());
    dist_old = other.dist_old;
    return *this;
  }
};

galois::DynamicBitSet bitset_dist_current;

#include "bfs_pull_sync.hh"

uint64_t src_node      = 0;
uint64_t maxIterations = 1000;

typedef galois::graphs::DistLocalGraph<NodeData, int> Graph;
typedef galois::graphs::WMDGraph<galois::graphs::ELVertex, galois::graphs::ELEdge, NodeData, int, OECPolicy> ELGraph;
typedef typename Graph::GraphNode GNode;
std::unique_ptr<galois::graphs::GluonSubstrate<Graph>> syncSubstrate;

struct InitializeGraph {
  const uint32_t& local_infinity;
  uint64_t& local_src_node;
  Graph* graph;

  InitializeGraph(uint64_t& _src_node, const uint32_t& _infinity, Graph* _graph)
      : local_infinity(_infinity), local_src_node(_src_node), graph(_graph) {}

  void static go(Graph& _graph) {
    const auto& allNodes = _graph.allNodesRange();
    galois::do_all(
        galois::iterate(allNodes), InitializeGraph(src_node, infinity, &_graph),
        galois::no_stats(),
        galois::loopname(
            syncSubstrate->get_run_identifier("InitializeGraph").c_str()));
  }

  void operator()(GNode src) const {
    NodeData& sdata = graph->getData(src);
    sdata.dist_current =
        (graph->getGID(src) == local_src_node) ? 0 : local_infinity;
    sdata.dist_old =
        (graph->getGID(src) == local_src_node) ? 0 : local_infinity;
  }
};

template <bool async>
struct FirstItr_BFS {
  Graph* graph;

  FirstItr_BFS(Graph* _graph) : graph(_graph) {}

  void static go(Graph& _graph) {
    uint32_t __begin, __end;
    if (_graph.isLocal(src_node)) {
      __begin = _graph.getLID(src_node);
      __end   = __begin + 1;
    } else {
      __begin = 0;
      __end   = 0;
    }
    syncSubstrate->set_num_round(0);
    galois::do_all(
        galois::iterate(__begin, __end), FirstItr_BFS{&_graph},
        galois::no_stats(),
        galois::loopname(syncSubstrate->get_run_identifier("BFS").c_str()));

    syncSubstrate->sync<writeDestination, readSource, Reduce_min_dist_current,
                        Bitset_dist_current, async>("BFS");

    galois::runtime::reportStat_Tsum(
        "BFS", syncSubstrate->get_run_identifier("NumWorkItems"),
        __end - __begin);
  }

  void operator()(GNode src) const {
    NodeData& snode = graph->getData(src);
    snode.dist_old  = snode.dist_current;

    for (auto jj : graph->edges(src)) {
      GNode dst         = graph->getEdgeDst(jj);
      auto& dnode       = graph->getData(dst);
      uint32_t new_dist = 1 + snode.dist_current;
      uint32_t old_dist = galois::atomicMin(dnode.dist_current, new_dist);
      if (old_dist > new_dist) {
        bitset_dist_current.set(dst);
      }
    }
  }
};

template <bool async>
struct BFS {
  uint32_t local_priority;
  Graph* graph;
  using DGTerminatorDetector =
      typename std::conditional<async, galois::DGTerminator<unsigned int>,
                                galois::DGAccumulator<unsigned int>>::type;
  using DGAccumulatorTy = galois::DGAccumulator<unsigned int>;

  DGTerminatorDetector& active_vertices;
  DGAccumulatorTy& work_edges;

  BFS(uint32_t _local_priority, Graph* _graph, DGTerminatorDetector& _dga,
      DGAccumulatorTy& _work_edges)
      : local_priority(_local_priority), graph(_graph), active_vertices(_dga),
        work_edges(_work_edges) {}

  void static go(Graph& _graph) {
    FirstItr_BFS<async>::go(_graph);

    unsigned _num_iterations = 1;

    const auto& nodesWithEdges = _graph.allNodesRange();

    uint32_t priority = std::numeric_limits<uint32_t>::max();
    DGTerminatorDetector dga;
    DGAccumulatorTy work_edges;

    do {

      syncSubstrate->set_num_round(_num_iterations);
      dga.reset();
      work_edges.reset();
      galois::do_all(
          galois::iterate(nodesWithEdges),
          BFS(priority, &_graph, dga, work_edges), galois::steal(),
          galois::no_stats(),
          galois::loopname(syncSubstrate->get_run_identifier("BFS").c_str()));
          galois::runtime::getHostBarrier().wait();
      syncSubstrate->sync<writeDestination, readSource, Reduce_min_dist_current,
                          Bitset_dist_current, async>("BFS");

      galois::runtime::reportStat_Tsum(
          "BFS", syncSubstrate->get_run_identifier("NumWorkItems"),
          (unsigned long)work_edges.read_local());

      ++_num_iterations;
    } while ((async || (_num_iterations < maxIterations)) &&
             dga.reduce(syncSubstrate->get_run_identifier()));
    galois::runtime::reportStat_Tmax(
        "BFS", "NumIterations_" + std::to_string(syncSubstrate->get_run_num()),
        (unsigned long)_num_iterations);
  }

  void operator()(GNode src) const {
    NodeData& snode = graph->getData(src);
    auto& net = galois::runtime::getSystemNetworkInterface();
    std::cout << "src: " << graph->getGID(src) << " dist_old " << snode.dist_old << " dist_current " << snode.dist_current << " host " << net.ID << std::endl;

    if (snode.dist_old > snode.dist_current) {
      active_vertices += 1;

      if (local_priority > snode.dist_current) {
        snode.dist_old = snode.dist_current;

        for (auto jj : graph->edges(src)) {
          work_edges += 1;

          GNode dst         = graph->getEdgeDst(jj);
          auto& dnode       = graph->getData(dst);
          uint32_t new_dist = 1 + snode.dist_current;
          uint32_t old_dist = galois::atomicMin(dnode.dist_current, new_dist);
          if (old_dist > new_dist)
            bitset_dist_current.set(dst);
        }
      }
    }
  }
};

/******************************************************************************/
/* Sanity check operators */
/******************************************************************************/

/* Prints total number of nodes visited + max distance */
struct BFSSanityCheck {
  const uint32_t& local_infinity;
  Graph* graph;

  galois::DGAccumulator<uint64_t>& DGAccumulator_sum;
  galois::DGReduceMax<uint32_t>& DGMax;

  BFSSanityCheck(const uint32_t& _infinity, Graph* _graph,
                 galois::DGAccumulator<uint64_t>& dgas,
                 galois::DGReduceMax<uint32_t>& dgm)
      : local_infinity(_infinity), graph(_graph), DGAccumulator_sum(dgas),
        DGMax(dgm) {}

  void static go(Graph& _graph, galois::DGAccumulator<uint64_t>& dgas,
                 galois::DGReduceMax<uint32_t>& dgm) {
    dgas.reset();
    dgm.reset();

    galois::do_all(galois::iterate(_graph.masterNodesRange().begin(),
                                   _graph.masterNodesRange().end()),
                   BFSSanityCheck(infinity, &_graph, dgas, dgm),
                   galois::no_stats(), galois::loopname("BFSSanityCheck"));

    uint64_t num_visited  = dgas.reduce();
    uint32_t max_distance = dgm.reduce();

    // Only host 0 will print the info
    if (galois::runtime::getSystemNetworkInterface().ID == 0) {
      galois::gPrint("Number of nodes visited from source ", src_node, " is ",
                     num_visited, "\n");
      galois::gPrint("Max distance from source ", src_node, " is ",
                     max_distance, "\n");
    }
  }

  void operator()(GNode src) const {
    NodeData& src_data = graph->getData(src);

    if (src_data.dist_current < local_infinity) {
      DGAccumulator_sum += 1;
      DGMax.update(src_data.dist_current);
    }
  }
};

/******************************************************************************/
/* Make results */
/******************************************************************************/

std::vector<uint32_t> makeResultsCPU(std::unique_ptr<Graph>& hg) {
  std::vector<uint32_t> values;

  values.reserve(hg->numMasters());
  for (auto node : hg->masterNodesRange()) {
    values.push_back(hg->getData(node).dist_current);
  }

  return values;
}

void printUnorderedMap (std::unordered_map<uint64_t, std::vector<uint64_t>> &edits, uint64_t id) {
  for (const auto &pair : edits) {
    std::cout << " Printing for host " << id << " src " << pair.first << " ";
    for (auto dst : pair.second) {
      std::cout << dst << " ";
    }
    std::cout << std::endl;
  }
}

void CheckGraph (std::unique_ptr<Graph> &hg, std::unordered_map<uint64_t, std::vector<uint64_t>> &mp) {
  galois::do_all(
    galois::iterate(hg->allNodesRange()),
    [&](size_t lid) {
      auto token = hg->getGID(lid);
      std::vector<uint64_t> edgeDst;
      auto end = hg->edge_end(lid);
      auto itr = hg->edge_begin(lid);
      std::cout << "token: " << token << "\n";
      for (; itr != end; itr++) {
        edgeDst.push_back(hg->getGID(hg->getEdgeDst(itr)));
      }
      std::vector<uint64_t> edgeDstDbg;
      for (auto& e : hg->edges(lid)) {
        edgeDstDbg.push_back(hg->getGID(hg->getEdgeDst(e)));
      }
      assert(edgeDst == edgeDstDbg);
      std::sort(edgeDst.begin(), edgeDst.end());
      // std::cout << token << " ";
      for (auto edge : edgeDst) {
        // std::cout << edge << " ";
        mp[token].push_back(edge);
      }
      // std::cout << std::endl;
    },
    galois::steal());
}

void PrintMasterMirrorNodes (Graph &hg, uint64_t id) {
  std::cout << "Master nodes on host " << id <<std::endl;
  for (auto node : hg.masterNodesRange()) {
    std::cout << hg.getGID(node) << " ";
  }
  std::cout << std::endl;
  std::cout << "Mirror nodes on host " << id <<std::endl;
  auto mirrors = hg.getMirrorNodes();
  for (auto vec : mirrors) {
    for (auto node : vec) {
      std::cout << node << " ";
    }
  }
  std::cout << std::endl;
}

const char* elGetOne(const char* line, std::uint64_t& val) {
  bool found = false;
  val        = 0;
  char c;
  while ((c = *line++) != '\0' && isspace(c)) {
  }
  do {
    if (isdigit(c)) {
      found = true;
      val *= 10;
      val += (c - '0');
    } else if (c == '_') {
      continue;
    } else {
      break;
    }
  } while ((c = *line++) != '\0' && !isspace(c));
  if (!found)
    val = UINT64_MAX;
  return line;
}

void parser(const char* line, Graph &hg, std::vector<std::vector<uint64_t>> &delta_mirrors) {
  uint64_t src, dst;
  line = elGetOne(line, src);
  line = elGetOne(line, dst);
  std::cout << "src: " << src << " dst: " << dst << " isowned " << hg.isOwned(src) << " " << hg.isOwned(dst) << "\n";
  if((hg.isOwned(src)) && (!hg.isLocal(dst))) {
    uint32_t h = hg.getHostID(dst);
    delta_mirrors[h].push_back(dst);
  }

}

std::vector<std::vector<uint64_t>> genMirrorNodes(Graph &hg, std::string filename, int batch) {

  auto& net = galois::runtime::getSystemNetworkInterface();
  std::vector<std::vector<uint64_t>> delta_mirrors(net.Num);

  for(uint32_t i=0; i<net.Num; i++) {
    std::string dynamicFile = filename + "_batch" + std::to_string(batch) + "_host" + std::to_string(i) + ".el";
    std::ifstream file(dynamicFile);
    std::string line;
    while (std::getline(file, line)) {
      parser(line.c_str(), hg, delta_mirrors);
    }
  }
  return delta_mirrors;
}

int main(int argc, char* argv[]) {

  std::string filename = argv[1];
  if (argc > 2) {
    src_node = std::stoul(argv[2]);
  }
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <filename> <src_node> <numVertices>\n";
    return 1;
  }

  uint64_t numVertices = std::stoul(argv[3]);
  galois::DistMemSys G;

  std::unique_ptr<Graph> hg;

  hg            = distLocalGraphInitialization<galois::graphs::ELVertex,
                                    galois::graphs::ELEdge, NodeData, int,
                                    OECPolicy>(filename, numVertices);
  syncSubstrate = gluonInitialization<NodeData, int>(hg);

  std::unordered_map<uint64_t, std::vector<uint64_t>> edits;
  CheckGraph(hg, edits);
  //printUnorderedMap(edits, galois::runtime::getSystemNetworkInterface().ID)  ;


  galois::runtime::getHostBarrier().wait();

  int num_batches = 2;
  ELGraph* wg = dynamic_cast<ELGraph*>(hg.get());

  auto& net = galois::runtime::getSystemNetworkInterface();
  for (int i=0; i<num_batches; i++) {
    //PrintMasterMirrorNodes(*hg, net.ID);

    std::vector<std::string> edit_files;
    std::string dynFile = "edits";
    std::string dynamicFile = dynFile + "_batch" + std::to_string(i) + "_host" + std::to_string(net.ID) + ".el";
    edit_files.emplace_back(dynamicFile);
    //IMPORTANT: CAll genMirrorNodes before creating the graphUpdateManager!!!!!!!!
    std::vector<std::vector<uint64_t>> delta_mirrors = genMirrorNodes(*hg, dynFile, i);
    graphUpdateManager<galois::graphs::ELVertex,
                                      galois::graphs::ELEdge, NodeData, int, OECPolicy> GUM(std::make_unique<galois::graphs::ELParser<galois::graphs::ELVertex,
                                      galois::graphs::ELEdge>> (1, edit_files), 100, wg);
    GUM.start();
    while (!GUM.stop()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(GUM.getPeriod()));
    }
    galois::runtime::getHostBarrier().wait();
    GUM.stop2();


    syncSubstrate->addDeltaMirrors(delta_mirrors);
    PrintMasterMirrorNodes(*hg, net.ID);
    galois::runtime::getHostBarrier().wait();
    
    bitset_dist_current.resize(hg->size());
    galois::DGAccumulator<uint64_t> DGAccumulator_sum;
    galois::DGReduceMax<uint32_t> m;
    InitializeGraph::go(*hg);
    galois::runtime::getHostBarrier().wait();
    syncSubstrate->printMirrors();
    {
      DIST_BENCHMARK_SCOPE("bfs-push", galois::runtime::getSystemNetworkInterface().ID);
        std::string timer_str("Timer_" + std::to_string(i));
        galois::StatTimer StatTimer_main(timer_str.c_str(), "BFS");

        StatTimer_main.start();
        BFS<false>::go(*hg);
        StatTimer_main.stop();

        // sanity check
        BFSSanityCheck::go(*hg, DGAccumulator_sum, m);

        if ((i + 1) != num_batches) {
          bitset_dist_current.reset();
          InitializeGraph::go((*hg));
          (*syncSubstrate).set_num_run(i + 1);
          galois::runtime::getHostBarrier().wait();
        }
    }

  }
  
  return 0;
}
