// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <functional>
#include <vector>

#include "galois/Galois.h"

namespace scea::graph {

/* Contains the minimal functionalities that are benchmarked. */
class MutableGraph {
public:
  virtual void add_edges(uint64_t src, const std::vector<uint64_t> dsts) = 0;
  virtual uint64_t size() noexcept                                       = 0;

  virtual void post_ingest()                                                = 0;
  virtual void for_each_edge(uint64_t src,
                             std::function<void(uint64_t const&)> callback) = 0;
};

} // namespace scea::graph
