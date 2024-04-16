// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

// SPDX-License-Identifier: MIT
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include <wf4/quiesce.hpp>

galois::DynamicBitSet bitset_bought_;
galois::DynamicBitSet bitset_sold_;

namespace {

const uint64_t num_nodes = 16;
const uint64_t num_edges = 240;
const bool async         = false;

std::unique_ptr<wf4::NetworkGraph>
generateTestGraph(bool remove_high = false, uint64_t nodes_to_set = num_nodes,
                  bool single_host = false) {
  std::vector<wf4::NetworkNode> vertices(num_nodes);
  std::vector<galois::GenericEdge<wf4::NetworkEdge>> edges(num_edges);

  for (uint64_t i = 0; i < num_nodes; i++) {
    wf4::NetworkNode node(i);
    if (remove_high) {
      if (i < nodes_to_set) {
        node.sold_    = i * i;
        node.bought_  = (nodes_to_set + i) * (nodes_to_set - (i + 1)) / 2;
        node.desired_ = (num_nodes + i) * (num_nodes - (i + 1)) / 2;
      } else {
        node.sold_    = 0;
        node.bought_  = 0;
        node.desired_ = 0;
      }
    } else {
      if (i <= nodes_to_set) {
        node.sold_    = 0;
        node.bought_  = 0;
        node.desired_ = 0;
      } else {
        node.sold_    = i * (i - nodes_to_set - 1);
        node.bought_  = (num_nodes + i) * (num_nodes - (i + 1)) / 2;
        node.desired_ = (num_nodes + i) * (num_nodes - (i + 1)) / 2;
      }
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

std::pair<std::unique_ptr<wf4::NetworkGraph>,
          std::unique_ptr<wf4::NetworkGraph>>
generateTestGraphs(std::vector<uint64_t> remove_nodes) {
  std::unique_ptr<wf4::NetworkGraph> graph1 = generateTestGraph(true);
  std::unique_ptr<wf4::NetworkGraph> graph2 =
      generateTestGraph(true, num_nodes, true);
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
      substrate2 =
          std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
              *graph2, 0, 1, graph2->isTransposed(), graph2->cartesianGrid());
  wf4::CancelNodes(*graph2, *substrate2, remove_nodes);

  return std::pair(std::move(graph1), std::move(graph2));
}

void nodePropertiesGreater(wf4::NetworkGraph& actual,
                           wf4::NetworkGraph& expected) {
  for (uint64_t i = 0; i < num_nodes; i++) {
    if (!actual.isOwned(i)) {
      continue;
    }
    wf4::NetworkNode actual_node   = actual.getData(actual.getLID(i));
    wf4::NetworkNode expected_node = expected.getData(expected.getLID(i));
    EXPECT_EQ(actual_node.id, expected_node.id);
    EXPECT_GE(actual_node.bought_, expected_node.bought_);
    EXPECT_GE(actual_node.sold_, expected_node.sold_);
    EXPECT_EQ(actual_node.desired_, expected_node.desired_);
  }
}

void nodePropertiesEqual(wf4::NetworkGraph& actual,
                         wf4::NetworkGraph& expected) {
  for (uint64_t i = 0; i < num_nodes; i++) {
    if (!actual.isOwned(i)) {
      continue;
    }
    wf4::NetworkNode actual_node   = actual.getData(actual.getLID(i));
    wf4::NetworkNode expected_node = expected.getData(expected.getLID(i));
    EXPECT_EQ(actual_node.id, expected_node.id);
    EXPECT_FLOAT_EQ(actual_node.bought_, expected_node.bought_);
    EXPECT_FLOAT_EQ(actual_node.sold_, expected_node.sold_);
    EXPECT_FLOAT_EQ(actual_node.desired_, expected_node.desired_);
  }
}

void nodePropertiesEqual(wf4::NetworkGraph& actual,
                         std::vector<wf4::GlobalNodeID>& removed_nodes) {
  std::unique_ptr<wf4::NetworkGraph> expected_ptr =
      generateTestGraph(true, num_nodes, true);
  wf4::NetworkGraph& expected = *expected_ptr;
  for (uint64_t i = 0; i < num_nodes; i++) {
    if (!actual.isOwned(i)) {
      continue;
    }
    wf4::NetworkNode actual_node   = actual.getData(actual.getLID(i));
    wf4::NetworkNode expected_node = expected.getData(expected.getLID(i));
    EXPECT_EQ(actual_node.id, expected_node.id);
    bool cancelled = false;
    for (wf4::GlobalNodeID node : removed_nodes) {
      if (i == node) {
        EXPECT_FLOAT_EQ(actual_node.bought_, 0);
        EXPECT_FLOAT_EQ(actual_node.sold_, 0);
        EXPECT_FLOAT_EQ(actual_node.desired_, 0);
        cancelled = true;
      }
    }
    if (!cancelled) {
      double sold_delta   = 0;
      double bought_delta = 0;
      for (wf4::GlobalNodeID node : removed_nodes) {
        if (node < i) {
          sold_delta -= i;
        } else {
          bought_delta -= node;
        }
      }
      EXPECT_FLOAT_EQ(actual_node.bought_,
                      expected_node.bought_ + bought_delta);
      EXPECT_FLOAT_EQ(actual_node.sold_, expected_node.sold_ + sold_delta);
      EXPECT_FLOAT_EQ(actual_node.desired_, expected_node.desired_);
    }
  }
}

} // namespace

TEST(Quiesce, Cancel) {
  const bool remove_high = true;
  {
    std::unique_ptr<wf4::NetworkGraph> graph_ptr =
        generateTestGraph(remove_high);
    wf4::NetworkGraph& graph = *graph_ptr;
    auto& net                = galois::runtime::getSystemNetworkInterface();
    std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
        substrate =
            std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
                graph, net.ID, net.Num, graph.isTransposed(),
                graph.cartesianGrid());

    for (uint64_t i = 0; i <= num_nodes; i++) {
      bitset_bought_.resize(graph.size());
      bitset_bought_.reset();
      bitset_sold_.resize(graph.size());
      bitset_sold_.reset();
      uint64_t node_id = num_nodes - i;
      galois::do_all(galois::iterate(graph.allNodesRange()),
                     [&](wf4::NetworkGraph::GraphNode node_lid) {
                       if (node_id == graph.getGID(node_lid)) {
                         wf4::internal::CancelNode(graph, node_id);
                       }
                     });
      // update mirrors
      substrate->sync<WriteLocation::writeAny, ReadLocation::readAny,
                      Reduce_add_bought_, Bitset_bought_, async>(
          "Update bought values after cancellation");
      substrate->sync<WriteLocation::writeAny, ReadLocation::readAny,
                      Reduce_add_sold_, Bitset_sold_, async>(
          "Update sold values after cancellation");
      galois::runtime::getHostBarrier().wait();
      for (uint64_t j = node_id; j < num_nodes; j++) {
        if (graph.isLocal(j)) {
          graph.getData(graph.getLID(j)).Cancel();
        }
      }

      std::unique_ptr<wf4::NetworkGraph> expected_graph =
          generateTestGraph(remove_high, num_nodes - i, true);
      nodePropertiesEqual(graph, *expected_graph);
    }
  }
  {
    std::unique_ptr<wf4::NetworkGraph> graph_ptr =
        generateTestGraph(remove_high);
    wf4::NetworkGraph& graph = *graph_ptr;
    auto& net                = galois::runtime::getSystemNetworkInterface();
    std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>
        substrate =
            std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
                graph, net.ID, net.Num, graph.isTransposed(),
                graph.cartesianGrid());

    for (uint64_t i = 0; i < num_nodes; i++) {
      bitset_bought_.resize(graph.size());
      bitset_bought_.reset();
      bitset_sold_.resize(graph.size());
      bitset_sold_.reset();
      uint64_t node_id = i;
      galois::do_all(galois::iterate(graph.allNodesRange()),
                     [&](wf4::NetworkGraph::GraphNode node_lid) {
                       if (node_id == graph.getGID(node_lid)) {
                         wf4::internal::CancelNode(graph, node_id);
                       }
                     });
      // update mirrors
      substrate->sync<WriteLocation::writeAny, ReadLocation::readAny,
                      Reduce_add_bought_, Bitset_bought_, async>(
          "Update bought values after cancellation");
      substrate->sync<WriteLocation::writeAny, ReadLocation::readAny,
                      Reduce_add_sold_, Bitset_sold_, async>(
          "Update sold values after cancellation");
      galois::runtime::getHostBarrier().wait();
      for (uint64_t j = 0; j <= node_id; j++) {
        if (graph.isLocal(j)) {
          graph.getData(graph.getLID(j)).Cancel();
        }
      }

      std::unique_ptr<wf4::NetworkGraph> expected_graph =
          generateTestGraph(!remove_high, i, true);
      nodePropertiesEqual(graph, *expected_graph);
    }
  }
}

