#include <iostream>
#include "../include/importer.cpp"
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

static const float alpha = (1.0 - 0.85);
static const float tolerance = 1e-5;
struct NodeData {
  float value;
  std::atomic<uint32_t> nout;
  float delta;
  std::atomic<float> residual;
  NodeData() : value(0), nout(0), delta(0), residual(0) {}
  NodeData(float v) : value(v), nout(0), delta(0), residual(0) {}

  //Copy constructor
    NodeData(const NodeData& other) {
        value = other.value;
        nout = other.nout.load();
        delta = other.delta;
        residual = other.residual.load();
    }
};

uint64_t maxIterations = 1000;

galois::DynamicBitSet bitset_residual;
galois::DynamicBitSet bitset_nout;

typedef galois::graphs::DistLocalGraph<NodeData, void> Graph;
typedef galois::graphs::WMDGraph<galois::graphs::ELVertex, galois::graphs::ELEdge, NodeData, int, OECPolicy> ELGraph;
typedef typename Graph::GraphNode GNode;
typedef GNode WorkItem;

std::unique_ptr<galois::graphs::GluonSubstrate<Graph>> syncSubstrate;

#include "pagerank_push_sync.hh"

struct ResetGraph {
  Graph* graph;

  ResetGraph(Graph* _graph) : graph(_graph) {}
  void static go(Graph& _graph) {
    const auto& allNodes = _graph.allNodesRange();
      galois::do_all(
          galois::iterate(allNodes.begin(), allNodes.end()),
          ResetGraph{&_graph}, galois::no_stats(),
          galois::loopname(
              syncSubstrate->get_run_identifier("ResetGraph").c_str()));
  }

  void operator()(GNode src) const {
    NodeData& sdata = graph->getData(src);
    sdata.value     = 0;
    sdata.nout      = 0;
    sdata.residual  = 0;
    sdata.delta     = 0;
  }
};

// Initialize residual at nodes with outgoing edges + find nout for
// nodes with outgoing edges
struct InitializeGraph {
  const float& local_alpha;
  Graph* graph;

  InitializeGraph(const float& _alpha, Graph* _graph)
      : local_alpha(_alpha), graph(_graph) {}

  void static go(Graph& _graph) {
    // first initialize all fields to 0 via ResetGraph (can't assume all zero
    // at start)
    ResetGraph::go(_graph);

    const auto& nodesWithEdges = _graph.allNodesRange();

      // regular do all without stealing; just initialization of nodes with
      // outgoing edges
    galois::do_all(
        galois::iterate(nodesWithEdges.begin(), nodesWithEdges.end()),
        InitializeGraph{alpha, &_graph}, galois::steal(), galois::no_stats(),
        galois::loopname(
            syncSubstrate->get_run_identifier("InitializeGraph").c_str()));

    syncSubstrate->sync<writeSource, readSource, Reduce_add_nout, Bitset_nout>(
        "InitializeGraphNout");
  }

  void operator()(GNode src) const {
    NodeData& sdata = graph->getData(src);
    sdata.residual  = local_alpha;
    uint32_t num_edges =
        std::distance(graph->edge_begin(src), graph->edge_end(src));
    galois::atomicAdd(sdata.nout, num_edges);
    bitset_nout.set(src);
  }
};

struct PageRank_delta {
  const float& local_alpha;
  const float& local_tolerance;
  Graph* graph;

  PageRank_delta(const float& _local_alpha, const float& _local_tolerance,
                 Graph* _graph)
      : local_alpha(_local_alpha), local_tolerance(_local_tolerance),
        graph(_graph) {}

  void static go(Graph& _graph) {
    const auto& nodesWithEdges = _graph.allNodesRange();

    galois::do_all(
        galois::iterate(nodesWithEdges.begin(), nodesWithEdges.end()),
        PageRank_delta{alpha, tolerance, &_graph}, galois::no_stats(),
        galois::loopname(
            syncSubstrate->get_run_identifier("PageRank_delta").c_str()));
  }

  void operator()(WorkItem src) const {
    NodeData& sdata = graph->getData(src);

    if (sdata.residual > 0) {
      float residual_old = sdata.residual;
      sdata.residual     = 0;
      sdata.value += residual_old;
      if (residual_old > this->local_tolerance) {
        if (sdata.nout > 0) {
          sdata.delta = residual_old * (1 - local_alpha) / sdata.nout;
        }
      }
    }
  }
};

