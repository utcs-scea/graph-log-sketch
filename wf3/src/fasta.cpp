//===------------------------------------------------------------*- C++ -*-===//
////
////                            The AGILE Workflows
////
////===----------------------------------------------------------------------===//
//// ** Pre-Copyright Notice
////
//// This computer software was prepared by Battelle Memorial Institute,
//// hereinafter the Contractor, under Contract No. DE-AC05-76RL01830 with the
//// Department of Energy (DOE). All rights in the computer software are reserved
//// by DOE on behalf of the United States Government and the Contractor as
//// provided in the Contract. You are authorized to use this computer software
//// for Governmental purposes but it is not to be released or distributed to the
//// public. NEITHER THE GOVERNMENT NOR THE CONTRACTOR MAKES ANY WARRANTY, EXPRESS
//// OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE. This
//// notice including this sentence must appear on any copies of this computer
//// software.
////
//// ** Disclaimer Notice
////
//// This material was prepared as an account of work sponsored by an agency of
//// the United States Government. Neither the United States Government nor the
//// United States Department of Energy, nor Battelle, nor any of their employees,
//// nor any jurisdiction or organization that has cooperated in the development
//// of these materials, makes any warranty, express or implied, or assumes any
//// legal liability or responsibility for the accuracy, completeness, or
//// usefulness or any information, apparatus, product, software, or process
//// disclosed, or represents that its use would not infringe privately owned
//// rights. Reference herein to any specific commercial product, process, or
//// service by trade name, trademark, manufacturer, or otherwise does not
//// necessarily constitute or imply its endorsement, recommendation, or favoring
//// by the United States Government or any agency thereof, or Battelle Memorial
//// Institute. The views and opinions of authors expressed herein do not
//// necessarily state or reflect those of the United States Government or any
//// agency thereof.
////
////                    PACIFIC NORTHWEST NATIONAL LABORATORY
////                                 operated by
////                                   BATTELLE
////                                   for the
////                      UNITED STATES DEPARTMENT OF ENERGY
////                       under Contract DE-AC05-76RL01830
////===----------------------------------------------------------------------===//

#include "fasta.hpp"
#include "util.hpp"

#include "galois/Galois.h"
#include "galois/runtime/Network.h"

#include <fstream>
#include <limits>
#include <mutex>
#include <sys/stat.h>

namespace {

template<typename T>
std::ostream& operator<<(std::ostream& s, const std::vector<T>& v)
{
    s.put('[');
    char comma[3] = {'\0', ' ', '\0'};
    for (const auto& e : v)
    {
        s << comma << e;
        comma[0] = ',';
    }
    return s << ']';
}

uint64_t
getMinBucketCount(std::vector<uint64_t> bucket_counts, uint64_t min_length_count) {
  uint64_t min_index = 0;
  uint64_t min_count = std::numeric_limits<uint64_t>::max();

  for (uint64_t i = 1; i < min_length_count; ++ i) {                  // compute ndx with minimum number of appearances
    uint64_t count = bucket_counts[i];
    if (count < min_count) {
      min_index = i;
      min_count = count;
    }
  }
  return min_index;
}

void
readFasta(std::pair<uint64_t, uint64_t> read_range, std::string filename, uint64_t mn_length, std::unordered_map<uint64_t, uint32_t>& host_kmer_map, std::mutex& lock) {
  std::unordered_map<uint64_t, uint32_t> kmer_map;
  uint32_t host = galois::runtime::getHostID();
  uint32_t hosts = galois::runtime::getSystemNetworkInterface().Num;
  uint64_t start = read_range.first;
  uint64_t end = read_range.second;

  std::ifstream file(filename);
  if (! file.is_open()) {
    printf("Host %u cannot open file %s\n", host, filename.c_str());
    exit(-1);
  }

  std::string line;
  if (start != 0) {                                       // check for partial line
     file.seekg(start - 1);
     getline(file, line);
     if (line[0] != '\n') start += line.size();                 // if not at start of a line, discard partial line
  }

  while (start < end) {
    getline(file, line);
    start += line.size() + 1;
    if (line[0] == '>') continue;                                        // skip comments

    uint64_t kmer = 0;
    for (uint64_t i = 0; i < mn_length; ++ i) {
      kmer = (kmer << 2) + CHAR_TO_EL(line[i]);                          // ... shift 2 bits left and add next char
    }

    for (uint64_t i = mn_length; i < line.size(); ++ i) {            // for each char until end of line
      kmer = (kmer << 2) + CHAR_TO_EL(line[i]);                          // ... shift 2 bits left and add next char

      if (kmer_map.find(kmer) == kmer_map.end()) {
        kmer_map[kmer] = 0;
      }
      kmer_map[kmer] += 1;
    }
  }

  file.close();

  std::lock_guard<std::mutex> lock_guard(lock);
  for (auto iter : kmer_map) {
    uint64_t kmer = iter.first;
    uint32_t count = iter.second;
    if (host_kmer_map.find(kmer) == host_kmer_map.end()) {
      host_kmer_map[kmer] = 0;
    }
    host_kmer_map[kmer] += count;
  }
}

} // end namespace

