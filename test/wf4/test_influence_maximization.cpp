// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

// SPDX-License-Identifier: MIT
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include <wf4/influence_maximization.hpp>

galois::DynamicBitSet bitset_bought_;
galois::DynamicBitSet bitset_sold_;

namespace {

const uint64_t num_nodes = 16;
const uint64_t num_edges = 240;
const uint64_t seed      = 9801;
const uint64_t epochs    = 10;

std::unique_ptr<wf4::NetworkGraph>
generateTestGraph(bool set_node_properties = false, bool single_host = false) {
  std::vector<wf4::NetworkNode> vertices(num_nodes);
  std::vector<galois::GenericEdge<wf4::NetworkEdge>> edges(num_edges);

  for (uint64_t i = 0; i < num_nodes; i++) {
    wf4::NetworkNode node(i);
    if (set_node_properties) {
      node.sold_      = i * i;
      node.bought_    = (num_nodes + i) * (num_nodes - (i + 1)) / 2;
      node.desired_   = node.bought_;
      node.frequency_ = i;
    }
    vertices[i] = node;
  }
  // Every node sells to every node with a global ID less than itself
  // And so every node buys from every node with a global ID more than itself
  // We set the edge weight to be equal to the global ID of the seller
  // Vertex 0 does not sell anything
  uint64_t edge_count = 0;
  for (uint64_t src = 0; src < num_nodes; src++) {
    for (uint64_t dst = 0; dst < src; dst++) {
      edges[edge_count++] = galois::GenericEdge(
          src, dst,
          wf4::NetworkEdge(src, dst, src, agile::workflow1::TYPES::SALE));
    }
    for (uint64_t dst = src + 1; dst < num_nodes; dst++) {
      edges[edge_count++] = galois::GenericEdge(
          src, dst,
          wf4::NetworkEdge(src, dst, dst, agile::workflow1::TYPES::PURCHASE));
    }
  }
  auto& net      = galois::runtime::getSystemNetworkInterface();
  uint32_t host  = net.ID;
  uint32_t hosts = net.Num;
  if (single_host) {
    host  = 0;
    hosts = 1;
  }
  return std::unique_ptr<wf4::NetworkGraph>(
      new wf4::NetworkGraph(host, hosts, vertices, edges));
}

double AmountSold(uint64_t num_nodes) {
  double sold = 0;
  for (uint64_t i = 0; i < num_nodes; i++) {
    sold += i * i;
  }
  return sold;
}

uint64_t getHostSets(uint64_t global_sets) {
  auto& net          = galois::runtime::getSystemNetworkInterface();
  uint32_t host      = net.ID;
  uint32_t num_hosts = net.Num;
  uint64_t host_sets = global_sets / num_hosts;
  if (host == num_hosts - 1) {
    host_sets = global_sets - ((num_hosts - 1) * (global_sets / num_hosts));
  }
  return host_sets;
}

std::vector<uint64_t> getRootCounts(wf4::NetworkGraph& graph,
                                    uint64_t host_sets) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::vector<uint64_t> root_counts(graph.globalSize());
  for (uint64_t i = 0; i < host_sets; i++) {
    galois::gstl::Vector<wf4::internal::PartialReachableSet> seed_nodes(
        host_sets);
    wf4::internal::GenerateSeedNodes(seed_nodes, graph, seed, i, net.ID);
    root_counts[*(seed_nodes[i].partial_reachable_set.begin())]++;
  }
  return root_counts;
}

} // namespace

