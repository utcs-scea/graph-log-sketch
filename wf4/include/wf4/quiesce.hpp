// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>

#include "galois/graphs/GluonSubstrate.h"
#include "graph.hpp"
#include "parallel_hashmap/phmap.h"

namespace wf4 {

void CancelNodes(NetworkGraph& graph,
                 galois::graphs::GluonSubstrate<NetworkGraph>& substrate,
                 std::vector<GlobalNodeID> nodes);
void QuiesceGraph(NetworkGraph& graph,
                  galois::graphs::GluonSubstrate<wf4::NetworkGraph>& substrate);

namespace internal {

struct __attribute__((packed)) ProposedEdge {
  ProposedEdge(wf4::GlobalNodeID src_, wf4::GlobalNodeID dst_, double weight_)
      : src(src_), dst(dst_), weight(weight_) {}
  friend size_t hash_value(const ProposedEdge& set) {
    return phmap::HashState().combine(0, set.src, set.dst);
  }
  bool operator==(const ProposedEdge& other) const;

  wf4::GlobalNodeID src;
  wf4::GlobalNodeID dst;
  double weight;
} typedef ProposedEdge;

struct IncomingEdges {
  IncomingEdges() { empty = true; }
  explicit IncomingEdges(galois::runtime::DeSerializeBuffer&& buffer);

  galois::runtime::DeSerializeBuffer buffer_;
  ProposedEdge* edges;
  uint64_t num_edges;
  bool empty;
} typedef IncomingEdges;

void CancelNode(wf4::NetworkGraph& graph, wf4::GlobalNodeID node_gid);

void TryQuiesceNode(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<phmap::parallel_flat_hash_set_m<ProposedEdge>>&
        remote_edges,
    wf4::NetworkGraph::GraphNode node);

void TryAddEdges(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<phmap::parallel_flat_hash_set_m<ProposedEdge>>&
        remote_edges,
    std::vector<IncomingEdges>&& incoming_edges);
void TryAddEdge(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<phmap::parallel_flat_hash_set_m<ProposedEdge>>&
        remote_edges,
    const ProposedEdge& edge);

void AddPurchaseEdges(wf4::NetworkGraph& graph,
                      std::vector<IncomingEdges>&& decided_edges);
void AddPurchaseEdge(wf4::NetworkGraph& graph, const ProposedEdge& edge);

std::vector<IncomingEdges> SendNewEdges(
    galois::gstl::Vector<phmap::parallel_flat_hash_set_m<ProposedEdge>>&
        remote_edges);
void SerializeSet(galois::runtime::SendBuffer& buf,
                  phmap::parallel_flat_hash_set_m<ProposedEdge>& edges);

} // namespace internal

} // namespace wf4
