// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include "wf4/quiesce.hpp"

#include "galois/AtomicHelpers.h"

// location based partitioning

namespace {

static uint32_t SEND_NEW_EDGES = 79408;

void dumpUpdates(wf4::NetworkGraph& graph, wf4::GlobalNodeID node) {
  if (!graph.isOwned(node)) {
    return;
  }
  std::cout << std::endl << std::endl;
  auto src = graph.getLID(node);
  for (auto edge : graph.edges(src)) {
    auto data = graph.getEdgeData(edge);
    std::cout << "EDGE: SRC_UID: " << data.src << ", DST_UID: " << data.dst
              << ", DST_GID: " << data.dst_glbid << ", Local: "
              << graph.isOwned(graph.getGID(graph.getEdgeDst(edge)))
              << ", Sale: " << (data.type == agile::workflow1::TYPES::SALE)
              << ", Amount: " << data.amount_
              << ", Bought: " << graph.getData(graph.getEdgeDst(edge)).bought_
              << ", Sold: " << graph.getData(graph.getEdgeDst(edge)).sold_
              << std::endl;
  }
}

void verifyState(wf4::NetworkGraph& graph) {
  galois::do_all(
      galois::iterate(graph.allNodesRange()),
      [&](wf4::GlobalNodeID gid) {
        if (graph.isLocal(gid)) {
          auto data = graph.getData(graph.getLID(gid));
          if (data.bought_ > 0 && data.sold_ > data.bought_ + 0.01) {
            std::cout << "ERROR: NODE " << data.id
                      << " SOLD MORE THAN IT BOUGHT: " << data.sold_ << "/"
                      << data.bought_ << std::endl;
          }
          if (graph.isOwned(gid) && data.bought_ > data.desired_ + 0.01) {
            std::cout << "ERROR: NODE " << data.id
                      << " BOUGHT MORE THAN IT DESIRED: " << data.bought_ << "/"
                      << data.desired_ << std::endl;
          }
        }
      },
      galois::loopname("VerifyState"));
}

} // end namespace

void wf4::CancelNodes(
    wf4::NetworkGraph& graph,
    galois::graphs::GluonSubstrate<wf4::NetworkGraph>& substrate,
    std::vector<wf4::GlobalNodeID> nodes) {
  const bool async = false;
  bitset_bought_.resize(graph.size());
  bitset_bought_.reset();
  bitset_sold_.resize(graph.size());
  bitset_sold_.reset();

  galois::do_all(
      galois::iterate(nodes.begin(), nodes.end()),
      [&](GlobalNodeID node) { internal::CancelNode(graph, node); },
      galois::loopname("CancelNodes"));

  // update mirrors
  substrate.sync<WriteLocation::writeAny, ReadLocation::readAny,
                 Reduce_add_bought_, Bitset_bought_, async>(
      "Update bought values after cancellation");
  substrate.sync<WriteLocation::writeAny, ReadLocation::readAny,
                 Reduce_add_sold_, Bitset_sold_, async>(
      "Update sold values after cancellation");
  galois::runtime::getHostBarrier().wait();
}

void wf4::QuiesceGraph(
    wf4::NetworkGraph& graph,
    galois::graphs::GluonSubstrate<wf4::NetworkGraph>& substrate) {
  galois::Timer timer;
  timer.start();
  auto& net = galois::runtime::getSystemNetworkInterface();
  galois::gstl::Vector<phmap::parallel_flat_hash_set_m<internal::ProposedEdge>>
      remote_edges(net.Num);
  const bool async = false;
  bitset_bought_.resize(graph.size());
  bitset_bought_.reset();
  bitset_sold_.resize(graph.size());
  bitset_sold_.reset();

  // will require multiple iterations
  galois::do_all(
      galois::iterate(graph.masterNodesRange().begin(),
                      graph.masterNodesRange().end()),
      [&](NetworkGraph::GraphNode node) {
        internal::TryQuiesceNode(graph, remote_edges, node);
      },
      galois::loopname("TryQuiesceGraph"));

  std::vector<internal::IncomingEdges> potential_edges =
      internal::SendNewEdges(remote_edges);
  internal::TryAddEdges(graph, remote_edges, std::move(potential_edges));
  std::vector<internal::IncomingEdges> decided_edges =
      internal::SendNewEdges(remote_edges);
  internal::AddPurchaseEdges(graph, std::move(decided_edges));

  substrate.sync<WriteLocation::writeSource, ReadLocation::readDestination,
                 Reduce_set_sold_, Bitset_sold_, async>(
      "Update Mirrors with amount sold");
  substrate.sync<WriteLocation::writeSource, ReadLocation::readDestination,
                 Reduce_set_bought_, Bitset_bought_, async>(
      "Update Mirrors with amount bought");

  timer.stop();
  std::cout << "Quiescence time: " << timer.get_usec() / 1000000.0f << "s"
            << std::endl;

  // verifyState(graph);
}