TEST(IF, INIT) {
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  EXPECT_EQ(graph.size(), num_nodes);
  galois::DGAccumulator<uint64_t> global_nodes;
  galois::DGAccumulator<uint64_t> global_edges;
  global_nodes.reset();
  global_edges.reset();

  galois::do_all(galois::iterate(graph.masterNodesRange()),
                 [&](wf4::NetworkGraph::GraphNode node) {
                   bool local = graph.isLocal(graph.getGID(node));
                   EXPECT_EQ(local, true);
                   if (!local) {
                     return;
                   }
                   global_nodes += 1;
                   auto actualEdges = graph.edges(node).begin();
                   uint64_t node_edges =
                       std::distance(actualEdges, graph.edges(node).end());
                   EXPECT_EQ(node_edges, num_nodes - 1);

                   for (auto& edge : graph.edges(node)) {
                     wf4::NetworkEdge edge_value = graph.getEdgeData(edge);
                     wf4::GlobalNodeID token_id  = graph.getGID(node);
                     EXPECT_EQ(edge_value.type == agile::workflow1::TYPES::SALE,
                               graph.getGID(graph.getEdgeDst(edge)) < token_id);
                     EXPECT_EQ(edge_value.type ==
                                   agile::workflow1::TYPES::PURCHASE,
                               graph.getGID(graph.getEdgeDst(edge)) > token_id);
                     global_edges += 1;
                   }
                 });
  EXPECT_EQ(global_nodes.reduce(), num_nodes);
  EXPECT_EQ(global_edges.reduce(), num_edges);
}

TEST(IF, FillNodeValues) {
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  bitset_sold_.resize(graph.size());
  bitset_sold_.reset();
  galois::do_all(galois::iterate(graph.masterNodesRange()),
                 [&](wf4::NetworkGraph::GraphNode node_lid) {
                   wf4::internal::FillNodeValues(graph, node_lid);
                   wf4::NetworkNode node_data = graph.getData(node_lid);
                   wf4::GlobalNodeID node_gid = graph.getGID(node_lid);
                   EXPECT_EQ(node_data.sold_, node_gid * node_gid);
                   EXPECT_EQ(node_data.bought_,
                             (num_nodes + node_gid) *
                                 (num_nodes - (node_gid + 1)) / 2);
                   EXPECT_EQ(node_data.bought_, node_data.desired_);
                 });
}

TEST(IF, EdgeProbability) {
  const bool set_node_properties = true;
  std::unique_ptr<wf4::NetworkGraph> graph_ptr =
      generateTestGraph(set_node_properties);
  wf4::NetworkGraph& graph = *graph_ptr;
  galois::DGAccumulator<double> total_edge_weights;
  galois::DGAccumulator<double> total_sales;
  total_edge_weights.reset();
  total_sales.reset();
  galois::do_all(
      galois::iterate(graph.masterNodesRange()),
      [&](wf4::NetworkGraph::GraphNode node_lid) {
        wf4::internal::CalculateEdgeProbability(
            graph, node_lid, total_edge_weights, total_sales);
        for (auto& edge : graph.edges(node_lid)) {
          wf4::NetworkEdge edge_value = graph.getEdgeData(edge);
          if (edge_value.type == agile::workflow1::TYPES::SALE) {
            EXPECT_FLOAT_EQ(edge_value.weight_, 1.0 / graph.getGID(node_lid));
          } else {
            EXPECT_FLOAT_EQ(edge_value.weight_,
                            1.0 / graph.getGID(graph.getEdgeDst(edge)));
          }
        }
      });
  EXPECT_FLOAT_EQ(total_edge_weights.reduce(), (num_nodes - 1) * 2);
  EXPECT_FLOAT_EQ(total_sales.reduce(), AmountSold(num_nodes) * 2);
}

