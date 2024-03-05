// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include "influencer.hpp"

#include <queue>
#include <random>

#include "galois/AtomicHelpers.h"
#include "parallel_hashmap/phmap_dump.h"
#include "pcg_random.hpp"

namespace {

static uint32_t EXCHANGE_INFLUENTIAL_NODES = 9801;
static uint32_t EXCHANGE_PARTIAL_SETS      = 19602;
static uint32_t REMOVE_RRR_SETS            = 39204;

void dumpGraph(wf4::NetworkGraph& graph) {
  for (auto src : graph.masterNodesRange()) {
    std::cout << "LID: " << src << ", GID: " << graph.getGID(src)
              << ", LID2: " << graph.getLID(graph.getGID(src))
              << ", UID: " << graph.getData(src).id
              << ", Sold: " << graph.getData(src).sold_ << std::endl;
    for (auto edge : graph.edges(src)) {
      std::cout << "TOPOLOGY: SRC_LID: " << src
                << ", DST_LID: " << graph.getEdgeDst(edge)
                << ", DST_GID: " << graph.getGID(graph.getEdgeDst(edge))
                << ", DST_UID: " << graph.getData(graph.getEdgeDst(edge)).id
                << std::endl;
      auto data = graph.getEdgeData(edge);
      std::cout << "EDGE: SRC_UID: " << data.src
                << ", SRC_GID: " << data.src_glbid << ", DST_UID: " << data.dst
                << ", DST_GID: " << data.dst_glbid
                << ", Sold: " << graph.getData(graph.getEdgeDst(edge)).sold_
                << std::endl;
    }
  }
}

void verifyInfluentialNodesDebug(
    wf4::NetworkGraph& graph,
    const std::vector<wf4::GlobalNodeID>& influential_nodes,
    uint64_t influential_node_threshold) {
  // This function only works with synthetically generated graphs
  // from the `tools` directory and is for debugging ease only

  if (influential_node_threshold == 0) {
    return;
  }
  auto& net = galois::runtime::getSystemNetworkInterface();
  galois::DGAccumulator<uint64_t> non_influential_nodes;
  non_influential_nodes.reset();
  for (const wf4::GlobalNodeID& influential_node : influential_nodes) {
    if (graph.isOwned(influential_node)) {
      const wf4::NetworkNode& node_data =
          graph.getData(graph.getLID(influential_node));
      if (node_data.id > influential_node_threshold) {
        non_influential_nodes += 1;
      }
    }
  }
  non_influential_nodes.reduce();
  if (net.ID == 0 &&
      non_influential_nodes.read() > influential_nodes.size() / 10) {
    std::cout << "ERROR: Too many non-influential nodes found: "
              << non_influential_nodes.read() << "/" << influential_nodes.size()
              << std::endl;
    std::cerr << "ERROR: Too many non-influential nodes found: "
              << non_influential_nodes.read() << "/" << influential_nodes.size()
              << std::endl;
    std::cout << "The node threshold was: " << influential_node_threshold
              << std::endl;
    std::cerr << "The node threshold was: " << influential_node_threshold
              << std::endl;
  }
}

} // end namespace

void wf4::CalculateEdgeProbabilities(
    wf4::NetworkGraph& graph,
    galois::graphs::GluonSubstrate<wf4::NetworkGraph>& substrate) {
  const bool async = false;
  substrate.set_num_round(0);
  bitset_sold_.resize(graph.size());
  bitset_sold_.reset();
  galois::do_all(
      galois::iterate(graph.masterNodesRange().begin(),
                      graph.masterNodesRange().end()),
      [&](NetworkGraph::GraphNode node) {
        internal::FillNodeValues(graph, node);
      },
      galois::loopname("FillNodeValues"));
  // update mirrors
  substrate.sync<WriteLocation::writeSource, ReadLocation::readDestination,
                 Reduce_set_sold_, Bitset_sold_, async>("Update Mirrors");
  galois::runtime::getHostBarrier().wait();
  bitset_sold_.resize(0);

  galois::DGAccumulator<double> total_edge_weights;
  galois::DGAccumulator<double> total_sales;
  total_edge_weights.reset();
  total_sales.reset();
  galois::do_all(
      galois::iterate(graph.masterNodesRange().begin(),
                      graph.masterNodesRange().end()),
      [&](NetworkGraph::GraphNode node) {
        internal::CalculateEdgeProbability(graph, node, total_edge_weights,
                                           total_sales);
      },
      galois::loopname("CalculateEdgeProbabilities"));
  total_edge_weights.reduce();
  total_sales.reduce();
  if (galois::runtime::getSystemNetworkInterface().ID == 0) {
    std::cout << "Total Edge weights: " << total_edge_weights.read()
              << std::endl;
    std::cout << "Total sold: " << total_sales.read() << std::endl;
  }
}

