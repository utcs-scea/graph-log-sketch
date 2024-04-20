// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#include "galois/wmd/WMDPartitioner.h"
#include "galois/graphs/DistributedLocalGraph.h"
#include "galois/graphs/GluonSubstrate.h"
#include "galois/graphs/GenericPartitioners.h"

template <typename NodeTy, typename EdgeTy, typename NodeData,
          typename EdgeData>
using DistLocalGraphPtr = std::unique_ptr<
    galois::graphs::WMDGraph<NodeTy, EdgeTy, NodeData, EdgeData, OECPolicy>>;
template <typename NodeData, typename EdgeData>
using DistLocalPtr =
    std::unique_ptr<galois::graphs::DistLocalGraph<NodeData, EdgeData>>;
template <typename NodeTy, typename EdgeTy, typename NodeData,
          typename EdgeData, typename OECPolicy>
DistLocalPtr<NodeData, EdgeData>
distLocalGraphInitialization(std::string& inputFile, uint64_t numVertices) {
  using Graph =
      galois::graphs::WMDGraph<NodeTy, EdgeTy, NodeData, EdgeData, OECPolicy>;
  DistLocalPtr<NodeData, EdgeData> dg;
  std::vector<std::string> filenames;
  filenames.emplace_back(inputFile);
  std::vector<std::unique_ptr<galois::graphs::FileParser<NodeTy, EdgeTy>>>
      parsers;
  parsers.emplace_back(
      std::make_unique<galois::graphs::ELParser<NodeTy, EdgeTy>>(2, filenames));
  const auto& net = galois::runtime::getSystemNetworkInterface();
  return std::make_unique<Graph>(parsers, net.ID, net.Num, true, false,
                                 numVertices,
                                 galois::graphs::BALANCED_EDGES_OF_MASTERS);
}

template <typename NodeData, typename EdgeData>
using DistLocalPtr =
    std::unique_ptr<galois::graphs::DistLocalGraph<NodeData, EdgeData>>;
template <typename NodeData, typename EdgeData>
using DistLocalSubstratePtr = std::unique_ptr<galois::graphs::GluonSubstrate<
    galois::graphs::DistLocalGraph<NodeData, EdgeData>>>;
template <typename NodeData, typename EdgeData>
DistLocalSubstratePtr<NodeData, EdgeData> gluonInitialization(
    std::unique_ptr<galois::graphs::DistLocalGraph<NodeData, EdgeData>>& g) {
  // DistLocalGraphPtr<NodeTy, EdgeTy, NodeData, EdgeData> g;
  using Graph     = galois::graphs::DistLocalGraph<NodeData, EdgeData>;
  using Substrate = galois::graphs::GluonSubstrate<Graph>;
  DistLocalSubstratePtr<NodeData, EdgeData> s;

  const auto& net = galois::runtime::getSystemNetworkInterface();
  // load substrate
  s = std::make_unique<Substrate>(*g, net.ID, net.Num, false);
  return s;
}
