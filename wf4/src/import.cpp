// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include "wf4/import.hpp"

#include <ctime>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "galois/wmd/schema.h"

std::unique_ptr<wf4::FullNetworkGraph>
wf4::ImportData(const InputFiles& input_files) {
  std::unique_ptr<FullNetworkGraph> full_graph =
      internal::ImportGraph(input_files);

  return full_graph;
}

std::unique_ptr<wf4::NetworkGraph>
wf4::ProjectGraph(std::unique_ptr<wf4::FullNetworkGraph> full_graph) {
  internal::NetworkGraphProjection projection;
  (void)projection;
  std::unique_ptr<NetworkGraph> projected_graph =
      full_graph->Project<NetworkGraph, internal::NetworkGraphProjection>(
          projection);
  return projected_graph;
}

std::unique_ptr<wf4::FullNetworkGraph>
wf4::internal::ImportGraph(const wf4::InputFiles& input_files) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  std::vector<std::unique_ptr<
      galois::graphs::FileParser<FullNetworkNode, FullNetworkEdge>>>
      parsers;

  parsers.emplace_back(
      std::make_unique<
          galois::graphs::WMDParser<FullNetworkNode, FullNetworkEdge>>(
          8, input_files.commercial_files));
  parsers.emplace_back(std::make_unique<CyberParser>(input_files.cyber_files));
  parsers.emplace_back(
      std::make_unique<SocialParser>(input_files.social_files));
  parsers.emplace_back(std::make_unique<UsesParser>(input_files.uses_files));
  parsers.emplace_back(std::make_unique<NodeParser>(input_files.nodes_files));
  std::unique_ptr<FullNetworkGraph> g =
      std::make_unique<FullNetworkGraph>(parsers, net.ID, net.Num);
  galois::runtime::getHostBarrier().wait();
  return g;
}

bool wf4::internal::NetworkGraphProjection::KeepNode(
    wf4::FullNetworkGraph& graph, wf4::FullNetworkGraph::GraphNode node) {
  return graph.getData(node).type_ == agile::workflow1::TYPES::PERSON;
}

bool wf4::internal::NetworkGraphProjection::KeepEdge(
    wf4::FullNetworkGraph& graph, const wf4::FullNetworkEdge& edge,
    wf4::FullNetworkGraph::GraphNode, wf4::FullNetworkGraph::GraphNode dst) {
  return (edge.type == agile::workflow1::TYPES::PURCHASE ||
          edge.type == agile::workflow1::TYPES::SALE) &&
         edge.topic == 8486 && edge.amount_ > 0 && KeepNode(graph, dst);
}

wf4::NetworkNode wf4::internal::NetworkGraphProjection::ProjectNode(
    wf4::FullNetworkGraph&, const wf4::FullNetworkNode& node,
    wf4::FullNetworkGraph::GraphNode) {
  return NetworkNode(node);
}
wf4::NetworkEdge wf4::internal::NetworkGraphProjection::ProjectEdge(
    wf4::FullNetworkGraph&, const wf4::FullNetworkEdge& edge,
    wf4::FullNetworkGraph::GraphNode, wf4::FullNetworkGraph::GraphNode) {
  return NetworkEdge(edge);
}

galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode, wf4::FullNetworkEdge>
wf4::internal::CyberParser::ParseLine(char* line, uint64_t lineLength) {
  std::vector<std::string> tokens =
      this->SplitLine(line, lineLength, ',', csv_fields_);

  std::vector<wf4::FullNetworkEdge> edges;
  edges.emplace_back(
      wf4::FullNetworkEdge(agile::workflow1::TYPES::COMMUNICATION, tokens));

  return galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                              wf4::FullNetworkEdge>(edges);
}
std::vector<wf4::FullNetworkNode>
wf4::internal::CyberParser::GetDstData(std::vector<wf4::FullNetworkEdge>&) {
  throw std::runtime_error("Not implemented");
}

galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode, wf4::FullNetworkEdge>
wf4::internal::SocialParser::ParseLine(char* line, uint64_t lineLength) {
  std::vector<std::string> tokens =
      this->SplitLine(line, lineLength, ',', csv_fields_);

  std::vector<wf4::FullNetworkEdge> edges;
  edges.emplace_back(
      wf4::FullNetworkEdge(agile::workflow1::TYPES::FRIEND, tokens));

  return galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                              wf4::FullNetworkEdge>(edges);
}

std::vector<wf4::FullNetworkNode>
wf4::internal::SocialParser::GetDstData(std::vector<wf4::FullNetworkEdge>&) {
  throw std::runtime_error("Not implemented");
}

galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode, wf4::FullNetworkEdge>
wf4::internal::UsesParser::ParseLine(char* line, uint64_t lineLength) {
  std::vector<std::string> tokens =
      this->SplitLine(line, lineLength, ',', csv_fields_);

  std::vector<wf4::FullNetworkEdge> edges;
  edges.emplace_back(
      wf4::FullNetworkEdge(agile::workflow1::TYPES::USES, tokens));

  return galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                              wf4::FullNetworkEdge>(edges);
}

std::vector<wf4::FullNetworkNode>
wf4::internal::UsesParser::GetDstData(std::vector<wf4::FullNetworkEdge>&) {
  throw std::runtime_error("Not implemented");
}

wf4::internal::NodeParser::~NodeParser() {}

galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode, wf4::FullNetworkEdge>
wf4::internal::NodeParser::ParseLine(char* line, uint64_t lineLength) {
  std::vector<std::string> tokens =
      this->SplitLine(line, lineLength, ',', csv_fields_);

  galois::graphs::ParsedUID uid =
      shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
  agile::workflow1::TYPES type = agile::workflow1::TYPES::PERSON;
  if (tokens[0] == "Device") {
    const uint64_t half_max = std::numeric_limits<uint64_t>::max() / 2;
    uid                     = half_max + (uid % half_max);
    type                    = agile::workflow1::TYPES::DEVICE;
  }
  wf4::FullNetworkNode node(uid, 0, type);

  return galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                              wf4::FullNetworkEdge>(node);
}

std::vector<wf4::FullNetworkNode>
wf4::internal::NodeParser::GetDstData(std::vector<wf4::FullNetworkEdge>&) {
  throw std::runtime_error("Not implemented");
}
