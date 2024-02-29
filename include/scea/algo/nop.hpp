// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include "algo_interface.hpp"

namespace scea::algo {

class Nop : public Algo {
public:
  void operator()(scea::graph::MutableGraph&) override {}
};

} // namespace scea::algo
