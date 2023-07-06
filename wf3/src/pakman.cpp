//===------------------------------------------------------------*- C++ -*-===//
//
//                            The AGILE Workflows
//
//===----------------------------------------------------------------------===//
// ** Pre-Copyright Notice
//
// This computer software was prepared by Battelle Memorial Institute,
// hereinafter the Contractor, under Contract No. DE-AC05-76RL01830 with the
// Department of Energy (DOE). All rights in the computer software are reserved
// by DOE on behalf of the United States Government and the Contractor as
// provided in the Contract. You are authorized to use this computer software
// for Governmental purposes but it is not to be released or distributed to the
// public. NEITHER THE GOVERNMENT NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS
// OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This
// notice including this sentence must appear on any copies of this computer
// software.
//
// ** Disclaimer Notice
//
// This material was prepared as an account of work sponsored by an agency of
// the United States Government. Neither the United States Government nor the
// United States Department of Energy, nor Battelle, nor any of their employees,
// nor any jurisdiction or organization that has cooperated in the development
// of these materials, makes any warranty, express or implied, or assumes any
// legal liability or responsibility for the accuracy, completeness, or
// usefulness or any information, apparatus, product, software, or process
// disclosed, or represents that its use would not infringe privately owned
// rights. Reference herein to any specific commercial product, process, or
// service by trade name, trademark, manufacturer, or otherwise does not
// necessarily constitute or imply its endorsement, recommendation, or favoring
// by the United States Government or any agency thereof, or Battelle Memorial
// Institute. The views and opinions of authors expressed herein do not
// necessarily state or reflect those of the United States Government or any
// agency thereof.
//
//                    PACIFIC NORTHWEST NATIONAL LABORATORY
//                                 operated by
//                                   BATTELLE
//                                   for the
//                      UNITED STATES DEPARTMENT OF ENERGY
//                       under Contract DE-AC05-76RL01830
//===----------------------------------------------------------------------===//

#include "pakman.hpp"

#include "util.hpp"

namespace {

void
walk(pakman::PakmanGraph& graph, const std::string& cstring, int64_t frequency, const pakman::PakmanNode& prefix_node, uint64_t mn_length) {
  for (auto& wire_edge : graph.out_edges(prefix_node)) {
    uint32_t wire_count = graph.getEdgeData(wire_edge).count_;
    int64_t freq_in_wire = std::min(frequency, (int64_t) wire_count);
    std::string my_cstring = cstring;
    pakman::PakmanNode suffix_node = graph.getEdgeDst(wire_edge);
    const pakman::MacroNode& suffix = graph.getData(suffix_node);

    my_cstring.append(suffix.affix_.to_string());
    //std::cout << ">contig_l_(" + std::to_string(my_cstring.size()) << "): " << my_cstring << std::endl;

    if (my_cstring.size() > 20000) {     // ... ... output partial contig to avoid recursion limit
      std::string name = ">contig_l_" + std::to_string(my_cstring.size());
      printf("%s\n%s\n", name.c_str(), my_cstring.c_str());
    } else if (suffix.terminal_) {                         // ... all done
      if (my_cstring.size() > CONTIG_LENGTH_THRESHOLD) {     // ... ... output contig if longer than threshold
        std::string name = ">contig_l_" + std::to_string(my_cstring.size());
        printf("%s\n%s\n", name.c_str(), my_cstring.c_str());
      }
    } else {                                                  // ... continue walk
      for (auto& relation_edge : graph.out_edges(suffix_node)) {
        pakman::PakmanNode next_prefix_node = graph.getEdgeDst(relation_edge);
        walk(graph, my_cstring, freq_in_wire, next_prefix_node, mn_length);
      }
    }
    frequency -= freq_in_wire;
  }
}

} // end namespace

void
pakman::processContigs(pakman::PakmanGraph& graph, uint64_t mn_length) {
  for (PakmanNode graph_node : graph) {
    const MacroNode& node = graph.getData(graph_node);
    if (node.prefix_ && node.terminal_ && node.visit_count_ > 0) {
      std::string cstring = node.affix_.to_string();
      cstring.append(node.kmer_.to_string());
      walk(graph, cstring, node.visit_count_, graph_node, mn_length);
    }
  }
}

pakman::BasePairVector::BasePairVector() {
  size_ = 0;
  vec_[0] = 0;
  vec_[1] = 0;
}

pakman::BasePairVector::BasePairVector(uint64_t word, uint64_t size) {
  size_ = size;
  vec_[0] = word;
  vec_[1] = 0;
}