TEST(IF, EdgeProbabilities) {
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      sync_substrate =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              graph, net.ID, net.Num, graph.isTransposed(),
              graph.cartesianGrid());
  wf4::CalculateEdgeProbabilities(graph, *sync_substrate);
  galois::do_all(galois::iterate(graph.masterNodesRange()),
                 [&](wf4::NetworkGraph::GraphNode node_lid) {
                   wf4::NetworkNode node_data = graph.getData(node_lid);
                   wf4::GlobalNodeID token_id = graph.getGID(node_lid);
                   EXPECT_EQ(node_data.sold_, token_id * token_id);
                   EXPECT_EQ(node_data.bought_,
                             (num_nodes + token_id) *
                                 (num_nodes - (token_id + 1)) / 2);
                   EXPECT_EQ(node_data.bought_, node_data.desired_);
                 });
  galois::do_all(
      galois::iterate(graph.masterNodesRange()),
      [&](wf4::NetworkGraph::GraphNode node_lid) {
        for (auto& edge : graph.edges(node_lid)) {
          wf4::NetworkEdge edge_value = graph.getEdgeData(edge);
          if (edge_value.type == agile::workflow1::TYPES::SALE) {
            EXPECT_FLOAT_EQ(edge_value.weight_, 1.0 / graph.getGID(node_lid));
          } else {
            EXPECT_FLOAT_EQ(edge_value.weight_,
                            1.0 / graph.getGID(graph.getEdgeDst(edge)));
          }
        }
      });
}

TEST(IF, GenerateRRRSet) {
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      sync_substrate =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              graph, net.ID, net.Num, graph.isTransposed(),
              graph.cartesianGrid());
  wf4::CalculateEdgeProbabilities(graph, *sync_substrate);
  wf4::ReverseReachableSet rrr_sets =
      wf4::GetRandomReverseReachableSets(graph, net.Num, seed, epochs);

  galois::gstl::Vector<wf4::internal::PartialReachableSet> seed_nodes(1);
  wf4::internal::GenerateSeedNodes(seed_nodes, graph, seed, 0, net.ID);
  wf4::GlobalNodeID root = *(seed_nodes[0].partial_reachable_set.begin());

  EXPECT_EQ(rrr_sets.size(), 1);
  for (phmap::flat_hash_set<wf4::GlobalNodeID> rrr_set : rrr_sets) {
    EXPECT_GT(rrr_set.size(), 0);
  }
  wf4::NetworkNode root_data = graph.getData(graph.getLID(root));
  uint64_t root_influence    = root_data.frequency_;
  EXPECT_GE(root_influence, 1);
  EXPECT_LE(root_influence, net.Num);
}

TEST(IF, GenerateRRRSets) {
  const uint64_t num_sets                      = 100;
  const uint64_t host_sets                     = getHostSets(num_sets);
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      sync_substrate =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              graph, net.ID, net.Num, graph.isTransposed(),
              graph.cartesianGrid());
  wf4::CalculateEdgeProbabilities(graph, *sync_substrate);
  wf4::ReverseReachableSet rrr_sets =
      wf4::GetRandomReverseReachableSets(graph, num_sets, seed, epochs);
  std::vector<uint64_t> root_counts = getRootCounts(graph, host_sets);

  EXPECT_EQ(rrr_sets.size(), host_sets);
  for (phmap::flat_hash_set<wf4::GlobalNodeID> rrr_set : rrr_sets) {
    EXPECT_GT(rrr_set.size(), 0);
  }

  galois::DGAccumulator<double> has_greater;
  has_greater.reset();
  for (uint64_t i = 0; i < num_nodes; i++) {
    if (!graph.isOwned(i)) {
      continue;
    }
    wf4::NetworkNode node_data = graph.getData(graph.getLID(i));
    uint64_t influence         = node_data.frequency_;
    uint64_t root_occurrences  = root_counts[i];
    EXPECT_GE(influence, root_occurrences);
    if (influence > root_occurrences) {
      has_greater += 1;
    }
  }
  EXPECT_GT(has_greater.reduce(), 0);
}

TEST(IF, FindLocalMax) {
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph(true);
  wf4::NetworkGraph& graph                     = *graph_ptr;
  galois::PerThreadVector<wf4::internal::LocalMaxNode> max_array;
  galois::DGAccumulator<uint64_t> total_influence;
  total_influence.reset();

  galois::do_all(galois::iterate(graph.masterNodesRange()),
                 [&](wf4::NetworkGraph::GraphNode node) {
                   wf4::internal::FindLocalMaxNode(graph, node, max_array.get(),
                                                   total_influence);
                 });
  EXPECT_EQ(total_influence.reduce(), num_edges / 2);
  EXPECT_GT(max_array.size_all(), 0);
}