/**
SAVE

root@845966d6ea34:/LS_CSR/docker-build/wf3# ./pakman --fasta-file /LS_CSR/data/ecoli_10x.fasta
WARNING: Numa support configured but not present at runtime.  Assuming numa topology matches socket topology.
Map Size: 33655008
Bucket Counts: [0, 24669987, 736175, 454243, 503200, 542963, 569788, 581557, 583048, 566681, 543824, 508861, 471419, 428282, 384294, 340286, 296892, 255129, 215572, 181448, 151386]
Min Index: 20
STAT_TYPE, REGION, CATEGORY, TOTAL_TYPE, TOTAL
*/

void
fasta::ingest(std::string filename, uint64_t mn_length, uint64_t min_length_count) {
  std::unordered_map<uint64_t, uint32_t> kmer_map = fasta::read(filename, mn_length);
  // TODO (Patrick) sync map in distributed broadcasting
  std::cout << "Map Size: " << kmer_map.size() << std::endl;

  std::vector<uint64_t> bucket_counts = fasta::getBucketCounts(kmer_map, min_length_count);
  std::cout << "Bucket Counts: " << bucket_counts << std::endl;

  uint64_t min_index = getMinBucketCount(bucket_counts, min_length_count);
  std::cout << "Min Index: " << min_index << std::endl;
}

std::vector<uint64_t>
fasta::getBucketCounts(std::unordered_map<uint64_t, uint32_t> kmer_map, uint64_t min_length_count) {
  std::vector<uint64_t> bucket_counts(min_length_count, 0);

  for (auto iter : kmer_map) {
    uint32_t count = iter.second;
    if (count < min_length_count) {
      bucket_counts[count]++;
    }
  }
  return bucket_counts;
}

std::unordered_map<uint64_t, uint32_t>
fasta::read(std::string filename, uint64_t mn_length) {
  // TODO (Patrick) finalize map choice
  std::unordered_map<uint64_t, uint32_t> kmer_map;
  std::mutex lock;

  uint32_t host = galois::runtime::getHostID();
  uint32_t hosts = galois::runtime::getSystemNetworkInterface().Num;
  uint32_t threads = galois::getActiveThreads();

  struct stat stats;
  std::ifstream file(filename);
  if (!file.is_open()) {
    printf("Host %u cannot open file %s\n", host, filename.c_str());
    exit(-1);
  }

  stat(filename.c_str(), &stats);
  file.close();

  uint64_t host_num_bytes = stats.st_size / hosts;             // file size / number of hosts
  uint64_t host_start = host * host_num_bytes;
  uint64_t host_end = host_start + host_num_bytes;
  if (host == hosts - 1) {
    // last host processes to end of file
    host_end = stats.st_size;
  }

  std::vector<std::pair<uint64_t, uint64_t>> read_ranges(threads);
  uint32_t block_size = host_num_bytes / threads;
  for (uint32_t t = 0; t < threads; t++) {
    uint64_t start = host_start + (t * block_size);
    uint64_t end = start + block_size;
    if (t == threads - 1) {
      end = host_end;
    }
    read_ranges.emplace_back(std::pair<uint64_t, uint64_t>(start, end));
  }

  galois::for_each(galois::iterate(read_ranges.begin(),
                                   read_ranges.end()),
                  [&](const std::pair<uint64_t, uint64_t>& read_range, auto&) {
                    readFasta(read_range, filename, mn_length, kmer_map, lock);
                  },
                  galois::loopname("ReadFASTA"));


  return kmer_map;
}
