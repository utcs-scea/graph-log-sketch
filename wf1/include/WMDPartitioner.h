/**
 * @file WMDPartitioner.h
 *
 * Graph partitioning that duplicates edges for WMD dataset. Currently only supports an
 * outgoing edge cut.
 *
 */

#ifndef _WMD_PARTITIONER_H
#define _WMD_PARTITIONER_H

#include "galois/Galois.h"
#include "galois/graphs/DistributedGraph.h"
#include "galois/DReducible.h"

#include "WMDGraph.h"

#include <atomic>
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>

#ifdef GRAPH_PROFILE
#include <filesystem>
#endif

namespace galois {
namespace graphs {
/**
 * @tparam NodeTy type of node data for the graph
 * @tparam EdgeTy type of edge data for the graph
 *
 * @todo fully document and clean up code
 * @warning not meant for public use + not fully documented yet
 */
template <typename NodeTy, typename EdgeTy, typename Partitioner>
class WMDGraph : public DistGraph<NodeTy, EdgeTy> {
  #ifdef GRAPH_PROFILE
  std::atomic<std::uint64_t> remote_file_read_size=0;
  std::atomic<std::uint64_t> local_file_read_size=0;
  std::atomic<std::uint64_t> local_seq_write_size=0;
  std::atomic<std::uint64_t> local_rand_write_size=0;
  std::atomic<std::uint64_t> local_seq_read_size=0;
  std::atomic<std::uint64_t> local_rand_read_size=0;
  std::atomic<std::uint64_t> local_seq_write_count=0;
  std::atomic<std::uint64_t> local_rand_write_count=0;
  std::atomic<std::uint64_t> local_seq_read_count=0;
  std::atomic<std::uint64_t> local_rand_read_count=0;
  std::vector<std::unique_ptr<std::atomic<uint64_t>>> remote_seq_read_size;
  std::vector<std::unique_ptr<std::atomic<uint64_t>>> remote_rand_read_size;
  std::vector<std::unique_ptr<std::atomic<uint64_t>>> remote_seq_read_count;
  std::vector<std::unique_ptr<std::atomic<uint64_t>>> remote_rand_read_count;
  std::vector<std::unique_ptr<std::atomic<uint64_t>>> remote_rand_rmw_size;
  std::vector<std::unique_ptr<std::atomic<uint64_t>>> remote_rand_rmw_count;  // reduction is a read-modify-write on dst
  #endif

  //! size used to buffer edge sends during partitioning
  constexpr static unsigned edgePartitionSendBufSize = 8388608;
  constexpr static const char* const GRNAME          = "dGraph_WMD";
  std::unique_ptr<Partitioner> graphPartitioner;

  uint32_t G2LEdgeCut(uint64_t gid, uint32_t globalOffset) const {
    assert(base_DistGraph::isLocal(gid));
    // optimized for edge cuts
    if (gid >= globalOffset && gid < globalOffset + base_DistGraph::numOwned)
      return gid - globalOffset;

    return base_DistGraph::globalToLocalMap.at(gid);
  }

  /**
   * Free memory of a vector by swapping an empty vector with it
   */
  template <typename V>
  void freeVector(V& vectorToKill) {
    V dummyVector;
    vectorToKill.swap(dummyVector);
  }

  uint32_t nodesToReceive;

  uint64_t myKeptEdges;
  uint64_t myReadEdges;
  uint64_t globalKeptEdges;
  uint64_t totalEdgeProxies;

  std::vector<std::vector<size_t>> mirrorEdges;
  std::unordered_map<uint64_t, uint64_t> localEdgeGIDToLID;

  virtual unsigned getHostIDImpl(uint64_t gid) const {
    assert(gid < base_DistGraph::numGlobalNodes);
    return graphPartitioner->retrieveMaster(gid);
  }

  virtual bool isOwnedImpl(uint64_t gid) const {
    assert(gid < base_DistGraph::numGlobalNodes);
    return (graphPartitioner->retrieveMaster(gid) == base_DistGraph::id);
  }

