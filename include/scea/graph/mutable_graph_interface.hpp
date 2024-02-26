#pragma once

#include <functional>

#include "galois/Galois.h"

namespace scea::graph {

/* Contains the minimal functionalities that are benchmarked. */
class MutableGraph {
public:
  virtual int add_edges(uint64_t src, const std::vector<uint64_t> dsts) = 0;
  virtual uint64_t size() noexcept                                      = 0;

  virtual void for_each_edge(uint64_t src,
                             std::function<void(uint64_t const&)> callback) = 0;
};

} // namespace scea::graph