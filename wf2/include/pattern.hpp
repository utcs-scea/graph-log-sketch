// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#ifndef GRAPH_LOG_SKETCH_WF2_INCLUDE_PATTERN_HPP_
#define GRAPH_LOG_SKETCH_WF2_INCLUDE_PATTERN_HPP_

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "galois/graphs/GluonSubstrate.h"
#include "galois/substrate/SimpleLock.h"
#include "graph_ds.hpp"

#define BENCH 1

#define NYC_TOPIC_ID 60

#define WF2_SYNC_STRUCTURE_MASTER_TO_MIRROR(bitsetname)                        \
  struct SyncMasterMirror_##bitsetname {                                       \
    typedef uint64_t ValTy;                                                    \
    static uint64_t extract(uint32_t, wf2::Vertex& node) { return node.id; }   \
    static bool reduce(uint32_t, struct wf2::Vertex& node, uint64_t y) {       \
      return false;                                                            \
    }                                                                          \
    static void reset(uint32_t, struct wf2::Vertex& node) {}                   \
    static void setVal(uint32_t, struct wf2::Vertex& node, uint64_t y) {       \
      bitset_##bitsetname.set(g->getLID(y));                                   \
    }                                                                          \
    static bool extract_batch(unsigned, uint8_t*, size_t*, DataCommMode*) {    \
      return false;                                                            \
    }                                                                          \
    static bool extract_batch(unsigned, uint8_t*) { return false; }            \
    static bool extract_reset_batch(unsigned, uint8_t*, size_t*,               \
                                    DataCommMode*) {                           \
      return false;                                                            \
    }                                                                          \
    static bool extract_reset_batch(unsigned, uint8_t*) { return false; }      \
    static bool reset_batch(size_t, size_t) { return false; }                  \
    static bool reduce_batch(unsigned, uint8_t*, DataCommMode) {               \
      return false;                                                            \
    }                                                                          \
    static bool reduce_mirror_batch(unsigned, uint8_t*, DataCommMode) {        \
      return false;                                                            \
    }                                                                          \
    static bool setVal_batch(unsigned, uint8_t*, DataCommMode) {               \
      return false;                                                            \
    }                                                                          \
  };

#define WF2_SYNC_STRUCTURE_MIRROR_TO_MASTER(bitsetname)                        \
  struct SyncMirrorMaster_##bitsetname {                                       \
    typedef uint64_t ValTy;                                                    \
    static uint64_t extract(uint32_t, wf2::Vertex& node) {                     \
      bitset_##bitsetname.reset(g->getLID(node.id));                           \
      return node.id;                                                          \
    }                                                                          \
    static bool reduce(uint32_t, struct wf2::Vertex& node, uint64_t y) {       \
      bitset_##bitsetname.set(g->getLID(y));                                   \
      return true;                                                             \
    }                                                                          \
    static void reset(uint32_t, struct wf2::Vertex& node) {}                   \
    static void setVal(uint32_t, struct wf2::Vertex& node, uint64_t y) {}      \
    static bool extract_batch(unsigned, uint8_t*, size_t*, DataCommMode*) {    \
      return false;                                                            \
    }                                                                          \
    static bool extract_batch(unsigned, uint8_t*) { return false; }            \
    static bool extract_reset_batch(unsigned, uint8_t*, size_t*,               \
                                    DataCommMode*) {                           \
      return false;                                                            \
    }                                                                          \
    static bool extract_reset_batch(unsigned, uint8_t*) { return false; }      \
    static bool reset_batch(size_t, size_t) { return false; }                  \
    static bool reduce_batch(unsigned, uint8_t*, DataCommMode) {               \
      return false;                                                            \
    }                                                                          \
    static bool reduce_mirror_batch(unsigned, uint8_t*, DataCommMode) {        \
      return false;                                                            \
    }                                                                          \
    static bool setVal_batch(unsigned, uint8_t*, DataCommMode) {               \
      return false;                                                            \
    }                                                                          \
  };

extern wf2::Graph* g;
extern std::unique_ptr<galois::graphs::GluonSubstrate<wf2::Graph>>
    sync_substrate;

namespace wf2 {
void MatchPattern(Graph& g_ref, galois::runtime::NetworkInterface& net);
} // namespace wf2

#endif // GRAPH_LOG_SKETCH_WF2_INCLUDE_PATTERN_HPP_
