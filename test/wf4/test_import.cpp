// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

// SPDX-License-Identifier: MIT
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include <gtest/gtest.h>

#include <wf4/import.hpp>

namespace {

const char* someFile = "some_file.csv";

void checkParsedEdge(galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                                          wf4::FullNetworkEdge>
                         result,
                     wf4::FullNetworkEdge expected,
                     agile::workflow1::TYPES expected_inverse,
                     uint64_t expected_num_edges) {
  EXPECT_EQ(result.isEdge, true);
  EXPECT_EQ(result.edges.size(), expected_num_edges);

  wf4::FullNetworkEdge edge0 = result.edges[0];

  EXPECT_EQ(edge0.src, expected.src);
  EXPECT_EQ(edge0.dst, expected.dst);
  EXPECT_EQ(edge0.type, expected.type);
  EXPECT_EQ(edge0.src_type, expected.src_type);
  EXPECT_EQ(edge0.dst_type, expected.dst_type);
  EXPECT_FLOAT_EQ(edge0.amount_, expected.amount_);
  EXPECT_EQ(edge0.topic, expected.topic);

  if (result.edges.size() >= 2) {
    wf4::FullNetworkEdge edge1 = result.edges[1];
    EXPECT_EQ(edge1.type, expected_inverse);
    EXPECT_NE(edge0.type, edge1.type);
    EXPECT_EQ(edge0.src, edge1.dst);
    EXPECT_EQ(edge0.dst, edge1.src);
    EXPECT_EQ(edge0.src_type, edge1.dst_type);
    EXPECT_EQ(edge0.dst_type, edge1.src_type);
    EXPECT_FLOAT_EQ(edge0.amount_, edge1.amount_);
    EXPECT_EQ(edge0.topic, edge1.topic);
  }
}

const uint64_t num_nodes           = 19;
const uint64_t num_projected_nodes = 17;
const uint64_t num_projected_edges =
    (num_projected_nodes) * (num_projected_nodes - 1);
const uint64_t num_edges = num_projected_edges + 4 * num_projected_nodes + 1;
const uint64_t projected_node_offset = num_projected_nodes;
const uint64_t projected_edge_offset =
    num_projected_edges + 4 * num_projected_nodes;

std::unique_ptr<wf4::FullNetworkGraph> generateTestFullGraph() {
  std::vector<wf4::FullNetworkNode> vertices(num_nodes);
  std::vector<galois::GenericEdge<wf4::FullNetworkEdge>> edges(num_edges);

  // Data that will be projected out
  {
    wf4::FullNetworkNode node(projected_node_offset + 0,
                              agile::workflow1::TYPES::DEVICE);
    vertices[projected_node_offset + 0] = node;
    wf4::FullNetworkNode node2(projected_node_offset + 1,
                               agile::workflow1::TYPES::PERSON);
    vertices[projected_node_offset + 1] = node2;

    edges[projected_edge_offset] = galois::GenericEdge(
        projected_node_offset + 1, projected_node_offset + 0,
        wf4::FullNetworkEdge(
            projected_node_offset + 1, projected_node_offset + 0,
            agile::workflow1::TYPES::SALE, agile::workflow1::TYPES::PERSON,
            agile::workflow1::TYPES::DEVICE, 1, 8486));
  }

  for (uint64_t i = 0; i < num_projected_nodes; i++) {
    wf4::FullNetworkNode node(i, agile::workflow1::TYPES::PERSON);
    node.sold_ = i * i;
    node.bought_ =
        (num_projected_nodes + i) * (num_projected_nodes - (i + 1)) / 2;
    vertices[i] = node;
  }
  // Every node sells to every node with a global ID less than itself
  // And so every node buys from every node with a global ID more than itself
  // We set the edge weight to be equal to the global ID of the seller
  // Vertex 0 does not sell anything
  uint64_t edge_count = 0;
  for (uint64_t src = 0; src < num_projected_nodes; src++) {
    // will be projected out
    edges[edge_count++] = galois::GenericEdge(
        src, 1,
        wf4::FullNetworkEdge(src, 2, agile::workflow1::TYPES::AUTHOR,
                             agile::workflow1::TYPES::PERSON,
                             agile::workflow1::TYPES::PERSON, 1, 8486));
    // will be projected out
    edges[edge_count++] = galois::GenericEdge(
        src, 1,
        wf4::FullNetworkEdge(src, 0, agile::workflow1::TYPES::AUTHOR,
                             agile::workflow1::TYPES::PERSON,
                             agile::workflow1::TYPES::DEVICE, 1, 8486));
    for (uint64_t dst = 0; dst < src; dst++) {
      edges[edge_count++] = galois::GenericEdge(
          src, dst,
          wf4::FullNetworkEdge(src, dst, agile::workflow1::TYPES::SALE,
                               agile::workflow1::TYPES::PERSON,
                               agile::workflow1::TYPES::PERSON, src, 8486));
    }
    // will be projected out
    edges[edge_count++] = galois::GenericEdge(
        src, 1,
        wf4::FullNetworkEdge(src, 1, agile::workflow1::TYPES::SALE,
                             agile::workflow1::TYPES::PERSON,
                             agile::workflow1::TYPES::PERSON, 1, 8487));
    for (uint64_t dst = src + 1; dst < num_projected_nodes; dst++) {
      edges[edge_count++] = galois::GenericEdge(
          src, dst,
          wf4::FullNetworkEdge(src, dst, agile::workflow1::TYPES::PURCHASE,
                               agile::workflow1::TYPES::PERSON,
                               agile::workflow1::TYPES::PERSON, dst, 8486));
    }
    // will be projected out
    edges[edge_count++] = galois::GenericEdge(
        src, 3,
        wf4::FullNetworkEdge(src, 3, agile::workflow1::TYPES::SALE,
                             agile::workflow1::TYPES::PERSON,
                             agile::workflow1::TYPES::PERSON, 0, 8486));
  }
  auto& net = galois::runtime::getSystemNetworkInterface();
  return std::unique_ptr<wf4::FullNetworkGraph>(
      new wf4::FullNetworkGraph(net.ID, net.Num, vertices, edges));
}

std::unique_ptr<wf4::NetworkGraph> generateTestGraph(bool singleHost = false) {
  std::vector<wf4::NetworkNode> vertices(num_projected_nodes);
  std::vector<galois::GenericEdge<wf4::NetworkEdge>> edges(num_projected_edges);

  for (uint64_t i = 0; i < num_projected_nodes; i++) {
    wf4::NetworkNode node(i);
    node.sold_ = i * i;
    node.bought_ =
        (num_projected_nodes + i) * (num_projected_nodes - (i + 1)) / 2;
    vertices[i] = node;
  }
  // Every node sells to every node with a global ID less than itself
  // And so every node buys from every node with a global ID more than itself
  // We set the edge weight to be equal to the global ID of the seller
  // Vertex 0 does not sell anything
  uint64_t edge_count = 0;
  for (uint64_t src = 0; src < num_projected_nodes; src++) {
    for (uint64_t dst = 0; dst < src; dst++) {
      edges[edge_count++] = galois::GenericEdge(
          src, dst,
          wf4::NetworkEdge(src, dst, src, agile::workflow1::TYPES::SALE));
    }
    for (uint64_t dst = src + 1; dst < num_projected_nodes; dst++) {
      edges[edge_count++] = galois::GenericEdge(
          src, dst,
          wf4::NetworkEdge(src, dst, dst, agile::workflow1::TYPES::PURCHASE));
    }
  }
  auto& net      = galois::runtime::getSystemNetworkInterface();
  uint32_t host  = net.ID;
  uint32_t hosts = net.Num;
  if (singleHost) {
    host  = 0;
    hosts = 1;
  }
  return std::unique_ptr<wf4::NetworkGraph>(
      new wf4::NetworkGraph(host, hosts, vertices, edges));
}

void graphsEqual(wf4::NetworkGraph& actual, wf4::NetworkGraph& expected) {
  EXPECT_EQ(actual.size(), expected.size());
  galois::DGAccumulator<uint64_t> vertex_count;
  vertex_count.reset();
  for (uint64_t i = 0; i < num_projected_nodes; i++) {
    if (!actual.isOwned(i)) {
      continue;
    }
    vertex_count += 1;
    wf4::NetworkNode actual_node = actual.getData(actual.getLID(i));
    bool found                   = false;
    for (uint64_t j = 0; j < num_projected_nodes; j++) {
      wf4::NetworkNode expected_node = expected.getData(expected.getLID(j));
      if (actual_node.id == expected_node.id) {
        found = true;
        EXPECT_FLOAT_EQ(actual_node.bought_, expected_node.bought_);
        EXPECT_FLOAT_EQ(actual_node.sold_, expected_node.sold_);
        EXPECT_FLOAT_EQ(actual_node.desired_, expected_node.desired_);
        std::printf(
            "node token id: %lu, node global id: %lu, node local id: %u\n", i,
            actual_node.id, actual.getLID(i));
        auto actualEdges   = actual.edges(actual.getLID(i)).begin();
        auto expectedEdges = expected.edges(expected.getLID(j)).begin();
        EXPECT_EQ(
            std::distance(actualEdges, actual.edges(actual.getLID(i)).end()),
            std::distance(expectedEdges,
                          expected.edges(expected.getLID(j)).end()));
        for (; actualEdges != actual.edges(actual.getLID(i)).end();
             actualEdges++) {
          wf4::NetworkEdge actual_edge = actual.getEdgeData(*actualEdges);
          wf4::NetworkEdge expected_edge =
              expected.getEdgeData(*(expectedEdges++));
          EXPECT_FLOAT_EQ(actual_edge.amount_, expected_edge.amount_);
          EXPECT_FLOAT_EQ(actual_edge.weight_, expected_edge.weight_);
          EXPECT_EQ(actual_edge.type, expected_edge.type);
        }
      }
    }
    EXPECT_EQ(found, true);
  }
  EXPECT_EQ(vertex_count.reduce(), num_projected_nodes);
}

} // namespace

