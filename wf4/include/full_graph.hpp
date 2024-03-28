// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <limits>
#include <string>
#include <vector>

#include "galois/graphs/CuSPPartitioner.h"
#include "galois/graphs/GenericPartitioners.h"
#include "galois/graphs/GluonSubstrate.h"
#include "galois/wmd/WMDPartitioner.h"
#include "galois/wmd/graphTypes.h"

namespace wf4 {

class FullNetworkNode;
class FullNetworkEdge;
typedef galois::graphs::WMDGraph<FullNetworkNode, FullNetworkEdge, OECPolicy>
    FullNetworkGraph;

class FullNetworkEdge {
public:
  FullNetworkEdge() = default;
  FullNetworkEdge(time_t date, double amount)
      : date_(date), amount_(amount), weight_(0) {}
  explicit FullNetworkEdge(const std::vector<std::string>& tokens) {
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
    if (tokens[3].size() > 0) {
      topic = std::stoull(tokens[3]);
    }
  }
  FullNetworkEdge(agile::workflow1::TYPES type_,
                  const std::vector<std::string>& tokens)
      : type(type_) {
    src                     = std::stoull(tokens[0]);
    dst                     = std::stoull(tokens[1]);
    const uint64_t half_max = std::numeric_limits<uint64_t>::max() / 2;

    if (type == agile::workflow1::TYPES::USES) {
      src_type = agile::workflow1::TYPES::PERSON;
      dst_type = agile::workflow1::TYPES::DEVICE;
      dst      = half_max + (dst % half_max);
    } else if (type == agile::workflow1::TYPES::FRIEND) {
      src_type = agile::workflow1::TYPES::PERSON;
      dst_type = agile::workflow1::TYPES::PERSON;
    } else if (type == agile::workflow1::TYPES::COMMUNICATION) {
      src_type = agile::workflow1::TYPES::DEVICE;
      dst_type = agile::workflow1::TYPES::DEVICE;
      src      = half_max + (src % half_max);
      dst      = half_max + (dst % half_max);

      epoch_time  = std::stoull(tokens[2]);
      duration    = std::stoull(tokens[3]);
      protocol    = std::stoull(tokens[4]);
      src_port    = std::stoull(tokens[5]);
      dst_port    = std::stoull(tokens[6]);
      src_packets = std::stoull(tokens[7]);
      dst_packets = std::stoull(tokens[8]);
      src_bytes   = std::stoull(tokens[9]);
      dst_bytes   = std::stoull(tokens[10]);
    }
  }

  time_t date_;
  double amount_;
  double weight_;
  agile::workflow1::TYPES type     = agile::workflow1::TYPES::SALE;
  agile::workflow1::TYPES src_type = agile::workflow1::TYPES::PERSON;
  agile::workflow1::TYPES dst_type = agile::workflow1::TYPES::PERSON;
  uint64_t src;
  uint64_t dst;
  uint64_t src_glbid = std::numeric_limits<uint64_t>::max();
  uint64_t dst_glbid = std::numeric_limits<uint64_t>::max();

  uint64_t topic;

  uint64_t epoch_time;
  uint64_t duration;
  uint64_t protocol;
  uint64_t src_port;
  uint64_t dst_port;
  uint64_t src_packets;
  uint64_t dst_packets;
  uint64_t src_bytes;
  uint64_t dst_bytes;
};

struct FullNetworkNode {
public:
  FullNetworkNode() = default;
  FullNetworkNode(uint64_t id_, uint64_t glbid_, agile::workflow1::TYPES type)
      : id(id_), glbid(glbid_), sold_(0), bought_(0), desired_(0), type_(type) {
  }
  explicit FullNetworkNode(uint64_t id_)
      : id(id_), sold_(0), bought_(0), desired_(0) {}

  void serialize(galois::runtime::SerializeBuffer& buf) const {
    galois::runtime::gSerialize(buf, id, type_);
  }
  void deserialize(galois::runtime::DeSerializeBuffer& buf) const {
    galois::runtime::gDeserialize(buf, id, type_);
  }

  uint64_t id;
  uint64_t glbid;
  galois::CopyableAtomic<uint64_t>
      frequency_; // number of occurrences in Reverse Reachable Sets
  galois::CopyableAtomic<double> sold_; // amount of coffee sold
  galois::CopyableAtomic<double>
      bought_;     // amount of coffee bought  (>= coffee sold)
  double desired_; // amount of coffee desired (>= coffee bought)

  agile::workflow1::TYPES type_;
  uint64_t extra_data;
};

} // namespace wf4

template <>
struct galois::runtime::has_serialize<wf4::FullNetworkNode> {
  static const bool value = true;
};
