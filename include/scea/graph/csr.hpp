// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>

#include "galois/Galois.h"
#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::graph {

class CSR : public MutableGraph {
private:
  std::vector<std::vector<uint64_t>> m_adj;

  std::vector<uint64_t> m_vertices;
  std::vector<uint64_t> m_edges;

  static uint64_t transmute(std::vector<uint64_t> const& p) { return p.size(); }
  static uint64_t scan_op(std::vector<uint64_t> const& p, const uint64_t& l) {
    return p.size() + l;
  }
  static uint64_t combiner(uint64_t const& f, uint64_t const& s) {
    return f + s;
  }
  galois::PrefixSum<typename std::vector<uint64_t>, uint64_t, transmute,
                    scan_op, combiner, galois::CacheLinePaddedArr>
      m_pfx_sum;

public:
  explicit CSR(uint64_t num_vertices)
      : m_adj(num_vertices), m_vertices(num_vertices + 1, 0ul),
        m_pfx_sum{&m_adj[0], &m_vertices[1]} {}

  virtual ~CSR() {}

  uint64_t size() noexcept override { return m_adj.size(); }

  uint64_t get_out_degree(uint64_t src) override {
    return m_vertices[src + 1] - m_vertices[src];
  }

  void add_edges(uint64_t src, const std::vector<uint64_t> dsts) override {
    std::copy(dsts.begin(), dsts.end(), std::back_inserter(m_adj[src]));
  }

  void post_ingest() override {
    m_pfx_sum.computePrefixSum(m_adj.size());
    m_edges.resize(m_vertices[m_adj.size()]);
    galois::do_all(
        galois::iterate(0ul, m_adj.size()),
        [&](uint64_t const& i) {
          std::copy(m_adj[i].begin(), m_adj[i].end(),
                    m_edges.begin() + m_vertices[i]);
        },
        galois::loopname("LC_CSR::post_ingest"));
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto i = m_vertices[src]; i < m_vertices[src + 1]; ++i) {
      callback(m_edges[i]);
    }
  }

  void sort_edges(uint64_t src) override {
    std::sort(m_edges.begin() + m_vertices[src],
              m_edges.begin() + m_vertices[src]);
  }

  bool find_edge_sorted(uint64_t src, uint64_t dst) override {
    return std::binary_search(m_edges.begin() + m_vertices[src],
                              m_edges.begin() + m_vertices[src + 1], dst);
  }
};

} // namespace scea::graph