wf4::ReverseReachableSet
wf4::GetRandomReverseReachableSets(wf4::NetworkGraph& graph,
                                   uint64_t global_sets, uint64_t seed,
                                   uint64_t epochs) {
  galois::Timer timer;
  timer.start();
  galois::Timer seed_timer;
  seed_timer.start();

  uint32_t threads   = galois::getActiveThreads();
  auto& net          = galois::runtime::getSystemNetworkInterface();
  uint32_t host      = net.ID;
  uint32_t num_hosts = net.Num;
  uint64_t host_sets = global_sets / num_hosts;
  if (host == num_hosts - 1) {
    host_sets = global_sets - ((num_hosts - 1) * (global_sets / num_hosts));
  }
  // lazy allocation does not help, some solution should be put in place to
  // limit memory overhead
  ReverseReachableSet reachable_sets(
      host_sets,
      phmap::flat_hash_set<wf4::GlobalNodeID>(internal::INITIAL_SET_SIZE));
  galois::gstl::Vector<internal::PartialReachableSet> partial_sets(host_sets);
  galois::gstl::Vector<galois::PerThreadVector<internal::PartialReachableSet>>
      remote_partial_sets(num_hosts);
  bool finished = false;

  std::cout << "Started generating seed nodes" << std::endl;
  galois::do_all(
      galois::iterate(uint64_t(0), host_sets),
      [&](uint64_t i) {
        internal::GenerateSeedNodes(partial_sets, graph, seed, i, host);
      },
      galois::loopname("GenerateSeedNodes"));
  seed_timer.stop();
  std::cout << "Finished generating seed nodes, time: "
            << seed_timer.get_usec() / 1000000.0f << "s" << std::endl;

  for (uint64_t epoch = 0; epoch < epochs; epoch++) {
    if (partial_sets.size() > 0) {
      std::cout << "Epoch: " << epoch
                << ", Partial Sets: " << partial_sets.size() << std::endl;
    }
    internal::GenerateRandomReversibleReachableSets(
        reachable_sets, graph, std::move(partial_sets), remote_partial_sets,
        finished);
    partial_sets = internal::ExchangePartialSets(remote_partial_sets);
  }

  // clean up logic, any reachable sets not run to completion are sent back to
  // their owner host where it is added as-is to reachable_sets
  finished = true;
  internal::GenerateRandomReversibleReachableSets(
      reachable_sets, graph, std::move(partial_sets), remote_partial_sets,
      finished);
  partial_sets = internal::ExchangePartialSets(remote_partial_sets);
  internal::GenerateRandomReversibleReachableSets(
      reachable_sets, graph, std::move(partial_sets), remote_partial_sets,
      finished);

  timer.stop();
  std::cout << "RRR Sets generated, time: " << timer.get_usec() / 1000000.0f
            << "s" << std::endl;
  galois::runtime::getHostBarrier().wait();

  return reachable_sets;
}

