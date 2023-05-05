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

#include "WMDBufferedGraph.h"
#include "graph.h"
#include "data_types.h"

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

  std::vector<uint64_t> getNodeDegrees(const std::string filename,
                                       uint32_t numNodes) {
    std::vector<uint64_t> nodeDegrees;
    nodeDegrees.resize(numNodes);

    // read in prefix sum from GR on disk
    std::ifstream graphFile(filename.c_str());
    graphFile.seekg(sizeof(uint64_t) * 4);

    uint64_t* outIndexBuffer = (uint64_t*)malloc(sizeof(uint64_t) * numNodes);
    if (outIndexBuffer == nullptr) {
      GALOIS_DIE("out of memory");
    }
    uint64_t numBytesToLoad = numNodes * sizeof(uint64_t);
    uint64_t bytesRead      = 0;

    while (numBytesToLoad > 0) {
      graphFile.read(((char*)outIndexBuffer) + bytesRead, numBytesToLoad);
      size_t numRead = graphFile.gcount();
      numBytesToLoad -= numRead;
      bytesRead += numRead;
    }
    assert(numBytesToLoad == 0);

    galois::do_all(
        galois::iterate(0u, numNodes),
        [&](unsigned n) {
          if (n != 0) {
            nodeDegrees[n] = outIndexBuffer[n] - outIndexBuffer[n - 1];
          } else {
            nodeDegrees[n] = outIndexBuffer[0];
          }
          // galois::gDebug(n, " degree ", nodeDegrees[n]);
        },
        galois::loopname("GetNodeDegrees"), galois::no_stats());
    free(outIndexBuffer);

#ifndef NDEBUG
    if (base_DistGraph::id == 0) {
      galois::gDebug("Sanity checking node degrees");
    }

    galois::GAccumulator<uint64_t> edgeCount;
    galois::do_all(
        galois::iterate(0u, numNodes),
        [&](unsigned n) { edgeCount += nodeDegrees[n]; },
        galois::loopname("SanityCheckDegrees"), galois::no_stats());
    GALOIS_ASSERT(edgeCount.reduce() == base_DistGraph::numGlobalEdges);
#endif

    return nodeDegrees;
  }

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
      galois::graphs::MASTERS_DISTRIBUTION md = BALANCED_MASTERS,
      uint32_t nodeWeight = 0, uint32_t edgeWeight = 0)
      : base_DistGraph(host, _numHosts) {
    galois::runtime::reportParam(GRNAME, "WMDGraph", "0");
    galois::CondStatTimer<MORE_DIST_STATS> Tgraph_construct(
        "GraphPartitioningTime", GRNAME);
    Tgraph_construct.start();

    ////////////////////////////////////////////////////////////////////////////
    galois::gPrint("[", base_DistGraph::id, "] Starting graph reading.\n");
    galois::StatTimer graphReadTimer("GraphReading", GRNAME);
    graphReadTimer.start();

    galois::graphs::WMDBufferedGraph<agile::workflow1::Edge> bufGraph;
    bufGraph.loadPartialGraph(filename);
    base_DistGraph::numGlobalNodes = bufGraph.size();
    base_DistGraph::numGlobalEdges = bufGraph.sizeEdges();
    std::vector<unsigned> dummy;
    // not actually getting masters, but getting assigned readers for nodes
    galois::gPrint("[", base_DistGraph::id, "] numGlobalNodes ", bufGraph.offlineGraph.size(), ".\n");
    galois::gPrint("[", base_DistGraph::id, "] numGlobalEdges ", bufGraph.offlineGraph.sizeEdges(), ".\n");
    base_DistGraph::computeMasters(md, bufGraph.offlineGraph, dummy, nodeWeight, edgeWeight);


    // TODO: support Node degree (if needed by other policy)
    std::vector<uint64_t> ndegrees;
    // if (Partitioner::needNodeDegrees()) {
    //   if (base_DistGraph::id == 0) {
    //     galois::gInfo("Calculating node degrees for partitioner");
    //   }

    //   galois::runtime::reportParam(GRNAME, "UsingDegreeOrdering", "1");
    //   ndegrees = getNodeDegrees(filename, base_DistGraph::numGlobalNodes);
    // }

    graphPartitioner = std::make_unique<Partitioner>(
        host, _numHosts, base_DistGraph::numGlobalNodes,
        base_DistGraph::numGlobalEdges, ndegrees);
    graphPartitioner->saveGIDToHost(base_DistGraph::gid2host);

    ////////////////////////////////////////////////////////////////////////////

    uint64_t nodeBegin = base_DistGraph::gid2host[base_DistGraph::id].first;
    uint64_t nodeEnd = base_DistGraph::gid2host[base_DistGraph::id].second;

    bufGraph.loadEdges(nodeBegin, nodeEnd);
    graphReadTimer.stop();
    galois::gPrint("[", base_DistGraph::id, "] Reading graph complete in ", graphReadTimer.get_usec() / 1000000.0, " sec.\n");

    ////////////////////////////////////////////////////////////////////////////
    galois::gPrint("[", base_DistGraph::id, "] Starting edge inspection.\n");
    galois::StatTimer inspectionTimer("EdgeInspection", GRNAME);
    inspectionTimer.start();
    galois::gstl::Vector<uint64_t> prefixSumOfEdges;
    base_DistGraph::numOwned = nodeEnd - nodeBegin;
    prefixSumOfEdges.resize(base_DistGraph::numOwned);

    // initial pass; set up lid-gid mappings, determine which proxies exist on
    // this host; prefix sum of edges can be set up to the last master
     
    galois::DynamicBitSet presentProxies =
        edgeInspectionRound1(bufGraph, prefixSumOfEdges);
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
    base_DistGraph::numEdges = bufGraph.sizeLocalEdges();
    // signifies how many outgoing edges a particular host should expect from
    // this host
    // std::vector<std::vector<uint64_t>> numOutgoingEdges;
    // numOutgoingEdges.resize(base_DistGraph::numHosts);
    // // edge inspection phase 2: determine how many edges to send to each host
    // // don't actually send yet
    // edgeInspectionRound2(bufGraph, numOutgoingEdges, proxiesOnOtherHosts);

    // // prefix sum finalization
    // finalizePrefixSum(numOutgoingEdges, prefixSumOfEdges);

    // // doubly make sure the data is cleared
    // freeVector(numOutgoingEdges); // should no longer use this variable
    inspectionTimer.stop();

    galois::gPrint("[", base_DistGraph::id, "] Edge inspection complete in ", inspectionTimer.get_usec() / 1000000.0, " sec.\n");
    ////////////////////////////////////////////////////////////////////////////
    galois::gPrint("[", base_DistGraph::id, "] Starting graph applocation.\n");
    galois::StatTimer allocationTimer("GraphAllocation", GRNAME);
    allocationTimer.start();

    galois::gPrint("[", base_DistGraph::id, "] numLocalNodes ", base_DistGraph::numNodes, ".\n");
    galois::gPrint("[", base_DistGraph::id, "] numLocalEdges ", base_DistGraph::numEdges, ".\n");
    // Graph construction related calls
    base_DistGraph::beginMaster = 0;
    // Allocate and construct the graph
    base_DistGraph::graph.allocateFrom(base_DistGraph::numNodes,
                                       base_DistGraph::numEdges);
    base_DistGraph::graph.constructNodes();

    // construct edges
    // not need to move edges from other host since all edges is already ready when no edge mirror are used.
    uint64_t offset = bufGraph.nodeOffset();
    galois::do_all(galois::iterate((uint64_t)0, (uint64_t) base_DistGraph::numNodes),
    [&](uint64_t n)
    {
      uint64_t glid = n + offset;
      auto edgeDst = bufGraph.edgeLocalDst(glid, base_DistGraph::globalToLocalMap);
      auto edgeData = bufGraph.edgeData(glid, base_DistGraph::globalToLocalMap);
      base_DistGraph::graph.addEdgesUnSort(true, n, edgeDst.data(), edgeData.data(), bufGraph.edgeNum(glid), true);
    }, galois::steal());

    assert(base_DistGraph::graph.size() == base_DistGraph::numNodes);
    assert(base_DistGraph::graph.sizeEdges() == base_DistGraph::numEdges);

//     // edge end fixing
//     auto& base_graph = base_DistGraph::graph;
//     galois::do_all(
//         galois::iterate((uint32_t)0, base_DistGraph::numNodes),
//         [&](uint64_t n) { base_graph.fixEndEdge(n, prefixSumOfEdges[n]); },
// #if MORE_DIST_STATS
//         galois::loopname("FixEndEdgeLoop"),
// #endif
//         galois::no_stats());
//     // get memory from prefix sum back
//     prefixSumOfEdges.clear();
//     freeVector(prefixSumOfEdges); // should no longer use this variable

    allocationTimer.stop();
    galois::gPrint("[", base_DistGraph::id, "] Graph applocation complete in ", allocationTimer.get_usec() / 1000000.0, " sec.\n");
    galois::gPrint("[", base_DistGraph::id, "] CSR building total time is ", (allocationTimer.get_usec() + inspectionTimer.get_usec())  / 1000000.0, " sec.\n");
    ////////////////////////////////////////////////////////////////////////////

    // if (setupGluon) {
    //   galois::CondStatTimer<MORE_DIST_STATS> TfillMirrors("FillMirrors",
    //                                                       GRNAME);

    //   TfillMirrors.start();
    //   fillMirrors();
    //   TfillMirrors.stop();
    // }

    ////////////////////////////////////////////////////////////////////////////

    // loadEdges(base_DistGraph::graph, bufGraph, proxiesOnOtherHosts);
    // TODO this might be useful to keep around
    proxiesOnOtherHosts.clear();
    ndegrees.clear();

    // SORT EDGES
    if (doSort) {
      base_DistGraph::sortEdgesByDestination();
    }

    // if (setupGluon) {
    //   galois::CondStatTimer<MORE_DIST_STATS> TfillMirrorsEdges(
    //       "FillMirrorsEdges", GRNAME);
    //   TfillMirrorsEdges.start();
    //   // edges
    //   mirrorEdges.resize(base_DistGraph::numHosts);
    //   galois::gPrint("[", base_DistGraph::id,
    //                  "] Filling mirrors and creating "
    //                  "mirror map\n");
    //   fillMirrorsEdgesAndCreateMirrorMap();
    //   TfillMirrorsEdges.stop();
    // }

    ////////////////////////////////////////////////////////////////////////////

    galois::CondStatTimer<MORE_DIST_STATS> Tthread_ranges("ThreadRangesTime",
                                                          GRNAME);

    galois::gPrint("[", base_DistGraph::id, "] Determining thread ranges\n");

    Tthread_ranges.start();
    base_DistGraph::determineThreadRanges();
    base_DistGraph::determineThreadRangesMaster();
    base_DistGraph::determineThreadRangesWithEdges();
    base_DistGraph::initializeSpecificRanges();
    Tthread_ranges.stop();

    Tgraph_construct.stop();
    galois::gPrint("[", base_DistGraph::id, "] Graph construction complete.\n");

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
      GALOIS_ASSERT(globalKeptEdges * 2 == base_DistGraph::globalSizeEdges());
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
  edgeInspectionRound1(galois::graphs::WMDBufferedGraph<agile::workflow1::Edge>& bufGraph,
                       galois::gstl::Vector<uint64_t>& prefixSumOfEdges) {
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
    galois::gPrint("[", base_DistGraph::id, "] edgeInspectionRound1 - start do_all.\n");
    galois::do_all(
        galois::iterate(base_DistGraph::gid2host[base_DistGraph::id].first,
                        base_DistGraph::gid2host[base_DistGraph::id].second),
        [&](size_t n) {
          // galois::gPrint("[", base_DistGraph::id, "] node ", n , ".\n");
          uint64_t edgeCount = 0;
          auto itr = bufGraph.edgeRange(n);
          for (auto cur=itr.first; cur != itr.second; ++cur) {
            uint64_t dst = cur->second.dst_glbid;

            if (graphPartitioner->keepEdge(n, dst)) {
              edgeCount++;
              keptEdges += 1;
              // which mirrors do I have
              if (graphPartitioner->retrieveMaster(dst) != myID) {
                incomingMirrors.set(dst);
              }
            }
          }
          allEdges += edgeCount;
          prefixSumOfEdges[n - globalOffset] = edgeCount;
          ltgv[n - globalOffset]             = n;
        },
#if MORE_DIST_STATS
        galois::loopname("EdgeInspectionLoop"),
#endif
        galois::steal(), galois::no_stats());
    galois::gPrint("[", base_DistGraph::id, "] edgeInspectionRound1 - finish do_all.\n");
    myKeptEdges     = keptEdges.read_local();
    myReadEdges     = allEdges.reduce();
    globalKeptEdges = keptEdges.reduce();

    // get incoming mirrors ready for creation
    uint32_t additionalMirrorCount = incomingMirrors.count();
    base_DistGraph::localToGlobalVector.resize(
        base_DistGraph::localToGlobalVector.size() + additionalMirrorCount);

    // note prefix sum will get finalized in a later step
    if (base_DistGraph::numOwned > 0) {
      prefixSumOfEdges.resize(prefixSumOfEdges.size() + additionalMirrorCount,
                              0);
    } else {
      prefixSumOfEdges.resize(additionalMirrorCount, 0);
    }

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
      galois::gPrint("[", base_DistGraph::id, "] edgeInspectionRound1 - start on_each.\n");
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
      galois::gPrint("[", base_DistGraph::id, "] edgeInspectionRound1 - finish on_each.\n");
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

  void edgeInspectionRound2(
      galois::graphs::WMDBufferedGraph<agile::workflow1::Edge>& bufGraph,
      std::vector<std::vector<uint64_t>>& numOutgoingEdges,
      std::vector<galois::DynamicBitSet>& proxiesOnOtherHosts) {
    auto& net = galois::runtime::getSystemNetworkInterface();
    // allocate vectors for counting edges that must be sent
    // number of nodes that this host has read from disk
    uint32_t numRead = base_DistGraph::gid2host[base_DistGraph::id].second -
                       base_DistGraph::gid2host[base_DistGraph::id].first;
    // allocate space for outgoing edges
    for (uint32_t i = 0; i < base_DistGraph::numHosts; ++i) {
      numOutgoingEdges[i].assign(numRead, 0);
    }
    uint64_t globalOffset = base_DistGraph::gid2host[base_DistGraph::id].first;

    galois::DynamicBitSet hostHasOutgoing;
    hostHasOutgoing.resize(base_DistGraph::numHosts);
    hostHasOutgoing.reset();

    // flip loop order, this can be optimized
    // for each host, loop over my local nodes
    galois::do_all(
        galois::iterate(base_DistGraph::gid2host[base_DistGraph::id].first,
                        base_DistGraph::gid2host[base_DistGraph::id].second),
        [&](size_t n) {
          auto itr = bufGraph.edgeRange(n);
          for (auto cur=itr.first; cur != itr.second; ++cur) {
            uint64_t dst = cur->second.dst_glbid;
            // make sure this edge is going to be kept and not dropped
            if (graphPartitioner->keepEdge(n, dst)) {
              for (unsigned h = 0; h < net.Num; h++) {
                if (h != net.ID) {
                  if (proxiesOnOtherHosts[h].test(n)) {
                    // if kept, make sure destination exists on that host
                    if (proxiesOnOtherHosts[h].test(dst)) {
                      // if it does, this edge must be duplicated on that host;
                      // increment count
                      numOutgoingEdges[h][n - globalOffset] += 1;
                      hostHasOutgoing.set(h);
                    }
                  }
                }
              }
            }
          }
        },
#if MORE_DIST_STATS
        galois::loopname("EdgeInspectionRound2Loop"),
#endif
        galois::steal(), galois::no_stats());

    // send data off, then receive it
    sendInspectionData(numOutgoingEdges, hostHasOutgoing);
    recvInspectionData(numOutgoingEdges);
    base_DistGraph::increment_evilPhase();
  }

  /**
   * Send data out from inspection to other hosts.
   *
   * @param[in,out] numOutgoingEdges specifies which nodes on a host will have
   * outgoing edges
   * @param[in] hostHasOutgoing bitset tracking which hosts have outgoing
   * edges from this host
   */
  void sendInspectionData(std::vector<std::vector<uint64_t>>& numOutgoingEdges,
                          galois::DynamicBitSet& hostHasOutgoing) {
    auto& net = galois::runtime::getSystemNetworkInterface();

    galois::GAccumulator<uint64_t> bytesSent;
    bytesSent.reset();

    for (unsigned h = 0; h < net.Num; h++) {
      if (h == net.ID) {
        continue;
      }

      // send outgoing edges data off to comm partner
      galois::runtime::SendBuffer b;

      // only send if non-zeros exist
      if (hostHasOutgoing.test(h)) {
        galois::runtime::gSerialize(b, 1); // token saying data exists
        galois::runtime::gSerialize(b, numOutgoingEdges[h]);
      } else {
        galois::runtime::gSerialize(b, 0); // token saying no data exists
      }
      numOutgoingEdges[h].clear();

      bytesSent.update(b.size());

      // send buffer and free memory
      net.sendTagged(h, galois::runtime::evilPhase, b);
      b.getVec().clear();
    }
    galois::runtime::reportStat_Tsum(
        GRNAME, std::string("EdgeInspectionBytesSent"), bytesSent.reduce());

    galois::gPrint("[", base_DistGraph::id, "] Inspection sends complete.\n");
  }

  /**
   * Receive data from inspection from other hosts. Processes the incoming
   * edge bitsets/offsets.
   *
   * @param[in,out] numOutgoingEdges specifies which nodes on a host will have
   * outgoing edges
   */
  void
  recvInspectionData(std::vector<std::vector<uint64_t>>& numOutgoingEdges) {
    auto& net = galois::runtime::getSystemNetworkInterface();

    for (unsigned h = 0; h < net.Num - 1; h++) {
      // expect data from comm partner back
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);

      uint32_t sendingHost = p->first;

      // get outgoing edges; first get status var
      uint32_t outgoingExists = 2;
      galois::runtime::gDeserialize(p->second, outgoingExists);

      if (outgoingExists == 1) {
        // actual data sent
        galois::runtime::gDeserialize(p->second, numOutgoingEdges[sendingHost]);
      } else if (outgoingExists == 0) {
        // no data sent; just clear again
        numOutgoingEdges[sendingHost].clear();
      } else {
        GALOIS_DIE("unreachable: ", outgoingExists);
      }
    }

    galois::gPrint("[", base_DistGraph::id,
                   "] Inspection receives complete.\n");
  }

  /**
   * Take inspection metadata and begin mapping nodes/creating prefix sums,
   * return the prefix sum.
   */
  galois::gstl::Vector<uint64_t>
  finalizePrefixSum(std::vector<std::vector<uint64_t>>& numOutgoingEdges,
                    galois::gstl::Vector<uint64_t>& prefixSumOfEdges) {
    base_DistGraph::numEdges = 0;

    inspectOutgoingNodes(numOutgoingEdges, prefixSumOfEdges);
    finalizeInspection(prefixSumOfEdges);
    galois::gDebug("[", base_DistGraph::id,
                   "] To receive this many nodes: ", nodesToReceive);
    galois::gPrint("[", base_DistGraph::id,
                   "] Inspection allocation complete.\n");
    return prefixSumOfEdges;
  }

  /**
   * Outgoing inspection: loop over proxy nodes, determnine if need to receive
   * edges.
   */
  void
  inspectOutgoingNodes(std::vector<std::vector<uint64_t>>& numOutgoingEdges,
                       galois::gstl::Vector<uint64_t>& prefixSumOfEdges) {
    galois::GAccumulator<uint32_t> toReceive;
    toReceive.reset();

    uint32_t proxyStart = base_DistGraph::numOwned;
    uint32_t proxyEnd   = base_DistGraph::numNodes;
    assert(proxyEnd == prefixSumOfEdges.size());

    galois::GAccumulator<uint64_t> edgesToReceive;
    edgesToReceive.reset();

    // loop over proxy nodes, see if edges need to be sent from another host
    // by looking at results of edge inspection
    galois::do_all(
        galois::iterate(proxyStart, proxyEnd),
        [&](uint32_t lid) {
          uint64_t gid = base_DistGraph::localToGlobalVector[lid];
          assert(gid < base_DistGraph::numGlobalNodes);
          unsigned hostReader = getHostReader(gid);
          assert(hostReader < base_DistGraph::numHosts);
          assert(hostReader != base_DistGraph::id); // self shouldn't be proxy

          uint64_t nodeOffset = base_DistGraph::gid2host[hostReader].first;
          if (numOutgoingEdges[hostReader].size()) {
            if (numOutgoingEdges[hostReader][gid - nodeOffset]) {
              // if this host is going to send me edges, note it for future use
              prefixSumOfEdges[lid] =
                  numOutgoingEdges[hostReader][gid - nodeOffset];
              edgesToReceive += numOutgoingEdges[hostReader][gid - nodeOffset];
              toReceive += 1;
            }
          }
        },
        galois::loopname("OutgoingNodeInspection"), galois::steal(),
        galois::no_stats());

    galois::gPrint("[", base_DistGraph::id, "] Need receive ",
                   edgesToReceive.reduce(), " edges; self is ", myKeptEdges,
                   "\n");
    // get memory back
    numOutgoingEdges.clear();
    nodesToReceive = toReceive.reduce();
  }

  /**
   * finalize metadata maps
   */
  void finalizeInspection(galois::gstl::Vector<uint64_t>& prefixSumOfEdges) {
    for (unsigned i = 1; i < base_DistGraph::numNodes; i++) {
      // finalize prefix sum
      prefixSumOfEdges[i] += prefixSumOfEdges[i - 1];
    }
    if (prefixSumOfEdges.size() != 0) {
      base_DistGraph::numEdges = prefixSumOfEdges.back();
    } else {
      base_DistGraph::numEdges = 0;
    }
  }

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

  void fillMirrorsEdgesAndCreateMirrorMap() {
    for (uint32_t src = base_DistGraph::numOwned;
         src < base_DistGraph::numNodes; src++) {
      auto ee               = base_DistGraph::edge_begin(src);
      auto ee_end           = base_DistGraph::edge_end(src);
      uint32_t globalSource = base_DistGraph::getGID(src);
      unsigned sourceOwner  = graphPartitioner->retrieveMaster(globalSource);

      for (; ee != ee_end; ++ee) {
        // create mirror array
        uint64_t edgeGID = getEdgeGIDFromSD(
            globalSource,
            base_DistGraph::getGID(base_DistGraph::getEdgeDst(ee)));
        mirrorEdges[sourceOwner].push_back(edgeGID);
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////

  template <typename GraphTy>
  void loadEdges(GraphTy& graph, galois::graphs::WMDBufferedGraph<agile::workflow1::Edge>& bufGraph,
                 std::vector<galois::DynamicBitSet>& proxiesOnOtherHosts) {
    galois::StatTimer loadEdgeTimer("EdgeLoading", GRNAME);
    loadEdgeTimer.start();

    bufGraph.resetReadCounters();
    std::atomic<uint32_t> receivedNodes;
    receivedNodes.store(0);

    // sends data
    sendEdges(graph, bufGraph, receivedNodes, proxiesOnOtherHosts);
    // uint64_t bufBytesRead = bufGraph.getBytesRead();
    // get data from graph back (don't need it after sending things out)
    bufGraph.resetAndFree();

    // receives data
    galois::on_each(
        [&](unsigned GALOIS_UNUSED(tid), unsigned GALOIS_UNUSED(nthreads)) {
          receiveEdges(graph, receivedNodes);
        });
    base_DistGraph::increment_evilPhase();
    loadEdgeTimer.stop();

    galois::gPrint("[", base_DistGraph::id, "] Edge loading time: ",
                   loadEdgeTimer.get_usec() / 1000000.0f, " seconds\n");
  }

//   // no edge data version
//   template <typename GraphTy>
//   void sendEdges(GraphTy& graph, galois::graphs::WMDBufferedGraph<agile::workflow1::Edge>& bufGraph,
//                  std::atomic<uint32_t>& receivedNodes,
//                  std::vector<galois::DynamicBitSet>& proxiesOnOtherHosts) {
//     using DstVecType      = std::vector<std::vector<uint64_t>>;
//     using SendBufferVecTy = std::vector<galois::runtime::SendBuffer>;

//     galois::substrate::PerThreadStorage<DstVecType> gdst_vecs(
//         base_DistGraph::numHosts);
//     galois::substrate::PerThreadStorage<SendBufferVecTy> sendBuffers(
//         base_DistGraph::numHosts);

//     auto& net                = galois::runtime::getSystemNetworkInterface();
//     const unsigned& id       = this->base_DistGraph::id;
//     const unsigned& numHosts = this->base_DistGraph::numHosts;

//     galois::GAccumulator<uint64_t> messagesSent;
//     galois::GAccumulator<uint64_t> bytesSent;
//     galois::GReduceMax<uint64_t> maxBytesSent;
//     messagesSent.reset();
//     bytesSent.reset();
//     maxBytesSent.reset();

//     // Go over assigned nodes and distribute edges.
//     galois::do_all(
//         galois::iterate(base_DistGraph::gid2host[base_DistGraph::id].first,
//                         base_DistGraph::gid2host[base_DistGraph::id].second),
//         [&](uint64_t src) {
//           uint32_t lsrc    = 0;
//           uint64_t curEdge = 0;
//           if (base_DistGraph::isLocal(src)) {
//             lsrc    = this->G2L(src);
//             curEdge = *graph.edge_begin(lsrc, galois::MethodFlag::UNPROTECTED);
//           }

//           auto ee        = bufGraph.edgeBegin(src);
//           auto ee_end    = bufGraph.edgeEnd(src);
//           auto& gdst_vec = *gdst_vecs.getLocal();

//           for (unsigned i = 0; i < numHosts; ++i) {
//             gdst_vec[i].clear();
//           }

//           for (; ee != ee_end; ++ee) {
//             uint64_t gdst = ee->second.dst_glbid;
//             // make sure this edge is going to be kept and not dropped
//             if (graphPartitioner->keepEdge(src, gdst)) {
//               assert(base_DistGraph::isLocal(src));
//               uint32_t ldst = this->G2L(gdst);
//               graph.constructEdge(curEdge++, ldst);

//               for (unsigned h = 0; h < net.Num; h++) {
//                 if (h != net.ID) {
//                   if (proxiesOnOtherHosts[h].test(src)) {
//                     // if kept, make sure destination exists on that host
//                     if (proxiesOnOtherHosts[h].test(gdst)) {
//                       // if it does, this edge must be duplicated on that host;
//                       // increment count
//                       gdst_vec[h].push_back(gdst);
//                     }
//                   }
//                 }
//               }
//             }
//           }

//           // make sure all edges accounted for if local
//           if (base_DistGraph::isLocal(src)) {
//             assert(curEdge == (*graph.edge_end(lsrc)));
//           }

//           // send
//           for (uint32_t h = 0; h < numHosts; ++h) {
//             if (h == id)
//               continue;

//             if (gdst_vec[h].size() > 0) {
//               auto& b = (*sendBuffers.getLocal())[h];
//               galois::runtime::gSerialize(b, src);
//               galois::runtime::gSerialize(b, gdst_vec[h]);

//               // send if over limit
//               if (b.size() > edgePartitionSendBufSize) {
//                 messagesSent += 1;
//                 bytesSent.update(b.size());
//                 maxBytesSent.update(b.size());

//                 net.sendTagged(h, galois::runtime::evilPhase, b);
//                 b.getVec().clear();
//                 b.getVec().reserve(edgePartitionSendBufSize * 1.25);
//               }
//             }
//           }

//           // overlap receives
//           auto buffer = net.recieveTagged(galois::runtime::evilPhase, nullptr);
//           this->processReceivedEdgeBuffer(buffer, graph, receivedNodes);
//         },
// #if MORE_DIST_STATS
//         galois::loopname("EdgeLoading"),
// #endif
//         galois::steal(), galois::no_stats());

//     // flush buffers
//     for (unsigned threadNum = 0; threadNum < sendBuffers.size(); ++threadNum) {
//       auto& sbr = *sendBuffers.getRemote(threadNum);
//       for (unsigned h = 0; h < this->base_DistGraph::numHosts; ++h) {
//         if (h == this->base_DistGraph::id)
//           continue;
//         auto& sendBuffer = sbr[h];
//         if (sendBuffer.size() > 0) {
//           messagesSent += 1;
//           bytesSent.update(sendBuffer.size());
//           maxBytesSent.update(sendBuffer.size());

//           net.sendTagged(h, galois::runtime::evilPhase, sendBuffer);
//           sendBuffer.getVec().clear();
//         }
//       }
//     }

//     net.flush();

//     galois::runtime::reportStat_Tsum(
//         GRNAME, std::string("EdgeLoadingMessagesSent"), messagesSent.reduce());
//     galois::runtime::reportStat_Tsum(
//         GRNAME, std::string("EdgeLoadingBytesSent"), bytesSent.reduce());
//     galois::runtime::reportStat_Tmax(
//         GRNAME, std::string("EdgeLoadingMaxBytesSent"), maxBytesSent.reduce());
//   }

//   //! @copydoc DistGraphHybridCut::processReceivedEdgeBuffer
//   template <typename GraphTy>
//   void processReceivedEdgeBuffer(
//       std::optional<std::pair<uint32_t, galois::runtime::RecvBuffer>>& buffer,
//       GraphTy& graph, std::atomic<uint32_t>& receivedNodes) {
//     if (buffer) {
//       auto& rb = buffer->second;
//       while (rb.r_size() > 0) {
//         uint64_t n;
//         std::vector<uint64_t> gdst_vec;
//         galois::runtime::gDeserialize(rb, n);
//         galois::runtime::gDeserialize(rb, gdst_vec);
//         assert(base_DistGraph::isLocal(n));
//         uint32_t lsrc = this->G2L(n);
//         uint64_t cur = *graph.edge_begin(lsrc, galois::MethodFlag::UNPROTECTED);
//         uint64_t cur_end = *graph.edge_end(lsrc);
//         assert((cur_end - cur) == gdst_vec.size());
//         deserializeEdges(graph, gdst_vec, cur, cur_end);
//         ++receivedNodes;
//       }
//     }
//   }

//   /**
//    * Receive the edge dest/data assigned to this host from other hosts
//    * that were responsible for reading them.
//    */
//   template <typename GraphTy>
//   void receiveEdges(GraphTy& graph, std::atomic<uint32_t>& receivedNodes) {
//     auto& net = galois::runtime::getSystemNetworkInterface();

//     // receive edges for all mirror nodes
//     while (receivedNodes < nodesToReceive) {
//       decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
//       p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
//       processReceivedEdgeBuffer(p, graph, receivedNodes);
//     }
//   }

//   template <typename GraphTy>
//   void deserializeEdges(GraphTy& graph, std::vector<uint64_t>& gdst_vec,
//                         uint64_t& cur, uint64_t& cur_end) {
//     uint64_t i = 0;
//     while (cur < cur_end) {
//       uint64_t gdst = gdst_vec[i++];
//       uint32_t ldst = this->G2L(gdst);
//       graph.constructEdge(cur++, ldst);
//     }
//   }
};

// make GRNAME visible to public
template <typename NodeTy, typename EdgeTy, typename Partitioner>
constexpr const char* const
    galois::graphs::WMDGraph<NodeTy, EdgeTy, Partitioner>::GRNAME;

} // end namespace graphs
} // end namespace galois
#endif