TEST(Import, Parse) {
  std::vector<std::string> files;
  files.emplace_back(std::string(someFile));
  wf4::internal::CyberParser cyber_parser(files);
  wf4::internal::SocialParser social_parser(files);
  wf4::internal::UsesParser uses_parser(files);
  galois::graphs::WMDParser<wf4::FullNetworkNode, wf4::FullNetworkEdge>
      commercial_parser(8, files);
  uint64_t half_max = std::numeric_limits<uint64_t>::max() / 2;

  // TODO(Patrick) add node parser test
  const char* invalid =
      "invalid,,,1615340315424362057,1116314936447312244,,,2/11/2018,,";

  const char* sale          = "Sale,1552474,1928788,,8/21/2018,,,";
  const char* weighted_sale = "Sale,299156,458364,8486,,,,3.0366367403882406";
  const char* communication = "0,217661,172800,0,6,26890,94857,6,5,1379,1770";
  const char* friend_edge   = "5,679697";
  const char* uses          = "12,311784";

  galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                       wf4::FullNetworkEdge>
      result;
  result = commercial_parser.ParseLine(const_cast<char*>(invalid),
                                       std::strlen(invalid));
  EXPECT_EQ(result.isEdge, false);

  result =
      commercial_parser.ParseLine(const_cast<char*>(sale), std::strlen(sale));
  checkParsedEdge(result,
                  wf4::FullNetworkEdge(1552474UL, 1928788UL,
                                       agile::workflow1::TYPES::SALE,
                                       agile::workflow1::TYPES::PERSON,
                                       agile::workflow1::TYPES::PERSON, 0, 0),
                  agile::workflow1::TYPES::PURCHASE, 2);
  result = commercial_parser.ParseLine(const_cast<char*>(weighted_sale),
                                       std::strlen(weighted_sale));
  checkParsedEdge(result,
                  wf4::FullNetworkEdge(299156UL, 458364UL,
                                       agile::workflow1::TYPES::SALE,
                                       agile::workflow1::TYPES::PERSON,
                                       agile::workflow1::TYPES::PERSON,
                                       3.0366367403882406, 8486),
                  agile::workflow1::TYPES::PURCHASE, 2);
  result = cyber_parser.ParseLine(const_cast<char*>(communication),
                                  std::strlen(communication));
  checkParsedEdge(result,
                  wf4::FullNetworkEdge(half_max + 0UL, half_max + 217661UL,
                                       agile::workflow1::TYPES::COMMUNICATION,
                                       agile::workflow1::TYPES::DEVICE,
                                       agile::workflow1::TYPES::DEVICE, 0, 0),
                  agile::workflow1::TYPES::NONE, 1);
  result = social_parser.ParseLine(const_cast<char*>(friend_edge),
                                   std::strlen(friend_edge));
  checkParsedEdge(result,
                  wf4::FullNetworkEdge(5UL, 679697UL,
                                       agile::workflow1::TYPES::FRIEND,
                                       agile::workflow1::TYPES::PERSON,
                                       agile::workflow1::TYPES::PERSON, 0, 0),
                  agile::workflow1::TYPES::NONE, 1);
  result = uses_parser.ParseLine(const_cast<char*>(uses), std::strlen(uses));
  checkParsedEdge(result,
                  wf4::FullNetworkEdge(12UL, half_max + 311784UL,
                                       agile::workflow1::TYPES::USES,
                                       agile::workflow1::TYPES::PERSON,
                                       agile::workflow1::TYPES::DEVICE, 0, 0),
                  agile::workflow1::TYPES::NONE, 1);
}