bool wf4::internal::ProposedEdge::operator==(
    const wf4::internal::ProposedEdge& other) const {
  return src == other.src && dst == other.dst;
}

wf4::internal::IncomingEdges::IncomingEdges(
    galois::runtime::DeSerializeBuffer&& buffer)
    : buffer_(std::move(buffer)) {
  if (buffer_.size() < 16) {
    empty = true;
  } else {
    empty          = false;
    uint64_t* data = reinterpret_cast<uint64_t*>(buffer_.data());
    num_edges      = data[0];
    edges          = reinterpret_cast<ProposedEdge*>(&data[1]);
  }
}

void wf4::internal::CancelNode(wf4::NetworkGraph& graph,
                               wf4::GlobalNodeID node_gid) {
  if (!graph.isLocal(node_gid)) {
    return;
  }
  wf4::NetworkGraph::GraphNode node = graph.getLID(node_gid);
  graph.getData(node).Cancel();
  if (!graph.isOwned(node_gid)) {
    return;
  }

  for (auto& edge : graph.edges(node)) {
    wf4::NetworkEdge edge_data = graph.getEdgeData(edge);
    if (edge_data.type == agile::workflow1::TYPES::SALE) {
      wf4::NetworkGraph::GraphNode dst_node = graph.getEdgeDst(edge);
      // if destination is a mirror set it to 0 and subtract
      if (!graph.isOwned(graph.getGID(dst_node))) {
        galois::atomicMin(graph.getData(dst_node).bought_, 0.0);
      }
      galois::atomicSubtract(graph.getData(dst_node).bought_,
                             edge_data.amount_);
      bitset_bought_.set(dst_node);

      // TODO(Patrick)
      // graph.removeEdge(node, out_edge);
    } else if (edge_data.type == agile::workflow1::TYPES::PURCHASE) {
      wf4::NetworkGraph::GraphNode src_node = graph.getEdgeDst(edge);
      // if source is a mirror set it to 0 and subtract
      if (!graph.isOwned(graph.getGID(src_node))) {
        galois::atomicMin(graph.getData(src_node).sold_, 0.0);
      }
      galois::atomicSubtract(graph.getData(src_node).sold_, edge_data.amount_);
      bitset_sold_.set(src_node);

      // TODO(Patrick)
      // graph.removeInEdge(node, in_edge);
    }
  }
}

void wf4::internal::TryQuiesceNode(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<
        phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>>&
        remote_edges,
    wf4::NetworkGraph::GraphNode node) {
  using map = phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>;
  wf4::NetworkNode& node_data = graph.getData(node);
  double needed               = node_data.desired_ - node_data.bought_;
  for (auto& in_edge : graph.edges(node)) {
    if (needed <= 0) {
      return;
    }
    wf4::NetworkEdge edge_data = graph.getEdgeData(in_edge);
    if (edge_data.type != agile::workflow1::TYPES::PURCHASE) {
      continue;
    }
    wf4::NetworkGraph::GraphNode seller_node = graph.getEdgeDst(in_edge);
    wf4::NetworkNode& seller_data            = graph.getData(seller_node);

    // TODO(Patrick) explore negative case more closely
    double seller_surplus = seller_data.bought_ - seller_data.sold_;
    double trade_delta    = std::min(needed, seller_surplus);
    if (trade_delta <= 0) {
      continue;
    }
    wf4::GlobalNodeID seller_gid = graph.getGID(seller_node);

    // TODO(Patrick) add new edges instead of increasing old ones
    double seller_sold =
        galois::atomicAdd(graph.getData(seller_node).sold_, trade_delta) +
        trade_delta;
    if (seller_sold > seller_data.bought_) {
      galois::atomicSubtract(graph.getData(seller_node).sold_, trade_delta);
    } else {
      if (graph.isOwned(seller_gid)) {
        edge_data.amount_ += trade_delta;
        galois::atomicAdd(graph.getData(node).bought_, trade_delta);
        bitset_bought_.set(node);
        bitset_sold_.set(seller_node);
      } else {
        // defer updating state until remote edge confirmed
        ProposedEdge proposed_edge =
            ProposedEdge(graph.getGID(node), seller_gid, trade_delta);
        remote_edges[graph.getHostID(seller_gid)].lazy_emplace_l(
            std::move(proposed_edge), [&](map::value_type&) {},
            [&](const map::constructor& ctor) {
              ctor(std::move(proposed_edge));
            });
      }
      needed -= trade_delta;
    }
  }
}