std::vector<wf4::GlobalNodeID> wf4::GetInfluentialNodes(
    wf4::NetworkGraph& graph, wf4::ReverseReachableSet&& reachability_sets,
    uint64_t num_nodes, uint64_t influential_node_threshold) {
  galois::Timer timer;
  timer.start();
  auto& net = galois::runtime::getSystemNetworkInterface();
  galois::gstl::Vector<phmap::parallel_flat_hash_map_m<GlobalNodeID, uint64_t>>
      remote_updates(net.Num);
  std::vector<GlobalNodeID> influential_nodes;
  GlobalNodeID influential_node = internal::GetMostInfluentialNode(graph);
  influential_nodes.emplace_back(influential_node);

  for (uint64_t i = 0; i < num_nodes - 1; i++) {
    galois::do_all(
        galois::iterate(uint64_t(0), reachability_sets.size()),
        [&](uint64_t set) {
          internal::RemoveReachableSetsWithInfluentialNode(
              graph, reachability_sets[set], influential_node, remote_updates);
        },
        galois::loopname("RemoveReachableSetsWithInfluentialNode"));
    internal::RemoveNodesFromRemotes(graph, remote_updates);

    influential_node = internal::GetMostInfluentialNode(graph);
    influential_nodes.emplace_back(influential_node);
  }
  // TODO(Patrick) remove debug statement
  uint64_t uninfluenced_sets = 0;
  for (uint64_t i = 0; i < reachability_sets.size(); i++) {
    if (reachability_sets[i].size() > 0) {
      uninfluenced_sets += 1;
    }
  }
  timer.stop();
  std::cout << "Remaining uninfluenced sets: " << uninfluenced_sets
            << ", time: " << timer.get_usec() / 1000000.0f << "s" << std::endl;
  verifyInfluentialNodesDebug(graph, influential_nodes,
                              influential_node_threshold);
  return influential_nodes;
}

wf4::internal::PartialReachableSet::PartialReachableSet(uint64_t id_,
                                                        uint32_t host_,
                                                        wf4::GlobalNodeID root_)
    : id(id_), owner_host(host_),
      partial_reachable_set(
          phmap::flat_hash_set<wf4::GlobalNodeID>(INITIAL_SET_SIZE)),
      frontier(phmap::flat_hash_set<wf4::GlobalNodeID>(INITIAL_SET_SIZE)) {
  partial_reachable_set.emplace(root_);
  frontier.emplace(root_);
}

uint32_t wf4::internal::PartialReachableSet::NextHost(wf4::NetworkGraph& graph,
                                                      bool finish) {
  if (finish || Finished()) {
    return owner_host;
  }
  return graph.getHostID(*frontier.begin());
}

void wf4::internal::PartialReachableSet::serialize(
    galois::runtime::SerializeBuffer& buf) {
  uint64_t length =
      1 + 1 + 1 + 1 + partial_reachable_set.size() + frontier.size();
  std::vector<uint64_t> data(length);
  uint64_t offset = 0;
  data[offset++]  = id;
  data[offset++]  = owner_host;
  data[offset++]  = partial_reachable_set.size();
  data[offset++]  = frontier.size();
  for (wf4::GlobalNodeID gid : partial_reachable_set) {
    data[offset++] = gid;
  }
  for (wf4::GlobalNodeID gid : frontier) {
    data[offset++] = gid;
  }
  buf.insert(std::reinterpret_cast<uint8_t*>(&data[0]),
             length * sizeof(uint64_t));
}

void wf4::internal::PartialReachableSet::deserialize(
    galois::runtime::DeSerializeBuffer& buf) {
  size_t initial_offset = buf.getOffset();
  uint64_t* data =
      std::reinterpret_cast<uint64_t*>(&(buf.getVec().data()[initial_offset]));
  uint64_t offset         = 0;
  id                      = data[offset++];
  owner_host              = data[offset++];
  uint64_t reachable_size = data[offset++];
  uint64_t frontier_size  = data[offset++];
  for (uint64_t i = 0; i < reachable_size; i++) {
    partial_reachable_set.emplace(data[offset++]);
  }
  for (uint64_t i = 0; i < frontier_size; i++) {
    frontier.emplace(data[offset++]);
  }
  buf.setOffset(initial_offset + (offset * sizeof(uint64_t)));
}

void wf4::internal::FillNodeValues(wf4::NetworkGraph& graph,
                                   const wf4::NetworkGraph::GraphNode& node) {
  for (const auto& node_edge : graph.edges(node)) {
    const wf4::NetworkEdge& edge = graph.getEdgeData(node_edge);
    graph.getData(node).id       = edge.src;
    graph.getData(graph.getEdgeDst(node_edge)).id = edge.dst;

    if (edge.type == gls::wmd::TYPES::SALE) {
      galois::atomicAdd(graph.getData(node).sold_, edge.amount_);
    } else if (edge.type == gls::wmd::TYPES::PURCHASE) {
      galois::atomicAdd(graph.getData(node).bought_, edge.amount_);
      graph.getData(node).desired_ += edge.amount_;
    }
  }
  if (graph.getData(node).sold_ > 0) {
    bitset_sold_.set(node);
  }
}

