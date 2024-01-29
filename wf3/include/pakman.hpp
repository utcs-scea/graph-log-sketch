//===------------------------------------------------------------*- C++ -*-===//
////
////                            The AGILE Workflows
////
////===----------------------------------------------------------------------===//
//// ** Pre-Copyright Notice
////
//// This computer software was prepared by Battelle Memorial Institute,
//// hereinafter the Contractor, under Contract No. DE-AC05-76RL01830 with the
//// Department of Energy (DOE). All rights in the computer software are
///reserved / by DOE on behalf of the United States Government and the
///Contractor as / provided in the Contract. You are authorized to use this
///computer software / for Governmental purposes but it is not to be released or
///distributed to the / public. NEITHER THE GOVERNMENT NOR THE CONTRACTOR MAKES
///ANY WARRANTY, EXPRESS / OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF
///THIS SOFTWARE. This / notice including this sentence must appear on any
///copies of this computer / software.
////
//// ** Disclaimer Notice
////
//// This material was prepared as an account of work sponsored by an agency of
//// the United States Government. Neither the United States Government nor the
//// United States Department of Energy, nor Battelle, nor any of their
///employees, / nor any jurisdiction or organization that has cooperated in the
///development / of these materials, makes any warranty, express or implied, or
///assumes any / legal liability or responsibility for the accuracy,
///completeness, or / usefulness or any information, apparatus, product,
///software, or process / disclosed, or represents that its use would not
///infringe privately owned / rights. Reference herein to any specific
///commercial product, process, or / service by trade name, trademark,
///manufacturer, or otherwise does not / necessarily constitute or imply its
///endorsement, recommendation, or favoring / by the United States Government or
///any agency thereof, or Battelle Memorial / Institute. The views and opinions
///of authors expressed herein do not / necessarily state or reflect those of
///the United States Government or any / agency thereof.
////
////                    PACIFIC NORTHWEST NATIONAL LABORATORY
////                                 operated by
////                                   BATTELLE
////                                   for the
////                      UNITED STATES DEPARTMENT OF ENERGY
////                       under Contract DE-AC05-76RL01830
////===----------------------------------------------------------------------===//

#ifndef WF3_PAKMAN_H_
#define WF3_PAKMAN_H_

#include "galois/graphs/Morph_SepInOut_Graph.h"

#include <string>
#include <unordered_set>

#define TINY 5000
#define SMALL 500000
#define MEDIUM 5000000
#define LARGE 50000000

#define UINT_BITS 64
#define SIZE_BP 2                   // bits per base pair
#define SIZE_BPV 6                  // size of base pair vector in 64 bit words
#define BP_PER_WORD 32              // number of base pairs per word = 64 / 2
#define CONTIG_LENGTH_THRESHOLD 400 // output contig length threshold

namespace pakman {

class MacroNode;
class PakmanEdge;
typedef galois::graphs::Morph_SepInOut_Graph<MacroNode, PakmanEdge, true, true,
                                             true>
    PakmanGraph;
typedef PakmanGraph::GraphNode PakmanNode;

void processContigs(PakmanGraph& graph, uint64_t mn_length);

// pred_mask(3, 5)
// shift_right = (32 - 3) * 2 = 58
// shift_left  = (5 - 3) * 2 = 4
//
// 11111...11111 >> 58 = 000...00000111111
// 00...00111111 <<  4 = 000...01111110000
inline uint64_t pred_mask(uint64_t pred_size, uint64_t word_size) {
  uint64_t shift_right = (BP_PER_WORD - pred_size) * SIZE_BP;
  uint64_t shift_left  = (word_size - pred_size) * SIZE_BP;
  return ((~0UL) >> shift_right) << shift_left;
}

inline uint64_t succ_mask(uint64_t succ_size) {
  return ((~0UL) >> (SIZE_BP * (BP_PER_WORD - succ_size)));
}

class BasePairVector {
public:
  uint64_t size_;
  uint64_t vec_[SIZE_BPV];

  BasePairVector();
  BasePairVector(uint64_t word, uint64_t size);

  uint64_t extract_pred(uint64_t pred_size);
  uint64_t extract_succ(uint64_t suff_size);
  void extract_succ2(BasePairVector& affix, uint64_t suff_size);
  void push_back(uint64_t val);
  void append(const BasePairVector& bpv);

  uint64_t size() const { return size_; }
  void print(FILE* ff);
  std::string to_string() const;

  uint64_t operator[](uint64_t pos) const;
};

class MacroNode {
public:
  MacroNode(BasePairVector kmer, BasePairVector affix, bool prefix,
            bool terminal, uint32_t visit_count, uint32_t count)
      : kmer_(kmer), affix_(affix), prefix_(prefix), terminal_(terminal),
        visit_count_(visit_count), count_(count) {}

  BasePairVector kmer_;
  BasePairVector affix_;
  bool prefix_;
  bool terminal_;
  uint32_t visit_count_;
  uint32_t count_;
};

class PakmanEdge {
public:
  PakmanEdge() = default;
  PakmanEdge(bool wire, uint32_t count) : wire_(wire), count_(count) {}

  bool wire_;
  uint32_t count_;
};

inline bool operator==(const BasePairVector& k1, const BasePairVector& k2) {
  if (k1.size() != k2.size()) {
    return false;
  }
  for (uint64_t i = 0; i < k1.size(); i++) {
    if (k1[i] != k2[i])
      return false;
  }
  return true;
}

inline bool operator!=(const BasePairVector& k1, const BasePairVector& k2) {
  return !(k1 == k2);
}

inline bool operator>(const BasePairVector& k1, const BasePairVector& k2) {
  if (k1.size() != k2.size()) {
    return k1.size() > k2.size();
  }
  for (uint64_t i = 0; i < k1.size(); i++) {
    if (k1[i] != k2[i]) {
      return k1[i] > k2[i];
    }
  }
  return false;
}

class Comp_rev {
  const PakmanGraph& graph_;
  const std::vector<PakmanNode>& v_;

public:
  Comp_rev(const PakmanGraph& graph, const std::vector<PakmanNode>& v)
      : graph_(graph), v_(v) {}

  bool operator()(size_t i, size_t j) {
    const MacroNode& node_i = graph_.getData(v_[i]);
    const MacroNode& node_j = graph_.getData(v_[j]);
    if (node_i.visit_count_ != node_j.visit_count_) {
      return node_i.visit_count_ > node_j.visit_count_;
    }
    if (node_i.count_ != node_j.count_) {
      return node_i.count_ > node_j.count_;
    }
    return node_i.affix_ > node_j.affix_;
  }
};

} // namespace pakman

#endif
