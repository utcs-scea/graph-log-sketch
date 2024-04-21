// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <optional>
#include <ostream>

#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::algo {

class Algo {
public:
  virtual ~Algo() = default;

  virtual void operator()(scea::graph::MutableGraph& g) = 0;

  /* Run the algorithm and output the result to the given stream. */
  virtual void operator()(scea::graph::MutableGraph& g, std::ostream& output) {
    operator()(g);
    output << "done" << std::endl;
  }
};

} // namespace scea::algo
