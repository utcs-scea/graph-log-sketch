// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "graph.hpp"

namespace wf4 {

std::unique_ptr<FullNetworkGraph> ImportData(const InputFiles& input_files);
std::unique_ptr<NetworkGraph>
ProjectGraph(std::unique_ptr<FullNetworkGraph> full_graph);

namespace internal {

std::unique_ptr<FullNetworkGraph> ImportGraph(const InputFiles& input_files);

struct NetworkGraphProjection {
  bool KeepNode(FullNetworkGraph& graph, FullNetworkGraph::GraphNode node);
  bool KeepEdgeLessMasters() const { return false; }
  bool KeepEdge(FullNetworkGraph& graph, const FullNetworkEdge& edge,
                FullNetworkGraph::GraphNode, FullNetworkGraph::GraphNode dst);
  NetworkNode ProjectNode(FullNetworkGraph&, const FullNetworkNode& node,
                          FullNetworkGraph::GraphNode);
  NetworkEdge ProjectEdge(FullNetworkGraph&, const FullNetworkEdge& edge,
                          FullNetworkGraph::GraphNode,
                          FullNetworkGraph::GraphNode);
};

class CyberParser : public galois::graphs::FileParser<wf4::FullNetworkNode,
                                                      wf4::FullNetworkEdge> {
public:
  explicit CyberParser(std::vector<std::string> files) : files_(files) {}

  const std::vector<std::string>& GetFiles() override { return files_; }
  galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                       wf4::FullNetworkEdge>
  ParseLine(char* line, uint64_t lineLength) override;

private:
  uint64_t csv_fields_ = 11;
  std::vector<std::string> files_;
};

class SocialParser : public galois::graphs::FileParser<wf4::FullNetworkNode,
                                                       wf4::FullNetworkEdge> {
public:
  explicit SocialParser(std::vector<std::string> files) : files_(files) {}

  const std::vector<std::string>& GetFiles() override { return files_; }
  galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                       wf4::FullNetworkEdge>
  ParseLine(char* line, uint64_t lineLength) override;

private:
  uint64_t csv_fields_ = 2;
  std::vector<std::string> files_;
};

class UsesParser : public galois::graphs::FileParser<wf4::FullNetworkNode,
                                                     wf4::FullNetworkEdge> {
public:
  explicit UsesParser(std::vector<std::string> files) : files_(files) {}

  const std::vector<std::string>& GetFiles() override { return files_; }
  galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                       wf4::FullNetworkEdge>
  ParseLine(char* line, uint64_t lineLength) override;

private:
  uint64_t csv_fields_ = 2;
  std::vector<std::string> files_;
};

class NodeParser : public galois::graphs::FileParser<wf4::FullNetworkNode,
                                                     wf4::FullNetworkEdge> {
public:
  explicit NodeParser(std::vector<std::string> files) : files_(files) {}

  const std::vector<std::string>& GetFiles() override { return files_; }
  galois::graphs::ParsedGraphStructure<wf4::FullNetworkNode,
                                       wf4::FullNetworkEdge>
  ParseLine(char* line, uint64_t lineLength) override;

private:
  uint64_t csv_fields_ = 7;
  std::vector<std::string> files_;
};

} // namespace internal

} // namespace wf4