// base pairs: AAGTCCTACG
// stored    : AAGT CCTA __CG
// word      :  0    1    2
//
// return leading base pairs in vector; extract(3), returns _AAG
uint64_t
pakman::BasePairVector::extract_pred(uint64_t pred_size) {
  assert (pred_size < BP_PER_WORD);

  uint64_t word_size = (size_ < BP_PER_WORD) ? size_ : BP_PER_WORD;
  uint64_t remove    = word_size - pred_size;               // remove 2 base pairs from word
  uint64_t mask      = pred_mask(pred_size, word_size);     // mask = 11110000
  return (vec_[0] & mask) >> (remove * SIZE_BP);            // (CCTA & 11110000) >> 4 = __CC
} 

// base pairs: AAGTCCTACG
// stored    : AAGT CCTA __CG
// word      :  0    1    2
//
// return trailing base pairs in vector; extract_succ(3), returns _ACG
uint64_t
pakman::BasePairVector::extract_succ(uint64_t suff_size) {
  assert (suff_size < BP_PER_WORD);

  uint64_t mod_size = (size_ % BP_PER_WORD);
  uint64_t last_word_full = (mod_size == 0);
  uint64_t num_words = (size_ / BP_PER_WORD) + ((last_word_full) ? 0 : 1);

  if (last_word_full || (suff_size <= mod_size)) {                   // suffix is all in last word
    return vec_[num_words - 1] & succ_mask(suff_size);
  } else {                                                           // suffix is partially in previous word
    uint64_t last_word = mod_size;                                  // ... last word has 2 bases
    uint64_t prev_word = suff_size - mod_size;                      // ... prevous word has 1 base
    uint64_t kmer = vec_[num_words - 2] & succ_mask(prev_word);     // ... suffix in previous word = __A

    // shift kmer left by # base pairs in previous word and OR in last word; __A << (1 * 2) | __CG
    kmer = (kmer << (last_word * SIZE_BP)) | (vec_[num_words - 1] & succ_mask(last_word));
    return kmer;
  }
}

// base pairs: AAGTCCTACG
// stored    : AAGT CCTA __CG
// word      :  0    1    2
//
// return trailing base pairs in vector; extract_succ(3), returns _ACG
void
pakman::BasePairVector::extract_succ2(pakman::BasePairVector& affix, uint64_t suff_size) {
  size_ = 0;
  uint64_t start = affix.size() - suff_size;
  for (uint64_t i = start; i < affix.size(); ++ i) {
    this->push_back(affix[i]);
  }
}

void
pakman::BasePairVector::push_back(uint64_t val) {                    // val is a single base pair
  if (size_ == SIZE_BPV * BP_PER_WORD) {
    printf("push back ERROR %lu\n", size_);
  } else {
    size_ ++;
    uint64_t word = (size_ - 1) / BP_PER_WORD;      // new base pair is in word
    vec_[word] = (vec_[word] << SIZE_BP) | val;     // shift word to left and OR in val
  }
}

void
pakman::BasePairVector::append(const pakman::BasePairVector& bpv) {
  for (uint64_t i = 0; i < bpv.size(); i++) {
    push_back(bpv[i]);
  }
}

void
pakman::BasePairVector::print(FILE * ff) {
  for (uint64_t i = 0; i < size_; ++ i)
    fprintf(ff, "%c", EL_TO_CHAR((* this)[i]));
}


std::string
pakman::BasePairVector::to_string() const {
  std::string str(size_, ' ');
  for (uint64_t i = 0; i < size_; ++ i)
      str[i] = EL_TO_CHAR((* this)[i]);
  return str;
}

// base pairs: AAGTCCTACG
// stored    : AAGT CCTA __CG          (assume 4 base pairs per word)
// position  : 0123 4567   89
// word      :  0    1    2
// offset    : 0123 0123   01
//
// return base pair at position; [2], returns G
uint64_t
pakman::BasePairVector::operator [] (uint64_t pos) const {
  assert(pos < size_);                          // position must be less than size_

  uint64_t word = pos / BP_PER_WORD;            // position 2 is in word 0
  uint64_t offset = pos % BP_PER_WORD;          // position 2 has offset 2
  uint64_t last_word = size_ / BP_PER_WORD;     // last word is 2
  uint64_t base_pairs_in_word = (word < last_word) ? BP_PER_WORD : size_ % BP_PER_WORD;

  uint64_t shift = (base_pairs_in_word - offset - 1) * SIZE_BP;     // (4 - 2 - 1) * 2 = 2
  return (vec_[word] >> shift) & (0x3);                             // (AAGT >> 2) & 00000011 = ___G
}
