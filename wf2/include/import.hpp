// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#ifndef GRAPH_LOG_SKETCH_WF2_INCLUDE_IMPORT_HPP_
#define GRAPH_LOG_SKETCH_WF2_INCLUDE_IMPORT_HPP_

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "graph_ds.hpp"

namespace wf2 {
wf2::Graph* ImportGraph(const std::string filename);
} // namespace wf2

#endif // GRAPH_LOG_SKETCH_WF2_INCLUDE_IMPORT_HPP_
