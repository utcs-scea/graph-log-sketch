// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>

#include "galois/Galois.h"
#include "scea/graph/adj.hpp"

namespace scea::graph {

class CSR : public AdjGraph {
private:
  static constexpr uint64_t PARALLEL_PREFIX_SUM_VERTEX_THRESHOLD = 1ul << 25;

  std::vector<uint64_t> m_vertices;
  std::vector<uint64_t> m_edges_compact;

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
  CSR() : m_vertices(1, 0ul), m_pfx_sum(&m_edges[0], &m_vertices[1]) {}
  virtual ~CSR(){};

  uint64_t size() noexcept override { return m_vertices.size() - 1; }

  uint64_t get_out_degree(uint64_t src) override {
    return m_vertices[src + 1] - m_vertices[src];
  }

  void post_ingest() override {
    // from the adjacency list:
    const size_t num_vertices = m_edges.size();
    if (num_vertices == 0)
      return;

    // compute prefix sum on the vertices
    m_vertices.resize(num_vertices + 1);
    m_pfx_sum.src = &m_edges[0];
    // note: this prefix sum is inclusive!
    m_pfx_sum.dst = &m_vertices[1];
    if (num_vertices < PARALLEL_PREFIX_SUM_VERTEX_THRESHOLD) {
      m_pfx_sum.computePrefixSumSerially(num_vertices);
    } else {
      m_pfx_sum.computePrefixSum(num_vertices);
    }
    GALOIS_ASSERT(m_vertices[0] == 0,
                  "First entry of CSR vertex array should always be 0!");

    // Compact edges.
    m_edges_compact.resize(m_vertices.back());
    galois::do_all(
        galois::iterate(0ul, num_vertices),
        [&](uint64_t const& i) {
          std::copy(m_edges[i].begin(), m_edges[i].end(),
                    m_edges_compact.begin() + m_vertices[i]);
        },
        galois::steal(), galois::loopname("CreateCSR"));
  }

  void for_each_edge(uint64_t src,
                     std::function<void(uint64_t const&)> callback) override {
    for (auto i = m_vertices[src]; i < m_vertices[src + 1]; ++i)
      callback(m_edges_compact[i]);
  }

  void sort_edges(uint64_t src) override {
    std::sort(m_edges_compact.begin() + m_vertices[src],
              m_edges_compact.begin() + m_vertices[src]);
  }

  bool find_edge_sorted(uint64_t src, uint64_t dst) override {
    return std::binary_search(m_edges_compact.begin() + m_vertices[src],
                              m_edges_compact.begin() + m_vertices[src + 1],
                              dst);
  }
};

} // namespace scea::graph