template <bool async>
struct PageRank {
  Graph* graph;
  using DGTerminatorDetector =
      typename std::conditional<async, galois::DGTerminator<unsigned int>,
                                galois::DGAccumulator<unsigned int>>::type;

  DGTerminatorDetector& active_vertices;

  PageRank(Graph* _g, DGTerminatorDetector& _dga)
      : graph(_g), active_vertices(_dga) {}

  void static go(Graph& _graph) {
    unsigned _num_iterations   = 0;
    const auto& nodesWithEdges = _graph.allNodesRange();
    DGTerminatorDetector dga;

    do {
      syncSubstrate->set_num_round(_num_iterations);
      PageRank_delta::go(_graph);
      dga.reset();
      // reset residual on mirrors
      syncSubstrate->reset_mirrorField<Reduce_add_residual>();

      galois::do_all(
          galois::iterate(nodesWithEdges), PageRank{&_graph, dga},
          galois::no_stats(), galois::steal(),
          galois::loopname(
              syncSubstrate->get_run_identifier("PageRank").c_str()));

      syncSubstrate->sync<writeDestination, readSource, Reduce_add_residual,
                          Bitset_residual, async>("PageRank");

      galois::runtime::reportStat_Tsum(
          "PAGERANK", "NumWorkItems_" + (syncSubstrate->get_run_identifier()),
          (unsigned long)dga.read_local());

      ++_num_iterations;
    } while ((async || (_num_iterations < maxIterations)) &&
             dga.reduce(syncSubstrate->get_run_identifier()));

    if (galois::runtime::getSystemNetworkInterface().ID == 0) {
      galois::runtime::reportStat_Single(
          "PAGERANK",
          "NumIterations_" + std::to_string(syncSubstrate->get_run_num()),
          (unsigned long)_num_iterations);
    }
  }

  void operator()(WorkItem src) const {
    NodeData& sdata = graph->getData(src);
    if (sdata.delta > 0) {
      float _delta = sdata.delta;
      sdata.delta  = 0;

      active_vertices += 1; // this should be moved to Pagerank_delta operator

      for (auto nbr : graph->edges(src)) {
        GNode dst       = graph->getEdgeDst(nbr);
        NodeData& ddata = graph->getData(dst);

        galois::atomicAdd(ddata.residual, _delta);

        bitset_residual.set(dst);
      }
    }
  }
};

/******************************************************************************/
/* Sanity check operators */
/******************************************************************************/

// Gets various values from the pageranks values/residuals of the graph
struct PageRankSanity {
  const float& local_tolerance;
  Graph* graph;

  galois::DGAccumulator<float>& DGAccumulator_sum;
  galois::DGAccumulator<float>& DGAccumulator_sum_residual;
  galois::DGAccumulator<uint64_t>& DGAccumulator_residual_over_tolerance;

  galois::DGReduceMax<float>& max_value;
  galois::DGReduceMin<float>& min_value;
  galois::DGReduceMax<float>& max_residual;
  galois::DGReduceMin<float>& min_residual;

  PageRankSanity(
      const float& _local_tolerance, Graph* _graph,
      galois::DGAccumulator<float>& _DGAccumulator_sum,
      galois::DGAccumulator<float>& _DGAccumulator_sum_residual,
      galois::DGAccumulator<uint64_t>& _DGAccumulator_residual_over_tolerance,
      galois::DGReduceMax<float>& _max_value,
      galois::DGReduceMin<float>& _min_value,
      galois::DGReduceMax<float>& _max_residual,
      galois::DGReduceMin<float>& _min_residual)
      : local_tolerance(_local_tolerance), graph(_graph),
        DGAccumulator_sum(_DGAccumulator_sum),
        DGAccumulator_sum_residual(_DGAccumulator_sum_residual),
        DGAccumulator_residual_over_tolerance(
            _DGAccumulator_residual_over_tolerance),
        max_value(_max_value), min_value(_min_value),
        max_residual(_max_residual), min_residual(_min_residual) {}

