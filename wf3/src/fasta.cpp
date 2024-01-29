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

#include "fasta.hpp"

#include "pakman.hpp"
#include "util.hpp"

#include "galois/Galois.h"
#include "galois/runtime/Network.h"

#include <fstream>
#include <limits>
#include <mutex>
#include <sys/stat.h>

typedef std::pair<std::vector<pakman::PakmanNode>,
                  std::vector<pakman::PakmanNode>>
    MacroNodeAffixes;

namespace {

uint64_t countEdges(pakman::PakmanGraph& graph) {
  uint64_t edges = 0;

  for (pakman::PakmanNode src : graph) {
    edges +=
        std::distance(graph.out_edges(src).begin(), graph.out_edges(src).end());
  }
  return edges;
}

void addMacroEdges(
    pakman::PakmanGraph& graph,
    const std::unordered_map<uint64_t, MacroNodeAffixes>& macro_nodes,
    uint64_t mn_length) {
  uint64_t suffix_mask = ~0UL >> (UINT_BITS - (2 * (mn_length - 1)));
  bool is_wire         = true;
  for (const auto& macro_node : macro_nodes) {
    for (const pakman::PakmanNode& suffix_node : macro_node.second.second) {
      pakman::MacroNode& suffix_macro = graph.getData(suffix_node);
      if (suffix_macro.affix_.size() > 0) {
        uint64_t affix_size = suffix_macro.affix_.size();
        uint64_t dest_kmer =
            ((macro_node.first & suffix_mask) << (affix_size * 2)) |
            suffix_macro.affix_.extract_succ(affix_size);
        const auto& prefix_macro_affix = macro_nodes.find(dest_kmer);
        if (prefix_macro_affix != macro_nodes.end()) {
          pakman::BasePairVector prefix = pakman::BasePairVector(
              suffix_macro.kmer_.extract_pred(affix_size), affix_size);
          for (const pakman::PakmanNode& prefix_node :
               prefix_macro_affix->second.first) {
            pakman::MacroNode& prefix_macro = graph.getData(prefix_node);
            if (prefix == prefix_macro.affix_) {
              pakman::PakmanEdge relation = pakman::PakmanEdge(!is_wire, 0);
              graph.getEdgeData(graph.addEdge(suffix_node, prefix_node)) =
                  relation;
            }
          }
        }
      }
    }
  }
}

void wireMacroNode(pakman::PakmanGraph& graph,
                   const MacroNodeAffixes& macro_node) {
  int64_t prefix_count       = 0;
  int64_t suffix_count       = 0;
  uint64_t null_prefix_index = 0;
  uint64_t null_suffix_index = 0;

  uint64_t index = 0;
  for (pakman::PakmanNode prefix_node : macro_node.first) {
    pakman::MacroNode& node = graph.getData(prefix_node);
    if (node.affix_.size() == 0) {
      null_prefix_index = index;
    } else {
      prefix_count += node.visit_count_;
    }
    index++;
  }
  index = 0;
  for (pakman::PakmanNode suffix_node : macro_node.second) {
    pakman::MacroNode& node = graph.getData(suffix_node);
    if (node.affix_.size() == 0) {
      null_suffix_index = index;
    } else {
      suffix_count += node.visit_count_;
    }
    index++;
  }

  // count and coverage for null affixes
  pakman::MacroNode& null_prefix =
      graph.getData(macro_node.first[null_prefix_index]);
  pakman::MacroNode& null_suffix =
      graph.getData(macro_node.second[null_suffix_index]);
  null_prefix.count_       = 1;
  null_prefix.visit_count_ = std::max(suffix_count - prefix_count, 0L);
  null_suffix.count_       = 1;
  null_suffix.visit_count_ = std::max(prefix_count - suffix_count, 0L);

  std::vector<uint64_t> prefix_indices(macro_node.first.size());
  std::iota(prefix_indices.begin(), prefix_indices.end(), 0);
  std::sort(prefix_indices.begin(), prefix_indices.end(),
            pakman::Comp_rev(graph, macro_node.first));

  std::vector<uint64_t> suffix_indices(macro_node.second.size());
  std::iota(suffix_indices.begin(), suffix_indices.end(), 0);
  std::sort(suffix_indices.begin(), suffix_indices.end(),
            pakman::Comp_rev(graph, macro_node.second));

  uint64_t prefix_index = 0;
  uint64_t suffix_index = 0;

  int64_t var_prefix = 0;
  int64_t var_suffix = 0;
  int64_t leftover   = suffix_count + null_suffix.visit_count_;

  bool is_wire = true;

  while (leftover > 0) {
    pakman::PakmanNode prefix_node =
        macro_node.first[prefix_indices[prefix_index]];
    pakman::PakmanNode suffix_node =
        macro_node.second[suffix_indices[suffix_index]];

    uint32_t prefix_visit_count = graph.getData(prefix_node).visit_count_;
    uint32_t suffix_visit_count = graph.getData(suffix_node).visit_count_;
    int64_t prefix_wire_count   = prefix_visit_count - var_prefix;
    int64_t suffix_wire_count   = suffix_visit_count - var_suffix;
    int64_t wire_count = std::min(prefix_wire_count, suffix_wire_count);

    pakman::PakmanEdge wire = pakman::PakmanEdge(is_wire, wire_count);
    graph.getEdgeData(graph.addEdge(prefix_node, suffix_node)) = wire;

    var_prefix += wire_count;
    var_suffix += wire_count;
    leftover -= wire_count;

    if (var_prefix == prefix_visit_count) {
      var_prefix = 0;
      prefix_index++;
    }

    if (var_suffix == suffix_visit_count) {
      var_suffix = 0;
      suffix_index++;
    }
  }
}

uint64_t getMinBucketCount(std::vector<uint64_t> bucket_counts,
                           uint64_t min_length_count) {
  uint64_t min_index = 0;
  uint64_t min_count = std::numeric_limits<uint64_t>::max();

  for (uint64_t i = 1; i < min_length_count;
       ++i) { // compute ndx with minimum number of appearances
    uint64_t count = bucket_counts[i];
    if (count < min_count) {
      min_index = i;
      min_count = count;
    }
  }
  return min_index;
}

void readFasta(std::pair<uint64_t, uint64_t> read_range, std::string filename,
               uint64_t mn_length,
               std::unordered_map<uint64_t, uint32_t>& host_kmer_map,
               std::mutex& lock) {
  std::unordered_map<uint64_t, uint32_t> kmer_map;
  uint32_t host  = galois::runtime::getHostID();
  uint32_t hosts = galois::runtime::getSystemNetworkInterface().Num;
  uint64_t start = read_range.first;
  uint64_t end   = read_range.second;

  std::ifstream file(filename);
  if (!file.is_open()) {
    printf("Host %u cannot open file %s\n", host, filename.c_str());
    exit(-1);
  }

  std::string line;
  if (start != 0) { // check for partial line
    file.seekg(start - 1);
    getline(file, line);
    if (line[0] != '\n')
      start += line.size(); // if not at start of a line, discard partial line
  }

  while (start < end) {
    getline(file, line);
    start += line.size() + 1;
    if (line[0] == '>')
      continue; // skip comments

    uint64_t kmer = 0;
    for (uint64_t i = 0; i < mn_length; ++i) {
      kmer = (kmer << 2) +
             CHAR_TO_EL(line[i]); // ... shift 2 bits left and add next char
    }

    for (uint64_t i = mn_length; i < line.size();
         ++i) { // for each char until end of line
      kmer = (kmer << 2) +
             CHAR_TO_EL(line[i]); // ... shift 2 bits left and add next char

      if (kmer_map.find(kmer) == kmer_map.end()) {
        kmer_map[kmer] = 0;
      }
      kmer_map[kmer] += 1;
    }
  }

  file.close();

  std::lock_guard<std::mutex> lock_guard(lock);
  for (auto iter : kmer_map) {
    uint64_t kmer  = iter.first;
    uint32_t count = iter.second;
    if (host_kmer_map.find(kmer) == host_kmer_map.end()) {
      host_kmer_map[kmer] = 0;
    }
    host_kmer_map[kmer] += count;
  }
}

uint64_t visitValue(uint64_t count, uint64_t coverage) {
  double ceil_val = (double)count / (double)coverage;
  return (uint64_t)ceil(ceil_val);
}

} // end namespace

