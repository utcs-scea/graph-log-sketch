// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <ctime>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

#include "full_graph.hpp"
#include "galois/AtomicWrapper.h"
#include "galois/DynamicBitset.h"
#include "galois/graphs/CuSPPartitioner.h"
#include "galois/graphs/GenericPartitioners.h"
#include "galois/graphs/GluonSubstrate.h"
#include "galois/runtime/SyncStructures.h"

class NodeData;

namespace wf4 {

using GlobalNodeID = uint64_t;

class NetworkEdge;
using NetworkNode = NodeData;
typedef galois::graphs::WMDGraph<NetworkNode, NetworkEdge, OECPolicy>
    NetworkGraph;

class NetworkEdge {
public:
  NetworkEdge() = default;
  NetworkEdge(time_t date, double amount)
      : date_(date), amount_(amount), weight_(0) {}
  explicit NetworkEdge(const std::vector<std::string>& tokens) {
    double amount_sold = 0;
    if (tokens[7].size() > 0) {
      amount_sold = std::stod(tokens[7]);
    }

    struct std::tm tm;
    std::istringstream ss(tokens[4]);
    ss >> std::get_time(&tm, "%MM:%DD:%YYYY");
    std::time_t date = mktime(&tm);

    date_   = date;
    amount_ = amount_sold;
    weight_ = 0;
    src     = std::stoull(tokens[1]);
    dst     = std::stoull(tokens[2]);
  }
  explicit NetworkEdge(const wf4::FullNetworkEdge& full_edge) {
    date_     = full_edge.date_;
    amount_   = full_edge.amount_;
    weight_   = full_edge.weight_;
    type      = full_edge.type;
    src_type  = full_edge.src_type;
    dst_type  = full_edge.dst_type;
    src       = full_edge.src;
    dst       = full_edge.dst;
    src_glbid = full_edge.src_glbid;
    dst_glbid = full_edge.dst_glbid;
  }

  time_t date_;
  double amount_;
  double weight_;
  gls::wmd::TYPES type     = gls::wmd::TYPES::SALE;
  gls::wmd::TYPES src_type = gls::wmd::TYPES::PERSON;
  gls::wmd::TYPES dst_type = gls::wmd::TYPES::PERSON;
  uint64_t src;
  uint64_t dst;
  uint64_t src_glbid = std::numeric_limits<uint64_t>::max();
  uint64_t dst_glbid = std::numeric_limits<uint64_t>::max();
};

struct InputFiles {
  InputFiles(std::string input_directory, std::string commercial_file,
             std::string cyber_file, std::string social_file,
             std::string uses_file, std::string nodes_file) {
    if (input_directory.size() > 0) {
      commercial_files.emplace_back(input_directory + "/commercial.csv");
      cyber_files.emplace_back(input_directory + "/cyber.csv");
      social_files.emplace_back(input_directory + "/social.csv");
      uses_files.emplace_back(input_directory + "/uses.csv");
      nodes_files.emplace_back(input_directory + "/nodes.csv");
    }
    if (commercial_file.size() > 0) {
      commercial_files.emplace_back(commercial_file);
    }
    if (cyber_file.size() > 0) {
      cyber_files.emplace_back(cyber_file);
    }
    if (social_file.size() > 0) {
      social_files.emplace_back(social_file);
    }
    if (uses_file.size() > 0) {
      uses_files.emplace_back(uses_file);
    }
    if (nodes_file.size() > 0) {
      nodes_files.emplace_back(nodes_file);
    }
  }

  void Print() {
    for (const std::string& file : commercial_files) {
      std::cout << "Commercial file: " << file << std::endl;
    }
    for (const std::string& file : cyber_files) {
      std::cout << "Cyber file: " << file << std::endl;
    }
    for (const std::string& file : social_files) {
      std::cout << "Social file: " << file << std::endl;
    }
    for (const std::string& file : uses_files) {
      std::cout << "Uses file: " << file << std::endl;
    }
    for (const std::string& file : nodes_files) {
      std::cout << "Node file: " << file << std::endl;
    }
  }

  std::vector<std::string> commercial_files;
  std::vector<std::string> cyber_files;
  std::vector<std::string> social_files;
  std::vector<std::string> uses_files;
  std::vector<std::string> nodes_files;
};

} // namespace wf4

struct NodeData {
public:
  NodeData() = default;
  NodeData(uint64_t id_, uint64_t, gls::wmd::TYPES)
      : id(id_), sold_(0), bought_(0), desired_(0) {}
  explicit NodeData(uint64_t id_)
      : id(id_), sold_(0), bought_(0), desired_(0) {}
  explicit NodeData(const wf4::FullNetworkNode& full_node)
      : id(full_node.id), frequency_(full_node.frequency_),
        sold_(full_node.sold_), bought_(full_node.bought_),
        desired_(full_node.desired_) {}

  void serialize(galois::runtime::SerializeBuffer& buf) const {
    galois::runtime::gSerialize(buf, id);
  }
  void deserialize(galois::runtime::DeSerializeBuffer& buf) const {
    galois::runtime::gDeserialize(buf, id);
  }

  void Cancel() {
    sold_    = 0;
    bought_  = 0;
    desired_ = 0;
  }

  uint64_t id;
  galois::CopyableAtomic<uint64_t>
      frequency_; // number of occurrences in Reverse Reachable Sets
  galois::CopyableAtomic<double> sold_; // amount of coffee sold
  galois::CopyableAtomic<double>
      bought_;     // amount of coffee bought  (>= coffee sold)
  double desired_; // amount of coffee desired (>= coffee bought)
};

template <>
struct galois::runtime::has_serialize<NodeData> {
  static const bool value = true;
};

extern galois::DynamicBitSet bitset_bought_;
extern galois::DynamicBitSet bitset_sold_;
GALOIS_SYNC_STRUCTURE_REDUCE_SET(sold_, double);
GALOIS_SYNC_STRUCTURE_REDUCE_ADD(sold_, double);
GALOIS_SYNC_STRUCTURE_BITSET(sold_);
GALOIS_SYNC_STRUCTURE_REDUCE_SET(bought_, double);
GALOIS_SYNC_STRUCTURE_REDUCE_ADD(bought_, double);
GALOIS_SYNC_STRUCTURE_BITSET(bought_);
