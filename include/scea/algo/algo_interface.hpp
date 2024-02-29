// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::algo {

class Algo {
public:
  virtual void operator()(scea::graph::MutableGraph& g) = 0;
};

} // namespace scea::algo