TEST(Quiesce, CancelNodes) {
  const bool remove_high                       = true;
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph(remove_high);
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>> substrate =
      std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
          graph, net.ID, net.Num, graph.isTransposed(), graph.cartesianGrid());
  std::vector<wf4::GlobalNodeID> influential_vertices;
  influential_vertices.emplace_back(num_nodes - 1);
  influential_vertices.emplace_back(num_nodes / 2);

  wf4::CancelNodes(graph, *substrate, influential_vertices);
  nodePropertiesEqual(graph, influential_vertices);
}

TEST(Quiesce, TryQuiesceNode) {
  const bool remove_high                       = true;
  const uint64_t removed_node                  = num_nodes - 1;
  std::unique_ptr<wf4::NetworkGraph> graph_ptr = generateTestGraph(remove_high);
  wf4::NetworkGraph& graph                     = *graph_ptr;
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>> substrate =
      std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
          graph, net.ID, net.Num, graph.isTransposed(), graph.cartesianGrid());
  if (net.Num != 2 && net.Num != 4) {
    return;
  }
  const uint64_t test_node = num_nodes / net.Num - 1;

  galois::gstl::Vector<
      phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>>
      proposed_edges(net.Num);
  std::vector<wf4::GlobalNodeID> influential_vertices;
  influential_vertices.emplace_back(removed_node);

  wf4::CancelNodes(graph, *substrate, influential_vertices);
  galois::do_all(galois::iterate(graph.masterNodesRange()),
                 [&](wf4::NetworkGraph::GraphNode node) {
                   if (graph.getGID(node) == test_node) {
                     wf4::internal::TryQuiesceNode(graph, proposed_edges, node);
                   }
                 });
  if (graph.isOwned(test_node)) {
    bool proposing_edge = false;
    for (phmap::parallel_flat_hash_set_m<wf4::internal::ProposedEdge>& edges :
         proposed_edges) {
      for (wf4::internal::ProposedEdge res : edges) {
        EXPECT_EQ(res.src, test_node);
        EXPECT_GT(res.dst, test_node);
        EXPECT_LE(res.weight, removed_node);
        proposing_edge = true;
      }
    }
    EXPECT_TRUE(proposing_edge);
  }
}