void wf4::internal::CalculateEdgeProbability(
    wf4::NetworkGraph& graph, const wf4::NetworkGraph::GraphNode& node,
    galois::DGAccumulator<double>& total_edge_weights,
    galois::DGAccumulator<double>& total_sales) {
  wf4::NetworkNode& node_data = graph.getData(node);

  auto& net            = galois::runtime::getSystemNetworkInterface();
  double amount_sold   = node_data.sold_;
  double amount_bought = node_data.bought_;
  for (const auto& node_edge : graph.edges(node)) {
    auto& edge = graph.getEdgeData(node_edge);
    total_sales += edge.amount_;
    if (edge.type == gls::wmd::TYPES::SALE && amount_sold > 0) {
      edge.weight_ = edge.amount_ / amount_sold;
      total_edge_weights += edge.weight_;
    } else if (edge.type == gls::wmd::TYPES::PURCHASE) {
      auto dst_sold = graph.getData(graph.getEdgeDst(node_edge)).sold_;
      if (dst_sold > 0) {
        edge.weight_ = edge.amount_ / dst_sold;
        total_edge_weights += edge.weight_;
      }
    }
  }
}

void wf4::internal::GenerateSeedNodes(
    galois::gstl::Vector<wf4::internal::PartialReachableSet>& seed_nodes,
    wf4::NetworkGraph& graph, uint64_t seed, uint64_t id, uint64_t host) {
  pcg64 generator = pcg64(seed, (1 + host) * id);
  std::uniform_int_distribution<size_t> dist_nodes =
      std::uniform_int_distribution<size_t>(0, graph.numMasters() - 1);
  seed_nodes[id] =
      PartialReachableSet(id, host, graph.getGID(dist_nodes(generator)));
}

void wf4::internal::GenerateRandomReversibleReachableSets(
    wf4::ReverseReachableSet& reachability_sets, wf4::NetworkGraph& graph,
    galois::gstl::Vector<wf4::internal::PartialReachableSet>&& partial_sets,
    galois::gstl::Vector<
        galois::PerThreadVector<wf4::internal::PartialReachableSet>>&
        remote_partial_sets,
    bool finish) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  galois::do_all(
      galois::iterate(uint64_t(0), partial_sets.size()),
      [&](uint64_t i) {
        PartialReachableSet& partial_set = partial_sets[i];
        ContinueRandomReversibleReachableSet(graph, partial_set, i, finish);
        if (partial_set.owner_host == net.ID &&
            (finish || partial_set.Finished())) {
          reachability_sets[partial_set.id] =
              std::move(partial_set.partial_reachable_set);
        } else {
          remote_partial_sets[partial_set.NextHost(graph, finish)]
              .get()
              .emplace_back(partial_set);
        }
      },
      galois::loopname("GenerateRandomReversibleReachableSets"));
}

