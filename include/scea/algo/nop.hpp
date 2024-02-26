#pragma once

#include "algo_interface.hpp"

namespace scea::algo {

class Nop : public Algo {
public:
  void operator()(scea::graph::MutableGraph&) override {}
};

} // namespace scea::algo