// TODO(Patrick) implement
// TEST(Quiesce, TryAddEdges)

// TODO(Patrick) re-enable when no longer hanging in ci
TEST(Quiesce, DISABLED_QuiesceOne) {
  const uint64_t removed_node = 1;
  std::vector<wf4::GlobalNodeID> influential_vertices;
  influential_vertices.emplace_back(removed_node);
  auto graphs                 = generateTestGraphs(influential_vertices);
  wf4::NetworkGraph& graph    = *(graphs.first);
  wf4::NetworkGraph& expected = *(graphs.second);
  auto& net                   = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>> substrate =
      std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
          graph, net.ID, net.Num, graph.isTransposed(), graph.cartesianGrid());
  wf4::CancelNodes(graph, *substrate, influential_vertices);

  wf4::QuiesceGraph(graph, *substrate);

  double delta                  = 1.0;
  wf4::NetworkNode& buyer_node  = expected.getData(expected.getLID(0));
  wf4::NetworkNode& seller_node = expected.getData(expected.getLID(2));
  galois::atomicAdd(buyer_node.bought_, delta);
  galois::atomicAdd(seller_node.sold_, delta);

  nodePropertiesEqual(graph, expected);
}

TEST(Quiesce, DISABLED_Quiesce) {
  std::vector<wf4::GlobalNodeID> influential_vertices;
  influential_vertices.emplace_back(0);
  influential_vertices.emplace_back(2);
  influential_vertices.emplace_back(num_nodes / 2);
  influential_vertices.emplace_back(num_nodes - 1);
  auto graphs                 = generateTestGraphs(influential_vertices);
  wf4::NetworkGraph& graph    = *(graphs.first);
  wf4::NetworkGraph& expected = *(graphs.second);
  auto& net                   = galois::runtime::getSystemNetworkInterface();
  std::unique_ptr<galois::graphs::GluonSubstrate<wf4::NetworkGraph>> substrate =
      std::make_unique<galois::graphs::GluonSubstrate<wf4::NetworkGraph>>(
          graph, net.ID, net.Num, graph.isTransposed(), graph.cartesianGrid());
  wf4::CancelNodes(graph, *substrate, influential_vertices);

  wf4::QuiesceGraph(graph, *substrate);

  nodePropertiesGreater(graph, expected);
}