void wf4::internal::ContinueRandomReversibleReachableSet(
    wf4::NetworkGraph& graph, wf4::internal::PartialReachableSet& partial_set,
    uint64_t random_noise, bool finish) {
  std::uniform_real_distribution<double> dist_bfs =
      std::uniform_real_distribution<double>(0, 1);
  pcg64 generator = pcg64(partial_set.id, random_noise);

  // do gymnastics since we cannot iterate and mutate the parallel hashmap
  std::queue<wf4::GlobalNodeID> local_frontier;
  phmap::flat_hash_set<wf4::GlobalNodeID> remote_frontier(INITIAL_SET_SIZE);
  for (wf4::GlobalNodeID gid : partial_set.frontier) {
    if (graph.isOwned(gid)) {
      local_frontier.emplace(gid);
    } else {
      remote_frontier.emplace(gid);
    }
  }
  while (!local_frontier.empty()) {
    wf4::GlobalNodeID gid = local_frontier.front();
    local_frontier.pop();
    wf4::NetworkGraph::GraphNode lid = graph.getLID(gid);

    galois::atomicAdd(graph.getData(lid).frequency_, (uint64_t)1);
    if (finish) {
      continue;
    }
    for (auto& edge : graph.edges(lid)) {
      if (dist_bfs(generator) <= graph.getEdgeData(edge).weight_) {
        uint64_t reachable_node_gid = graph.getGID(graph.getEdgeDst(edge));
        if (partial_set.partial_reachable_set.find(reachable_node_gid) ==
            partial_set.partial_reachable_set.end()) {
          partial_set.partial_reachable_set.emplace(reachable_node_gid);
          if (graph.isOwned(reachable_node_gid)) {
            local_frontier.emplace(reachable_node_gid);
          } else {
            remote_frontier.emplace(reachable_node_gid);
          }
        }
      }
    }
  }
  // remove dangling frontier nodes if traversal is forcefully terminated
  if (finish) {
    for (wf4::GlobalNodeID gid : remote_frontier) {
      partial_set.partial_reachable_set.erase(gid);
    }
    remote_frontier.clear();
  }
  partial_set.frontier = std::move(remote_frontier);
}

wf4::GlobalNodeID
wf4::internal::GetMostInfluentialNode(wf4::NetworkGraph& graph) {
  auto& net        = galois::runtime::getSystemNetworkInterface();
  uint32_t threads = galois::getActiveThreads();
  galois::PerThreadVector<LocalMaxNode> most_influential_nodes;
  galois::DGAccumulator<uint64_t> total_influence;
  total_influence.reset();

  galois::do_all(
      galois::iterate(graph.masterNodesRange().begin(),
                      graph.masterNodesRange().end()),
      [&](wf4::NetworkGraph::GraphNode node) {
        FindLocalMaxNode(graph, node, most_influential_nodes.get(),
                         total_influence);
      },
      galois::loopname("FindMostInfluentialNode"));

  LocalMaxNode host_max                = GetMaxNode(most_influential_nodes);
  std::vector<LocalMaxNode> host_maxes = ExchangeMostInfluentialNodes(host_max);
  LocalMaxNode global_max              = GetMaxNode(host_maxes);
  wf4::GlobalNodeID most_influential_node = global_max.max_node;

  total_influence.reduce();
  if (graph.isOwned(most_influential_node)) {
    auto node_lid = graph.getLID(most_influential_node);
    std::cout << "Most influential node " << EXCHANGE_INFLUENTIAL_NODES - 9801
              << " on " << net.ID << ": " << graph.getData(node_lid).id
              << ", Occurred: " << global_max.max_influence << ", Degree: "
              << std::distance(graph.edge_begin(node_lid),
                               graph.edge_end(node_lid))
              << ", Bought: " << graph.getData(node_lid).bought_
              << ", Sold: " << graph.getData(node_lid).sold_
              << ", Total Influence in Graph: " << total_influence.read()
              << std::endl;
  }
  return most_influential_node;
}

wf4::internal::LocalMaxNode
wf4::internal::GetMaxNode(galois::PerThreadVector<wf4::internal::LocalMaxNode>&
                              most_influential_nodes) {
  LocalMaxNode most_influential_node;
  for (auto iter = most_influential_nodes.begin_all();
       iter != most_influential_nodes.end_all(); iter++) {
    const LocalMaxNode& influential_node = *iter;
    if (influential_node.max_influence > most_influential_node.max_influence) {
      most_influential_node.max_node      = influential_node.max_node;
      most_influential_node.max_influence = influential_node.max_influence;
    }
  }
  return most_influential_node;
}

wf4::internal::LocalMaxNode wf4::internal::GetMaxNode(
    std::vector<wf4::internal::LocalMaxNode>& most_influential_nodes) {
  LocalMaxNode most_influential_node;
  for (const LocalMaxNode& influential_node : most_influential_nodes) {
    if (influential_node.max_influence > most_influential_node.max_influence) {
      most_influential_node.max_node      = influential_node.max_node;
      most_influential_node.max_influence = influential_node.max_influence;
    }
  }
  return most_influential_node;
}