TEST(Import, GeneratedGraph) {
  std::unique_ptr<wf4::NetworkGraph> test_graph_ptr = generateTestGraph();
  wf4::NetworkGraph& test_graph                     = *test_graph_ptr;
  galois::DGAccumulator<uint64_t> vertex_count;
  galois::DGAccumulator<uint64_t> edge_count;
  vertex_count.reset();
  edge_count.reset();
  for (wf4::NetworkGraph::GraphNode node : test_graph.masterNodesRange()) {
    vertex_count += 1;
    wf4::NetworkNode node_data = test_graph.getData(node);
    double bought              = node_data.bought_;
    EXPECT_EQ(test_graph.getGID(node), node_data.id);
    EXPECT_EQ(bought, (num_projected_nodes + node_data.id) *
                          (num_projected_nodes - (node_data.id + 1)) / 2);
    for (const auto& edge : test_graph.edges(node)) {
      edge_count += 1;
      wf4::NetworkEdge edge_data = test_graph.getEdgeData(edge);
      uint64_t dst_id = test_graph.getGID(test_graph.getEdgeDst(edge));
      EXPECT_NE(dst_id, node_data.id);
      EXPECT_EQ(dst_id < node_data.id,
                edge_data.type == agile::workflow1::TYPES::SALE);
      EXPECT_EQ(dst_id > node_data.id,
                edge_data.type == agile::workflow1::TYPES::PURCHASE);
      if (edge_data.type == agile::workflow1::TYPES::SALE) {
        EXPECT_EQ(edge_data.amount_, node_data.id);
      } else {
        EXPECT_EQ(edge_data.amount_, dst_id);
      }
    }
  }
  EXPECT_EQ(vertex_count.reduce(), num_projected_nodes);
  EXPECT_EQ(edge_count.reduce(), num_projected_edges);
}

TEST(Import, Projection) {
  std::unique_ptr<wf4::FullNetworkGraph> full_graph = generateTestFullGraph();
  std::unique_ptr<wf4::NetworkGraph> projected_graph =
      wf4::ProjectGraph(std::move(full_graph));
  std::unique_ptr<wf4::NetworkGraph> expected_graph = generateTestGraph(true);
  graphsEqual(*projected_graph, *expected_graph);
}