std::unique_ptr<pakman::PakmanGraph> pakman::ingest(std::string filename,
                                                    uint64_t mn_length,
                                                    uint64_t coverage,
                                                    uint64_t min_length_count) {
  std::unordered_map<uint64_t, uint32_t> kmer_map =
      pakman::read(filename, mn_length);
  std::cout << "Map Size: " << kmer_map.size() << std::endl;

  std::vector<uint64_t> bucket_counts =
      pakman::getBucketCounts(kmer_map, min_length_count);
  uint64_t min_index = getMinBucketCount(bucket_counts, min_length_count);
  std::cout << "Min Index: " << min_index << std::endl;

  std::unique_ptr<PakmanGraph> graph = pakman::createPakmanNodes(
      std::move(kmer_map), mn_length, coverage, min_index);
  std::cout << "Nodes: " << graph->size() << std::endl;
  uint64_t edges = countEdges(*graph);
  std::cout << "Edges: " << edges << std::endl;

  return graph;
}

std::unique_ptr<pakman::PakmanGraph>
pakman::createPakmanNodes(std::unordered_map<uint64_t, uint32_t>&& kmer_map,
                          uint64_t mn_length, uint64_t coverage,
                          uint64_t min_index) {
  std::unique_ptr<pakman::PakmanGraph> graph = std::make_unique<PakmanGraph>();
  uint64_t suffix_mask = ~0UL >> (UINT_BITS - (2 * (mn_length)));
  bool is_prefix       = true;
  bool is_terminal     = true;
  std::unordered_map<uint64_t, MacroNodeAffixes> macro_nodes;

  for (auto iter : kmer_map) {
    uint64_t kmer  = iter.first;
    uint32_t count = iter.second;
    if (count >= min_index) {
      uint64_t suffix_k1_mer = kmer >> 2;
      uint64_t prefix_k1_mer = kmer & suffix_mask;

      BasePairVector suffix_kmer = BasePairVector(suffix_k1_mer, mn_length);
      BasePairVector prefix_kmer = BasePairVector(prefix_k1_mer, mn_length);

      BasePairVector suffix_affix =
          BasePairVector(kmer & 3, 1); // push back last  protein of kmer
      BasePairVector prefix_affix = BasePairVector(
          kmer >> (2 * mn_length), 1); // push back first protein of kmer

      uint32_t visit_count = visitValue(count, coverage);

      MacroNode suffix_mn = MacroNode(suffix_kmer, suffix_affix, !is_prefix,
                                      !is_terminal, visit_count, count);
      MacroNode prefix_mn = MacroNode(prefix_kmer, prefix_affix, is_prefix,
                                      !is_terminal, visit_count, count);

      PakmanNode suffix_node = graph->createNode(suffix_mn);
      PakmanNode prefix_node = graph->createNode(prefix_mn);

      graph->addNode(suffix_node);
      graph->addNode(prefix_node);

      if (macro_nodes.find(suffix_k1_mer) == macro_nodes.end()) {
        macro_nodes[suffix_k1_mer] = MacroNodeAffixes();
      }
      macro_nodes[suffix_k1_mer].second.emplace_back(suffix_node);

      if (macro_nodes.find(prefix_k1_mer) == macro_nodes.end()) {
        macro_nodes[prefix_k1_mer] = MacroNodeAffixes();
      }
      macro_nodes[prefix_k1_mer].first.emplace_back(prefix_node);
    }
  }

  // TODO (Patrick) return graph and macro nodes then split the following off
  // into a different function

  std::cout << "Number of (k-1)-mers: " << macro_nodes.size() << std::endl;

  // add null prefix and suffixes for each k-1 mer
  for (auto k1_mer : macro_nodes) {
    MacroNode null_suffix_mn =
        MacroNode(BasePairVector(k1_mer.first, mn_length), BasePairVector(),
                  !is_prefix, is_terminal, 1, 1);
    MacroNode null_prefix_mn =
        MacroNode(BasePairVector(k1_mer.first, mn_length), BasePairVector(),
                  is_prefix, is_terminal, 1, 1);
    PakmanNode suffix_null_node = graph->createNode(null_suffix_mn);
    PakmanNode prefix_null_node = graph->createNode(null_prefix_mn);
    graph->addNode(suffix_null_node);
    graph->addNode(prefix_null_node);
    k1_mer.second.second.emplace_back(suffix_null_node);
    k1_mer.second.first.emplace_back(prefix_null_node);

    wireMacroNode(*graph, k1_mer.second);
  }

  std::cout << "Nodes: " << graph->size() << std::endl;
  uint64_t edges = countEdges(*graph);
  std::cout << "Edges: " << edges << std::endl;

  addMacroEdges(*graph, macro_nodes, mn_length);
  return graph;
}