void wf4::internal::RemoveReachableSetsWithInfluentialNode(
    wf4::NetworkGraph& graph,
    phmap::flat_hash_set<wf4::GlobalNodeID>& reachability_set,
    const wf4::GlobalNodeID& influential_node,
    galois::gstl::Vector<
        phmap::parallel_flat_hash_map_m<wf4::GlobalNodeID, uint64_t>>&
        remote_updates) {
  using map = phmap::parallel_flat_hash_map_m<wf4::GlobalNodeID, uint64_t>;
  if (reachability_set.find(influential_node) != reachability_set.end()) {
    for (wf4::GlobalNodeID reachable_node_gid : reachability_set) {
      if (graph.isOwned(reachable_node_gid)) {
        galois::atomicSubtract(
            graph.getData(graph.getLID(reachable_node_gid)).frequency_,
            (uint64_t)1);
      } else {
        remote_updates[graph.getHostID(reachable_node_gid)].try_emplace_l(
            reachable_node_gid, [&](map::value_type& p) { p.second++; },
            (uint64_t)0); // may need to change to 1
      }
    }
    reachability_set.clear();
  }
}

void wf4::internal::RemoveNodesFromRemotes(
    wf4::NetworkGraph& graph,
    galois::gstl::Vector<
        phmap::parallel_flat_hash_map_m<wf4::GlobalNodeID, uint64_t>>&
        remote_updates) {
  auto& net          = galois::runtime::getSystemNetworkInterface();
  uint32_t host      = net.ID;
  uint32_t num_hosts = net.Num;
  // TODO(Patrick): ensure efficient serialization of phmap maps

  std::vector<galois::runtime::DeSerializeBuffer> nodes_to_remove(num_hosts);

  // send partial sets to other hosts
  for (uint32_t h = 0; h < num_hosts; h++) {
    if (h == host) {
      continue;
    }
    galois::runtime::SendBuffer send_buffer;
    SerializeMap(send_buffer, remote_updates[h]);
    if (send_buffer.size() == 0) {
      send_buffer.push('a');
    }
    net.sendTagged(h, REMOVE_RRR_SETS, send_buffer);
  }

  // recv node range from other hosts
  for (uint32_t h = 0; h < num_hosts - 1; h++) {
    decltype(net.recieveTagged(REMOVE_RRR_SETS, nullptr)) p;
    do {
      p = net.recieveTagged(REMOVE_RRR_SETS, nullptr);
    } while (!p);
    uint32_t sending_host         = p->first;
    nodes_to_remove[sending_host] = std::move(p->second);
  }
  REMOVE_RRR_SETS++;

  for (galois::runtime::DeSerializeBuffer& buf : nodes_to_remove) {
    if (buf.size() < 16) {
      continue;
    }
    size_t initial_offset = buf.getOffset();
    uint64_t* data        = std::reinterpret_cast<uint64_t*>(
        &(buf.getVec().data()[initial_offset]));
    uint64_t size = data[0];
    data          = &data[1];
    galois::do_all(
        galois::iterate((uint64_t)0, size),
        [&](uint64_t i) {
          galois::atomicSubtract(
              graph.getData(graph.getLID(data[i * 2])).frequency_,
              data[i * 2 + 1]);
        },
        galois::loopname("RemoveNodesFromRemotes"));
  }
  galois::runtime::getHostBarrier().wait();
}

void wf4::internal::FindLocalMaxNode(
    wf4::NetworkGraph& graph, wf4::NetworkGraph::GraphNode node,
    galois::gstl::Vector<wf4::internal::LocalMaxNode>& local_max_node,
    galois::DGAccumulator<uint64_t>& total_influence) {
  const wf4::NetworkNode& node_data = graph.getData(node);
  total_influence += node_data.frequency_;
  if (local_max_node.size() == 0) {
    local_max_node.emplace_back(
        LocalMaxNode(graph.getGID(node), node_data.frequency_));
  } else if (node_data.frequency_ > local_max_node[0].max_influence) {
    local_max_node[0].max_node      = graph.getGID(node);
    local_max_node[0].max_influence = node_data.frequency_;
  }
}