TEST(IF, GetMaxNode) {
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph(true);
  wf4::NetworkGraph& graph                     = *graph_ptr;
  EXPECT_EQ(wf4::internal::GetMostInfluentialNode(graph), num_nodes - 1);
}

TEST(IF, GetInfluential) {
  const uint64_t num_sets                      = 100;
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      sync_substrate =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              graph, net.ID, net.Num, graph.isTransposed(),
              graph.cartesianGrid());
  wf4::CalculateEdgeProbabilities(graph, *sync_substrate);
  wf4::ReverseReachableSet rrr_sets =
      wf4::GetRandomReverseReachableSets(graph, num_sets, seed, epochs);
  std::vector<wf4::GlobalNodeID> influential_nodes =
      wf4::GetInfluentialNodes(graph, std::move(rrr_sets), 1, 0);
  EXPECT_EQ(influential_nodes.size(), 1);

  wf4::GlobalNodeID influential = influential_nodes[0];
  galois::DGAccumulator<uint64_t> most_influence_accum;
  most_influence_accum.reset();
  if (graph.isOwned(influential)) {
    wf4::NetworkNode most_influential =
        graph.getData(graph.getLID(influential));
    uint64_t most_influence = most_influential.frequency_;
    EXPECT_GT(most_influence, 0);
    most_influence_accum += most_influence;
  }
  uint64_t most_influence = most_influence_accum.reduce();
  for (uint64_t node = 0; node < num_nodes; node++) {
    wf4::NetworkNode node_data = graph.getData(graph.getLID(node));
    uint64_t influence         = node_data.frequency_;
    EXPECT_GE(most_influence, influence);
  }
}

TEST(IF, GetInfluentials2) {
  const uint64_t num_sets                      = 100;
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      sync_substrate =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              graph, net.ID, net.Num, graph.isTransposed(),
              graph.cartesianGrid());
  wf4::CalculateEdgeProbabilities(graph, *sync_substrate);
  wf4::ReverseReachableSet rrr_sets =
      wf4::GetRandomReverseReachableSets(graph, num_sets, seed, epochs);
  std::vector<wf4::GlobalNodeID> influential_nodes =
      wf4::GetInfluentialNodes(graph, std::move(rrr_sets), 2, 0);
  EXPECT_EQ(influential_nodes.size(), 2);
  wf4::GlobalNodeID influential = influential_nodes[0];
  if (graph.isOwned(influential)) {
    wf4::NetworkNode most_influential =
        graph.getData(graph.getLID(influential));
    uint64_t most_influence = most_influential.frequency_;
    EXPECT_EQ(most_influence, 0);
  }
}

TEST(IF, GetInfluentials3) {
  const uint64_t num_sets                      = 100;
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph();
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      sync_substrate =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              graph, net.ID, net.Num, graph.isTransposed(),
              graph.cartesianGrid());
  wf4::CalculateEdgeProbabilities(graph, *sync_substrate);
  wf4::ReverseReachableSet rrr_sets =
      wf4::GetRandomReverseReachableSets(graph, num_sets, seed, epochs);
  std::vector<wf4::GlobalNodeID> influential_nodes =
      wf4::GetInfluentialNodes(graph, std::move(rrr_sets), 3, 0);
  EXPECT_EQ(influential_nodes.size(), 3);
  wf4::GlobalNodeID influential = influential_nodes[0];
  if (graph.isOwned(influential)) {
    wf4::NetworkNode most_influential =
        graph.getData(graph.getLID(influential));
    uint64_t most_influence = most_influential.frequency_;
    EXPECT_EQ(most_influence, 0);
  }
  for (uint64_t node = 0; node < num_nodes; node++) {
    if (!graph.isOwned(node)) {
      continue;
    }
    wf4::NetworkNode node_data = graph.getData(graph.getLID(node));
    uint64_t influence         = node_data.frequency_;
    EXPECT_LE(influence, 100);
  }
}