std::vector<uint64_t>
pakman::getBucketCounts(std::unordered_map<uint64_t, uint32_t> kmer_map,
                        uint64_t min_length_count) {
  std::vector<uint64_t> bucket_counts(min_length_count, 0);

  for (auto iter : kmer_map) {
    uint32_t count = iter.second;
    if (count < min_length_count) {
      bucket_counts[count]++;
    }
  }
  return bucket_counts;
}

std::unordered_map<uint64_t, uint32_t> pakman::read(std::string filename,
                                                    uint64_t mn_length) {
  // TODO (Patrick) use parallel or distributed hashmap
  std::unordered_map<uint64_t, uint32_t> kmer_map;
  std::mutex lock;

  uint32_t host    = galois::runtime::getHostID();
  uint32_t hosts   = galois::runtime::getSystemNetworkInterface().Num;
  uint32_t threads = galois::getActiveThreads();

  struct stat stats;
  std::ifstream file(filename);
  if (!file.is_open()) {
    printf("Host %u cannot open file %s\n", host, filename.c_str());
    exit(-1);
  }

  stat(filename.c_str(), &stats);
  file.close();

  uint64_t host_num_bytes =
      stats.st_size / hosts; // file size / number of hosts
  uint64_t host_start = host * host_num_bytes;
  uint64_t host_end   = host_start + host_num_bytes;
  if (host == hosts - 1) {
    // last host processes to end of file
    host_end = stats.st_size;
  }

  std::vector<std::pair<uint64_t, uint64_t>> read_ranges(threads);
  uint32_t block_size = host_num_bytes / threads;
  for (uint32_t t = 0; t < threads; t++) {
    uint64_t start = host_start + (t * block_size);
    uint64_t end   = start + block_size;
    if (t == threads - 1) {
      end = host_end;
    }
    read_ranges.emplace_back(std::pair<uint64_t, uint64_t>(start, end));
  }

  galois::for_each(
      galois::iterate(read_ranges.begin(), read_ranges.end()),
      [&](const std::pair<uint64_t, uint64_t>& read_range, auto&) {
        readFasta(read_range, filename, mn_length, kmer_map, lock);
      },
      galois::loopname("ReadFASTA"));

  return kmer_map;
}