galois::gstl::Vector<wf4::internal::PartialReachableSet>
wf4::internal::ExchangePartialSets(
    galois::gstl::Vector<
        galois::PerThreadVector<wf4::internal::PartialReachableSet>>&
        remote_partial_sets) {
  galois::gstl::Vector<PartialReachableSet> local_partial_sets;
  auto& net          = galois::runtime::getSystemNetworkInterface();
  uint32_t host      = net.ID;
  uint32_t num_hosts = net.Num;
  // TODO(Patrick): ensure efficient serialization of phmap sets

  std::vector<galois::runtime::DeSerializeBuffer> host_partial_sets(num_hosts);

  // send partial sets to other hosts
  for (uint32_t h = 0; h < num_hosts; h++) {
    if (h == host) {
      continue;
    }
    galois::runtime::SendBuffer send_buffer;
    for (auto iter = remote_partial_sets[h].begin_all();
         iter != remote_partial_sets[h].end_all(); iter++) {
      (*iter).serialize(send_buffer);
    }
    if (send_buffer.size() == 0) {
      send_buffer.push('a');
    }
    net.sendTagged(h, EXCHANGE_PARTIAL_SETS, send_buffer);
  }

  // recv node range from other hosts
  for (uint32_t h = 0; h < num_hosts - 1; h++) {
    decltype(net.recieveTagged(EXCHANGE_PARTIAL_SETS, nullptr)) p;
    do {
      p = net.recieveTagged(EXCHANGE_PARTIAL_SETS, nullptr);
    } while (!p);
    uint32_t sending_host           = p->first;
    host_partial_sets[sending_host] = std::move(p->second);
  }
  EXCHANGE_PARTIAL_SETS++;

  for (galois::runtime::DeSerializeBuffer& buf : host_partial_sets) {
    if (buf.size() < 16) {
      continue;
    }
    while (buf.getOffset() < buf.size()) {
      PartialReachableSet set;
      set.deserialize(buf);
      local_partial_sets.emplace_back(set);
    }
  }
  for (galois::PerThreadVector<PartialReachableSet>& remote_sets :
       remote_partial_sets) {
    remote_sets.clear_all_parallel();
  }
  galois::runtime::getHostBarrier().wait();

  return local_partial_sets;
}

std::vector<wf4::internal::LocalMaxNode>
wf4::internal::ExchangeMostInfluentialNodes(
    const wf4::internal::LocalMaxNode& local_max) {
  auto& net          = galois::runtime::getSystemNetworkInterface();
  uint32_t host      = net.ID;
  uint32_t num_hosts = net.Num;
  std::vector<LocalMaxNode> host_maxes(num_hosts);
  host_maxes[host] = local_max;

  // send local max to other hosts
  for (uint32_t h = 0; h < num_hosts; h++) {
    if (h == host) {
      continue;
    }
    galois::runtime::SendBuffer send_buffer;
    galois::runtime::gSerialize(send_buffer, local_max);
    net.sendTagged(h, EXCHANGE_INFLUENTIAL_NODES, send_buffer);
  }

  // recv node range from other hosts
  for (uint32_t h = 0; h < num_hosts - 1; h++) {
    decltype(net.recieveTagged(EXCHANGE_INFLUENTIAL_NODES, nullptr)) p;
    do {
      p = net.recieveTagged(EXCHANGE_INFLUENTIAL_NODES, nullptr);
    } while (!p);
    uint32_t sending_host = p->first;

    galois::runtime::gDeserialize(p->second, host_maxes[sending_host]);
  }
  galois::runtime::getHostBarrier().wait();
  EXCHANGE_INFLUENTIAL_NODES++;

  return host_maxes;
}

void wf4::internal::SerializeMap(
    galois::runtime::SendBuffer& buf,
    phmap::parallel_flat_hash_map_m<wf4::GlobalNodeID, uint64_t>& updates) {
  uint64_t length = 1 + updates.size() * 2;
  uint64_t data[length];
  uint64_t offset = 0;
  data[offset++]  = updates.size();
  for (const std::pair<wf4::GlobalNodeID, uint64_t>& update : updates) {
    data[offset++] = update.first;
    data[offset++] = update.second;
  }
  buf.insert(std::reinterpret_cast<uint8_t*>(&data), length * sizeof(uint64_t));
  updates.clear();
}