void wf4::internal::TryAddEdges(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<
        phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>>&
        remote_edges,
    std::vector<IncomingEdges>&& incoming_edges) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  for (const IncomingEdges& edges : incoming_edges) {
    if (edges.empty) {
      continue;
    }
    galois::do_all(
        galois::iterate(uint64_t(0), edges.num_edges),
        [&](uint64_t edge) {
          TryAddEdge(graph, remote_edges, edges.edges[edge]);
        },
        galois::loopname("TryAddEdges"));
  }
}

void wf4::internal::TryAddEdge(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<
        phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>>&
        remote_edges,
    const wf4::internal::ProposedEdge& edge) {
  using map = phmap::parallel_flat_hash_set_m<ProposedEdge>;
  wf4::NetworkGraph::GraphNode seller_lid = graph.getLID(edge.dst);
  wf4::NetworkNode& seller_data           = graph.getData(seller_lid);
  // TODO(Patrick) explore negative case more closely
  double seller_surplus = seller_data.bought_ - seller_data.sold_;
  double trade_delta    = std::min(edge.weight, seller_surplus);
  if (trade_delta <= 0) {
    return;
  }

  double seller_sold =
      galois::atomicAdd(graph.getData(seller_lid).sold_, trade_delta) +
      trade_delta;
  if (seller_sold > seller_data.bought_) {
    galois::atomicSubtract(graph.getData(seller_lid).sold_, trade_delta);
  } else {
    // TODO(Patrick) add new edge instead of increasing old ones
    // edge_data.amount_ += trade_delta;
    bitset_sold_.set(seller_lid);
    // defer updating buyer state on master when new edge is added
    ProposedEdge proposed_edge = ProposedEdge(edge.src, edge.dst, trade_delta);
    remote_edges[graph.getHostID(edge.src)].lazy_emplace_l(
        std::move(proposed_edge), [&](map::value_type&) {},
        [&](const map::constructor& ctor) { ctor(std::move(proposed_edge)); });
  }
}

void wf4::internal::AddPurchaseEdges(
    wf4::NetworkGraph& graph,
    std::vector<wf4::internal::IncomingEdges>&& decided_edges) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  for (const IncomingEdges& edges : decided_edges) {
    if (edges.empty) {
      continue;
    }
    galois::do_all(
        galois::iterate(uint64_t(0), edges.num_edges),
        [&](uint64_t edge) { AddPurchaseEdge(graph, edges.edges[edge]); },
        galois::loopname("AddPurchaseEdge"));
  }
}

void wf4::internal::AddPurchaseEdge(wf4::NetworkGraph& graph,
                                    const wf4::internal::ProposedEdge& edge) {
  wf4::NetworkGraph::GraphNode buyer_lid = graph.getLID(edge.src);
  galois::atomicAdd(graph.getData(buyer_lid).bought_, edge.weight);
  // TODO(Patrick) add new edge instead of increasing old ones
  // edge_data.amount_ += trade_delta;
  bitset_bought_.set(buyer_lid);
}

std::vector<wf4::internal::IncomingEdges> wf4::internal::SendNewEdges(
    galois::gstl::Vector<
        phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>>&
        remote_edges) {
  auto& net          = galois::runtime::getSystemNetworkInterface();
  uint32_t host      = net.ID;
  uint32_t num_hosts = net.Num;
  // TODO(Patrick): ensure efficient serialization of phmap maps

  std::vector<IncomingEdges> incoming_edges(num_hosts);

  // send partial sets to other hosts
  for (uint32_t h = 0; h < num_hosts; h++) {
    if (h == host) {
      continue;
    }
    uint64_t size = remote_edges[h].size();
    galois::runtime::SendBuffer send_buffer;
    SerializeSet(send_buffer, remote_edges[h]);
    if (send_buffer.size() == 0) {
      send_buffer.push('a');
    }
    net.sendTagged(h, SEND_NEW_EDGES, std::move(send_buffer));
  }

  // recv node range from other hosts
  for (uint32_t h = 0; h < num_hosts - 1; h++) {
    decltype(net.recieveTagged(SEND_NEW_EDGES)) p;
    do {
      p = net.recieveTagged(SEND_NEW_EDGES);
    } while (!p);
    uint32_t sending_host        = p->first;
    incoming_edges[sending_host] = IncomingEdges(std::move(p->second));
  }
  SEND_NEW_EDGES++;

  return incoming_edges;
}

void wf4::internal::SerializeSet(
    galois::runtime::SendBuffer& buf,
    phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>& edges) {
  uint64_t length = 1 + edges.size() * 3;
  uint64_t data[length];
  uint64_t offset = 0;
  data[offset++]  = edges.size();
  for (const ProposedEdge& edge : edges) {
    data[offset++] = edge.src;
    data[offset++] = edge.dst;
    data[offset++] = edge.weight;
  }
  buf.insert(reinterpret_cast<uint8_t*>(&data), length * sizeof(uint64_t));
  edges.clear();
}