  void static go(Graph& _graph, galois::DGAccumulator<float>& DGA_sum,
                 galois::DGAccumulator<float>& DGA_sum_residual,
                 galois::DGAccumulator<uint64_t>& DGA_residual_over_tolerance,
                 galois::DGReduceMax<float>& max_value,
                 galois::DGReduceMin<float>& min_value,
                 galois::DGReduceMax<float>& max_residual,
                 galois::DGReduceMin<float>& min_residual) {
    DGA_sum.reset();
    DGA_sum_residual.reset();
    max_value.reset();
    max_residual.reset();
    min_value.reset();
    min_residual.reset();
    DGA_residual_over_tolerance.reset();

    galois::do_all(galois::iterate(_graph.masterNodesRange().begin(),
                                   _graph.masterNodesRange().end()),
                   PageRankSanity(tolerance, &_graph, DGA_sum,
                                  DGA_sum_residual,
                                  DGA_residual_over_tolerance, max_value,
                                  min_value, max_residual, min_residual),
                   galois::no_stats(), galois::loopname("PageRankSanity"));

    float max_rank          = max_value.reduce();
    float min_rank          = min_value.reduce();
    float rank_sum          = DGA_sum.reduce();
    float residual_sum      = DGA_sum_residual.reduce();
    uint64_t over_tolerance = DGA_residual_over_tolerance.reduce();
    float max_res           = max_residual.reduce();
    float min_res           = min_residual.reduce();

    // Only node 0 will print data
    if (galois::runtime::getSystemNetworkInterface().ID == 0) {
      galois::gPrint("Max rank is ", max_rank, "\n");
      galois::gPrint("Min rank is ", min_rank, "\n");
      galois::gPrint("Rank sum is ", rank_sum, "\n");
      galois::gPrint("Residual sum is ", residual_sum, "\n");
      galois::gPrint("# nodes with residual over ", tolerance,
                     " (tolerance) is ", over_tolerance, "\n");
      galois::gPrint("Max residual is ", max_res, "\n");
      galois::gPrint("Min residual is ", min_res, "\n");
    }
  }

  /* Gets the max, min rank from all owned nodes and
   * also the sum of ranks */
  void operator()(GNode src) const {
    NodeData& sdata = graph->getData(src);

    max_value.update(sdata.value);
    min_value.update(sdata.value);
    max_residual.update(sdata.residual);
    min_residual.update(sdata.residual);

    DGAccumulator_sum += sdata.value;
    DGAccumulator_sum_residual += sdata.residual;

    if (sdata.residual > local_tolerance) {
      DGAccumulator_residual_over_tolerance += 1;
    }
  }
};

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

int main(int argc, char* argv[]) {

  if (argc < 5) {
    std::cerr << "Usage: " << argv[0]
              << " <filename> <numVertices> <numBatches> <maxEditsInBatch>\n";
    return 1;
  }

  std::string filename = argv[1];
  uint64_t numVertices = std::stoul(argv[2]);
  uint64_t num_batches = std::stoul(argv[3]);
  uint64_t max_edits_in_batch = std::stoul(argv[4]);

  galois::DistMemSys G;

  std::unique_ptr<Graph> hg;

  hg = distLocalGraphInitialization<galois::graphs::ELVertex,
                                    galois::graphs::ELEdge, NodeData, void,
                                    OECPolicy>(filename, numVertices);
  syncSubstrate = gluonInitialization<NodeData, void>(hg);

  galois::runtime::getHostBarrier().wait();

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
    GUM.setBatchSize(max_edits_in_batch);
    GUM.start();
    while (!GUM.stop()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(GUM.getPeriod()));
    }
    galois::runtime::getHostBarrier().wait();
    GUM.stop2();

    syncSubstrate->addDeltaMirrors(delta_mirrors);
    PrintMasterMirrorNodes(*hg, net.ID);
    galois::runtime::getHostBarrier().wait();

    bitset_residual.resize(hg->size());
    bitset_nout.resize(hg->size());

    InitializeGraph::go((*hg));
    galois::runtime::getHostBarrier().wait();

    galois::DGAccumulator<float> DGA_sum;
    galois::DGAccumulator<float> DGA_sum_residual;
    galois::DGAccumulator<uint64_t> DGA_residual_over_tolerance;
    galois::DGReduceMax<float> max_value;
    galois::DGReduceMin<float> min_value;
    galois::DGReduceMax<float> max_residual;
    galois::DGReduceMin<float> min_residual;

    for (auto run = 0; run < 1; ++run) {
      PageRank<false>::go(*hg);

      // sanity check
      PageRankSanity::go(*hg, DGA_sum, DGA_sum_residual,
                        DGA_residual_over_tolerance, max_value, min_value,
                        max_residual, min_residual);

      if ((run + 1) != 1) {
        bitset_residual.reset();
        bitset_nout.reset();

        (*syncSubstrate).set_num_run(run + 1);
        InitializeGraph::go(*hg);
        galois::runtime::getHostBarrier().wait();
      }
    }
  }

  return 0;

}