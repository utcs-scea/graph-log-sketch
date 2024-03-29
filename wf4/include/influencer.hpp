// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "galois/PerThreadContainer.h"
#include "galois/graphs/GluonSubstrate.h"
#include "galois/gstl.h"
#include "graph.hpp"
#include "parallel_hashmap/phmap.h"

namespace wf4 {

typedef std::vector<phmap::flat_hash_set<GlobalNodeID>> ReverseReachableSet;

void CalculateEdgeProbabilities(
    NetworkGraph& graph,
    galois::graphs::GluonSubstrate<NetworkGraph>& substrate);
ReverseReachableSet GetRandomReverseReachableSets(NetworkGraph& graph,
                                                  uint64_t num_sets,
                                                  uint64_t seed,
                                                  uint64_t epochs);
std::vector<GlobalNodeID>
GetInfluentialNodes(wf4::NetworkGraph& graph,
                    ReverseReachableSet&& reachability_sets, uint64_t num_nodes,
                    uint64_t influential_node_threshold);

namespace internal {

const uint64_t INITIAL_SET_SIZE = 10;
struct PartialReachableSet {
  PartialReachableSet()
      : id(0), owner_host(0),
        partial_reachable_set(
            phmap::flat_hash_set<wf4::GlobalNodeID>(INITIAL_SET_SIZE)),
        frontier(phmap::flat_hash_set<wf4::GlobalNodeID>(INITIAL_SET_SIZE)) {}
  PartialReachableSet(uint64_t id_, uint32_t host_, wf4::GlobalNodeID root_);

  bool Finished() { return frontier.size() == 0; }
  uint32_t NextHost(wf4::NetworkGraph& graph, bool finish);
  friend size_t hash_value(const PartialReachableSet& set) {
    return phmap::HashState().combine(0, set.owner_host, set.id);
  }
  void serialize(galois::runtime::SerializeBuffer& buf);
  void deserialize(galois::runtime::DeSerializeBuffer& buf);

  uint64_t id;
  uint64_t owner_host;
  phmap::flat_hash_set<wf4::GlobalNodeID> partial_reachable_set;
  phmap::flat_hash_set<wf4::GlobalNodeID> frontier;
} typedef PartialReachableSet;

struct LocalMaxNode {
  LocalMaxNode() : max_influence(0) {}
  LocalMaxNode(wf4::GlobalNodeID node, uint64_t influence)
      : max_node(node), max_influence(influence) {}

  wf4::GlobalNodeID max_node;
  uint64_t max_influence;
} typedef LocalMaxNode;

void FillNodeValues(wf4::NetworkGraph& graph,
                    const wf4::NetworkGraph::GraphNode& node);
void CalculateEdgeProbability(wf4::NetworkGraph& graph,
                              const wf4::NetworkGraph::GraphNode& node,
                              galois::DGAccumulator<double>& total_edge_weights,
                              galois::DGAccumulator<double>& total_sales);

void GenerateSeedNodes(galois::gstl::Vector<PartialReachableSet>& seed_nodes,
                       wf4::NetworkGraph& graph, uint64_t seed, uint64_t id,
                       uint64_t host);
void GenerateRandomReversibleReachableSets(
    wf4::ReverseReachableSet& reachability_sets, wf4::NetworkGraph& graph,
    galois::gstl::Vector<PartialReachableSet>&& partial_sets,
    galois::gstl::Vector<galois::PerThreadVector<PartialReachableSet>>&
        remote_partial_sets,
    bool finish);
void ContinueRandomReversibleReachableSet(wf4::NetworkGraph& graph,
                                          PartialReachableSet& partial_set,
                                          uint64_t random_noise, bool finish);

wf4::GlobalNodeID GetMostInfluentialNode(wf4::NetworkGraph& graph);
void FindLocalMaxNode(wf4::NetworkGraph& graph,
                      wf4::NetworkGraph::GraphNode node,
                      galois::gstl::Vector<LocalMaxNode>& local_max_node,
                      galois::DGAccumulator<uint64_t>& total_influence);
LocalMaxNode
GetMaxNode(galois::PerThreadVector<LocalMaxNode>& most_influential_nodes);
LocalMaxNode GetMaxNode(std::vector<LocalMaxNode>& most_influential_nodes);

void RemoveReachableSetsWithInfluentialNode(
    wf4::NetworkGraph& graph,
    phmap::flat_hash_set<wf4::GlobalNodeID>& reachability_set,
    const wf4::GlobalNodeID& influential_node,
    galois::gstl::Vector<
        phmap::parallel_flat_hash_map_m<wf4::GlobalNodeID, uint64_t>>&
        remote_updates);
void RemoveNodesFromRemotes(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<
        phmap::parallel_flat_hash_map_m<wf4::GlobalNodeID, uint64_t>>&
        remote_updates);

galois::gstl::Vector<PartialReachableSet> ExchangePartialSets(
    galois::gstl::Vector<galois::PerThreadVector<PartialReachableSet>>&
        remote_partial_sets);
std::vector<LocalMaxNode>
ExchangeMostInfluentialNodes(const LocalMaxNode& local_max);
void SerializeMap(
    galois::runtime::SendBuffer& buf,
    phmap::parallel_flat_hash_map_m<wf4::GlobalNodeID, uint64_t>& updates);

} // namespace internal

} // namespace wf4
