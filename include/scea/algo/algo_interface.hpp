#pragma once

#include "scea/graph/mutable_graph_interface.hpp"

namespace scea::algo {

class Algo {
public:
  virtual void operator()(scea::graph::MutableGraph& g) = 0;
};

} // namespace scea::algo