  virtual bool isLocalImpl(uint64_t gid) const {
    assert(gid < base_DistGraph::numGlobalNodes);
    return (base_DistGraph::globalToLocalMap.find(gid) !=
            base_DistGraph::globalToLocalMap.end());
  }

  virtual bool isVertexCutImpl() const { return false; }

public:
  //! typedef for base DistGraph class
  using base_DistGraph = DistGraph<NodeTy, EdgeTy>;

  /**
   * Returns edges owned by this graph (i.e. read).
   */
  uint64_t numOwnedEdges() const { return myKeptEdges; }

  /**
   * Returns # edges kept in all graphs.
   */
  uint64_t globalEdges() const { return globalKeptEdges; }

  std::vector<std::vector<size_t>>& getMirrorEdges() { return mirrorEdges; }

  /**
   * Return the reader of a particular node.
   * @param gid GID of node to get reader of
   * @return Host reader of node passed in as param
   */
  unsigned getHostReader(uint64_t gid) const {
    for (auto i = 0U; i < base_DistGraph::numHosts; ++i) {
      uint64_t start, end;
      std::tie(start, end) = base_DistGraph::gid2host[i];
      if (gid >= start && gid < end) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Constructor
   */
  WMDGraph(
      const std::string& filename, unsigned host, unsigned _numHosts,
      bool setupGluon = true, bool doSort = false,
      galois::graphs::MASTERS_DISTRIBUTION md = BALANCED_EDGES_OF_MASTERS,
      uint32_t nodeWeight = 0, uint32_t edgeWeight = 0)
      : base_DistGraph(host, _numHosts) {
    galois::gInfo("[", base_DistGraph::id, "] Start DistGraph construction.");
    galois::runtime::reportParam(GRNAME, "WMDGraph", "0");
    galois::StatTimer Tgraph_construct(
        "GraphPartitioningTime", GRNAME);
    Tgraph_construct.start();

    ////////////////////////////////////////////////////////////////////////////
    galois::gInfo("[", base_DistGraph::id, "] Start reading graph.");
    galois::StatTimer graphReadTimer("GraphReading", GRNAME);
    graphReadTimer.start();

    galois::gDebug("[", base_DistGraph::id, "] WMDOfflineGraph End!");
    galois::graphs::WMDOfflineGraph<NodeTy, EdgeTy> g(filename, md);
    galois::gDebug("[", base_DistGraph::id, "] WMDOfflineGraph End!");
    base_DistGraph::numGlobalNodes = g.size();
    base_DistGraph::numGlobalEdges = g.sizeEdges();
    std::vector<unsigned> dummy;

    galois::gDebug("[", base_DistGraph::id, "] node size: ", base_DistGraph::numGlobalNodes, ", edge size: ", base_DistGraph::numGlobalEdges);

    galois::gDebug("[", base_DistGraph::id, "] computeMasters Begin!");
    // not actually getting masters, but getting assigned readers for nodes
    base_DistGraph::computeMasters(md, g, dummy, nodeWeight, edgeWeight);
    galois::gDebug("[", base_DistGraph::id, "] computeMasters End!");

    // freeup memory that won't be used in the future
    g.clearEdgePrefixSumInfo();

    std::vector<uint64_t> ndegrees;

    graphPartitioner = std::make_unique<Partitioner>(
        host, _numHosts, base_DistGraph::numGlobalNodes,
        base_DistGraph::numGlobalEdges, ndegrees);
    graphPartitioner->saveGIDToHost(base_DistGraph::gid2host);

    graphReadTimer.stop();
    galois::gInfo("[", base_DistGraph::id, "] Reading graph complete in ", graphReadTimer.get_usec() / 1000000.0, " sec.");
    ////////////////////////////////////////////////////////////////////////////
    galois::gInfo("[", base_DistGraph::id, "] Start exchanging edges.");
    galois::StatTimer edgesExchangeTimer("EdgesExchange", GRNAME);
    edgesExchangeTimer.start();

    uint64_t nodeBegin = base_DistGraph::gid2host[base_DistGraph::id].first;
    uint64_t nodeEnd = base_DistGraph::gid2host[base_DistGraph::id].second;

    galois::gDebug("[", base_DistGraph::id, "] nodeBegin: ", nodeBegin, ", nodeEnd: ", nodeEnd);

    // never read edge data from disk
    galois::graphs::WMDBufferedGraph<NodeTy, EdgeTy> bufGraph;
    bufGraph.loadPartialGraph(g, nodeBegin, nodeEnd, 
                              base_DistGraph::numGlobalNodes,
                              base_DistGraph::numGlobalEdges);

    edgesExchangeTimer.stop();
    galois::gInfo("[", base_DistGraph::id, "] Exchanging edges complete in ", edgesExchangeTimer.get_usec() / 1000000.0, " sec.");
    ////////////////////////////////////////////////////////////////////////////
    galois::gInfo("[", base_DistGraph::id, "] Starting edge inspection.");
    galois::StatTimer inspectionTimer("EdgeInspection", GRNAME);
    inspectionTimer.start();
  
    // galois::gstl::Vector<uint64_t> prefixSumOfEdges;
    base_DistGraph::numOwned = nodeEnd - nodeBegin;
    // prefixSumOfEdges.resize(base_DistGraph::numOwned);

    // initial pass; set up lid-gid mappings, determine which proxies exist on
    // this host
    galois::DynamicBitSet presentProxies =
        edgeInspectionRound1(bufGraph);
    // set my read nodes on present proxies
    // TODO parallel?
    for (uint64_t i = nodeBegin; i < nodeEnd; i++) {
      presentProxies.set(i);
    }

    // vector to store bitsets received from other hosts
    std::vector<galois::DynamicBitSet> proxiesOnOtherHosts;
    proxiesOnOtherHosts.resize(_numHosts);

    // send off mirror proxies that exist on this host to other hosts
    communicateProxyInfo(presentProxies, proxiesOnOtherHosts);
    // put them together to save memory
    proxiesOnOtherHosts[base_DistGraph::id] = std::move(presentProxies);

    base_DistGraph::numEdges = bufGraph.sizeLocalEdges();

    inspectionTimer.stop();
    galois::gInfo("[", base_DistGraph::id, "] Edge inspection complete in ", inspectionTimer.get_usec() / 1000000.0, " sec.");
    ////////////////////////////////////////////////////////////////////////////
    galois::gInfo("[", base_DistGraph::id, "] Starting building LS_CSR.");
    galois::StatTimer buildingTimer("GraphBuilding", GRNAME);
    buildingTimer.start();

    // Graph construction related calls
    base_DistGraph::beginMaster = 0;
    // Allocate and construct the graph
    base_DistGraph::graph.allocateFrom(base_DistGraph::numNodes,
                                       base_DistGraph::numEdges);
    base_DistGraph::graph.constructNodes();

    // construct edges
    // not need to move edges from other host since all edges is already ready when no edge mirror are used.
    galois::gDebug("[", base_DistGraph::id, "] add edges into graph.");
    uint64_t offset = bufGraph.getNodeOffset();
    galois::do_all(galois::iterate(nodeBegin, nodeEnd),
    [&](uint64_t globalID)
    {
      auto edgeDst = bufGraph.edgeLocalDst(globalID, base_DistGraph::globalToLocalMap);
      auto edgeData = bufGraph.edgeDataPtr(globalID);
      base_DistGraph::graph.addEdgesUnSort(true, globalID - offset, edgeDst.data(), edgeData, bufGraph.edgeNum(globalID), false);
    }, galois::steal());

    // move node data (include mirror nodes) from other hosts to graph in this host
    galois::gDebug("[", base_DistGraph::id, "] add nodes data into graph.");
    bufGraph.gatherNodes(g, base_DistGraph::graph, proxiesOnOtherHosts, base_DistGraph::numNodes, base_DistGraph::globalToLocalMap);

    galois::gDebug("[", base_DistGraph::id, "] LS_CSR construction done.");
    galois::gInfo("[", base_DistGraph::id, "] LS_CSR graph local nodes: ", base_DistGraph::numNodes);
    galois::gInfo("[", base_DistGraph::id, "] LS_CSR graph master nodes: ", base_DistGraph::numOwned);
    galois::gInfo("[", base_DistGraph::id, "] LS_CSR graph local edges: ", base_DistGraph::graph.sizeEdges());
    assert(base_DistGraph::graph.sizeEdges() == base_DistGraph::numEdges);
    assert(base_DistGraph::graph.size() == base_DistGraph::numNodes);

    bufGraph.resetAndFree();

    buildingTimer.stop();
    galois::gInfo("[", base_DistGraph::id, "] Building LS_CSR complete in ", buildingTimer.get_usec() / 1000000.0, " sec.");
    ////////////////////////////////////////////////////////////////////////////

    if (setupGluon) {
      galois::CondStatTimer<MORE_DIST_STATS> TfillMirrors("FillMirrors",
                                                          GRNAME);

      TfillMirrors.start();
      fillMirrors();
      TfillMirrors.stop();
    }

    ////////////////////////////////////////////////////////////////////////////

    // TODO this might be useful to keep around
    proxiesOnOtherHosts.clear();
    proxiesOnOtherHosts.shrink_to_fit();
    ndegrees.clear();
    ndegrees.shrink_to_fit();

    // SORT EDGES
    if (doSort) {
      base_DistGraph::sortEdgesByDestination();
    }

    ////////////////////////////////////////////////////////////////////////////

    galois::CondStatTimer<MORE_DIST_STATS> Tthread_ranges("ThreadRangesTime",
                                                          GRNAME);

    galois::gInfo("[", base_DistGraph::id, "] Determining thread ranges");

    Tthread_ranges.start();
    base_DistGraph::determineThreadRanges();
    base_DistGraph::determineThreadRangesMaster();
    base_DistGraph::determineThreadRangesWithEdges();
    base_DistGraph::initializeSpecificRanges();
    Tthread_ranges.stop();

    Tgraph_construct.stop();
    galois::gInfo("[", base_DistGraph::id, "] Total time of DistGraph construction is ", Tgraph_construct.get_usec() / 1000000.0, " sec.");

    galois::DGAccumulator<uint64_t> accumer;
    accumer.reset();
    accumer += base_DistGraph::sizeEdges();
    totalEdgeProxies = accumer.reduce();

    uint64_t totalNodeProxies;
    accumer.reset();
    accumer += base_DistGraph::size();
    totalNodeProxies = accumer.reduce();

    // report some statistics
    if (base_DistGraph::id == 0) {
      galois::runtime::reportStat_Single(
          GRNAME, std::string("TotalNodeProxies"), totalNodeProxies);
      galois::runtime::reportStat_Single(
          GRNAME, std::string("TotalEdgeProxies"), totalEdgeProxies);
      galois::runtime::reportStat_Single(GRNAME,
                                         std::string("OriginalNumberEdges"),
                                         base_DistGraph::globalSizeEdges());
      galois::runtime::reportStat_Single(GRNAME, std::string("TotalKeptEdges"),
                                         globalKeptEdges);
      galois::runtime::reportStat_Single(
          GRNAME, std::string("ReplicationFactorNodes"),
          (totalNodeProxies) / (double)base_DistGraph::globalSize());
      galois::runtime::reportStat_Single(
          GRNAME, std::string("ReplicatonFactorEdges"),
          (totalEdgeProxies) / (double)globalKeptEdges);
    }
  }

private:
  galois::DynamicBitSet
  edgeInspectionRound1(galois::graphs::WMDBufferedGraph<NodeTy, EdgeTy>& bufGraph) {
    galois::DynamicBitSet incomingMirrors;
    incomingMirrors.resize(base_DistGraph::numGlobalNodes);
    incomingMirrors.reset();

    uint32_t myID         = base_DistGraph::id;
    uint64_t globalOffset = base_DistGraph::gid2host[base_DistGraph::id].first;

    // already set before this is called
    base_DistGraph::localToGlobalVector.resize(base_DistGraph::numOwned);

    galois::DGAccumulator<uint64_t> keptEdges;
    keptEdges.reset();

    galois::GAccumulator<uint64_t> allEdges;
    allEdges.reset();

    auto& ltgv = base_DistGraph::localToGlobalVector;
    galois::do_all(
        galois::iterate(base_DistGraph::gid2host[base_DistGraph::id].first,
                        base_DistGraph::gid2host[base_DistGraph::id].second),
        [&](size_t n) {
          uint64_t edgeCount = 0;
          auto ii            = bufGraph.edgeBegin(n);
          auto ee            = bufGraph.edgeEnd(n);
          allEdges += std::distance(ii, ee);
          for (; ii < ee; ++ii) {
            uint32_t dst = bufGraph.edgeDestination(*ii);

            // we keep all edges in OEC so no need to do the check
            // if (graphPartitioner->keepEdge(n, dst))
            edgeCount++;
            keptEdges += 1;
            // which mirrors do I have
            if (graphPartitioner->retrieveMaster(dst) != myID) {
              incomingMirrors.set(dst);
            }       
          }
          // prefixSumOfEdges[n - globalOffset] = edgeCount;
          ltgv[n - globalOffset]             = n;
        },
#if MORE_DIST_STATS
        galois::loopname("EdgeInspectionLoop"),
#endif
        galois::steal(), galois::no_stats());

    myKeptEdges     = keptEdges.read_local();
    myReadEdges     = allEdges.reduce();
    globalKeptEdges = keptEdges.reduce();

    // get incoming mirrors ready for creation
    uint32_t additionalMirrorCount = incomingMirrors.count();
    base_DistGraph::localToGlobalVector.resize(
        base_DistGraph::localToGlobalVector.size() + additionalMirrorCount);

    // map creation: lid to gid
    if (additionalMirrorCount > 0) {
      uint32_t totalNumNodes = base_DistGraph::numGlobalNodes;
      uint32_t activeThreads = galois::getActiveThreads();
      std::vector<uint64_t> threadPrefixSums(activeThreads);
      galois::on_each([&](unsigned tid, unsigned nthreads) {
        size_t beginNode;
        size_t endNode;
        std::tie(beginNode, endNode) =
            galois::block_range(0u, totalNumNodes, tid, nthreads);
        uint64_t count = 0;
        for (size_t i = beginNode; i < endNode; i++) {
          if (incomingMirrors.test(i))
            ++count;
        }
        threadPrefixSums[tid] = count;
      });
      // get prefix sums
      for (unsigned int i = 1; i < threadPrefixSums.size(); i++) {
        threadPrefixSums[i] += threadPrefixSums[i - 1];
      }

      assert(threadPrefixSums.back() == additionalMirrorCount);

      uint32_t startingNodeIndex = base_DistGraph::numOwned;
      // do actual work, second on_each
      galois::on_each([&](unsigned tid, unsigned nthreads) {
        size_t beginNode;
        size_t endNode;
        std::tie(beginNode, endNode) =
            galois::block_range(0u, totalNumNodes, tid, nthreads);
        // start location to start adding things into prefix sums/vectors
        uint32_t threadStartLocation = 0;
        if (tid != 0) {
          threadStartLocation = threadPrefixSums[tid - 1];
        }
        uint32_t handledNodes = 0;
        for (size_t i = beginNode; i < endNode; i++) {
          if (incomingMirrors.test(i)) {
            base_DistGraph::localToGlobalVector[startingNodeIndex +
                                                threadStartLocation +
                                                handledNodes] = i;
            handledNodes++;
          }
        }
      });
    }

    base_DistGraph::numNodes = base_DistGraph::numOwned + additionalMirrorCount;
    base_DistGraph::numNodesWithEdges = base_DistGraph::numNodes;
    assert(base_DistGraph::localToGlobalVector.size() ==
           base_DistGraph::numNodes);

    // g2l mapping
    base_DistGraph::globalToLocalMap.reserve(base_DistGraph::numNodes);
    for (unsigned i = 0; i < base_DistGraph::numNodes; i++) {
      // global to local map construction
      base_DistGraph::globalToLocalMap[base_DistGraph::localToGlobalVector[i]] =
          i;
    }
    assert(base_DistGraph::globalToLocalMap.size() == base_DistGraph::numNodes);

    return incomingMirrors;
  }

  /**
   * Communicate to other hosts which proxies exist on this host.
   *
   * @param presentProxies Bitset marking which proxies are present on this host
   * @param proxiesOnOtherHosts Vector to deserialize received bitsets into
   */
  void communicateProxyInfo(
      galois::DynamicBitSet& presentProxies,
      std::vector<galois::DynamicBitSet>& proxiesOnOtherHosts) {
    auto& net = galois::runtime::getSystemNetworkInterface();
    // Send proxies on this host to other hosts
    for (unsigned h = 0; h < base_DistGraph::numHosts; ++h) {
      if (h != base_DistGraph::id) {
        galois::runtime::SendBuffer bitsetBuffer;
        galois::runtime::gSerialize(bitsetBuffer, presentProxies);
        net.sendTagged(h, galois::runtime::evilPhase, bitsetBuffer);
      }
    }

    // receive loop
    for (unsigned h = 0; h < net.Num - 1; h++) {
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);
      uint32_t sendingHost = p->first;
      // deserialize proxiesOnOtherHosts
      galois::runtime::gDeserialize(p->second,
                                    proxiesOnOtherHosts[sendingHost]);
    }

    base_DistGraph::increment_evilPhase();
  }
  #ifdef GRAPH_PROFILE
  void print_profile(std::ofstream &output) {
    int id = base_DistGraph::id;

    output << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:remote_file_read_size=" << remote_file_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_file_read_size=" << local_file_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_seq_write_size=" << local_seq_write_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_rand_write_size=" << local_rand_write_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_seq_read_size=" << local_seq_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_rand_read_size=" << local_rand_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_seq_write_count=" << local_seq_write_count << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_rand_write_count=" << local_rand_write_count << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_seq_read_count=" << local_seq_read_count << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:local_rand_read_count=" << local_rand_read_count << std::endl;

    for (int i = 0; i < base_DistGraph::numHosts; i++) {
      if (*remote_seq_read_count[i] != 0) {
        output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:remote_seq_read_size[" << i << "]=" << *remote_seq_read_size[i] << std::endl;
        output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:remote_seq_read_count[" << i << "]=" << *remote_seq_read_count[i] << std::endl;
      }

      if (*remote_rand_read_count[i] != 0) {
        output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:remote_rand_read_size[" << i << "]=" << *remote_rand_read_size[i] << std::endl;
        output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:remote_rand_read_count[" << i << "]=" << *remote_rand_read_count[i] << std::endl;
      }

      if (*remote_rand_rmw_count[i] !=0) {
        output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:remote_rand_rmw_size[" << i << "]=" << *remote_rand_rmw_size[i] << std::endl;
        output << "PROFILE: " << "[" << id << "] " << "WMDPartitioner:remote_rand_rmw_count[" << i << "]=" << *remote_rand_rmw_count[i] << std::endl;
      }
    }
  }
  #endif

  ////////////////////////////////////////////////////////////////////////////////
public:
  galois::GAccumulator<uint64_t> lgMapAccesses;
  /**
   * Construct a map from local edge GIDs to LID
   */
  void constructLocalEdgeGIDMap() {
    lgMapAccesses.reset();
    galois::StatTimer mapConstructTimer("GID2LIDMapConstructTimer", GRNAME);
    mapConstructTimer.start();

    localEdgeGIDToLID.reserve(base_DistGraph::sizeEdges());

    uint64_t count = 0;
    for (unsigned src = 0; src < base_DistGraph::size(); src++) {
      for (auto edge = base_DistGraph::edge_begin(src);
           edge != base_DistGraph::edge_end(src); edge++) {
        assert((*edge) == count);
        unsigned dst      = base_DistGraph::getEdgeDst(edge);
        uint64_t localGID = getEdgeGIDFromSD(src, dst);
        // insert into map
        localEdgeGIDToLID.insert(std::make_pair(localGID, count));
        count++;
      }
    }

    GALOIS_ASSERT(localEdgeGIDToLID.size() == base_DistGraph::sizeEdges());
    GALOIS_ASSERT(count == base_DistGraph::sizeEdges());

    mapConstructTimer.stop();
  }

  void reportAccessBefore() {
    galois::runtime::reportStat_Single(GRNAME, std::string("MapAccessesBefore"),
                                       lgMapAccesses.reduce());
  }

  void reportAccess() {
    galois::runtime::reportStat_Single(GRNAME, std::string("MapAccesses"),
                                       lgMapAccesses.reduce());
  }

  /**
   * checks map constructed above to see which local id corresponds
   * to a node/edge (if it exists)
   *
   * assumes map is generated
   */
  std::pair<uint64_t, bool> getLIDFromMap(unsigned src, unsigned dst) {
    lgMapAccesses += 1;
    // try to find gid in map
    uint64_t localGID = getEdgeGIDFromSD(src, dst);
    auto findResult   = localEdgeGIDToLID.find(localGID);

    // return if found, else return a false
    if (findResult != localEdgeGIDToLID.end()) {
      return std::make_pair(findResult->second, true);
    } else {
      // not found
      return std::make_pair((uint64_t)-1, false);
    }
  }

  uint64_t getEdgeLID(uint64_t gid) {
    uint64_t sourceNodeGID = edgeGIDToSource(gid);
    uint64_t sourceNodeLID = base_DistGraph::getLID(sourceNodeGID);
    uint64_t destNodeLID   = base_DistGraph::getLID(edgeGIDToDest(gid));

    for (auto edge : base_DistGraph::edges(sourceNodeLID)) {
      uint64_t edgeDst = base_DistGraph::getEdgeDst(edge);
      if (edgeDst == destNodeLID) {
        return *edge;
      }
    }
    GALOIS_DIE("unreachable");
    return (uint64_t)-1;
  }

  uint32_t findSourceFromEdge(uint64_t lid) {
    // TODO binary search
    // uint32_t left = 0;
    // uint32_t right = base_DistGraph::numNodes;
    // uint32_t mid = (left + right) / 2;

    for (uint32_t mid = 0; mid < base_DistGraph::numNodes; mid++) {
      uint64_t edge_left  = *(base_DistGraph::edge_begin(mid));
      uint64_t edge_right = *(base_DistGraph::edge_begin(mid + 1));

      if (edge_left <= lid && lid < edge_right) {
        return mid;
      }
    }

    GALOIS_DIE("unreachable");
    return (uint32_t)-1;
  }

  uint64_t getEdgeGID(uint64_t lid) {
    uint32_t src = base_DistGraph::getGID(findSourceFromEdge(lid));
    uint32_t dst = base_DistGraph::getGID(base_DistGraph::getEdgeDst(lid));
    return getEdgeGIDFromSD(src, dst);
  }

private:
  // https://www.quora.com/
  // Is-there-a-mathematical-function-that-converts-two-numbers-into-one-so-
  // that-the-two-numbers-can-always-be-extracted-again
  // GLOBAL IDS ONLY
  uint64_t getEdgeGIDFromSD(uint32_t source, uint32_t dest) {
    return source + (dest % base_DistGraph::numGlobalNodes) *
                        base_DistGraph::numGlobalNodes;
  }

  uint64_t edgeGIDToSource(uint64_t gid) {
    return gid % base_DistGraph::numGlobalNodes;
  }

  uint64_t edgeGIDToDest(uint64_t gid) {
    // assuming this floors
    return gid / base_DistGraph::numGlobalNodes;
  }

  /**
   * Fill up mirror arrays.
   * TODO make parallel?
   */
  void fillMirrors() {
    base_DistGraph::mirrorNodes.reserve(base_DistGraph::numNodes -
                                        base_DistGraph::numOwned);
    for (uint32_t i = base_DistGraph::numOwned; i < base_DistGraph::numNodes;
         i++) {
      uint32_t globalID = base_DistGraph::localToGlobalVector[i];
      base_DistGraph::mirrorNodes[graphPartitioner->retrieveMaster(globalID)]
          .push_back(globalID);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
};

// make GRNAME visible to public
template <typename NodeTy, typename EdgeTy, typename Partitioner>
constexpr const char* const
    galois::graphs::WMDGraph<NodeTy, EdgeTy, Partitioner>::GRNAME;

} // end namespace graphs
} // end namespace galois
#endif
