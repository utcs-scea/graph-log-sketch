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

#include <limits>
#include <string>
#include <sys/stat.h>

#include "main.h"
#include "graph.h"

namespace agile::workflow1 {

std::vector<std::string> split(std::string& line, char delim,
                               uint64_t size = 0) {
  uint64_t ndx = 0, start = 0, end = 0;
  std::vector<std::string> tokens(size);

  for (; end < line.length(); end++) {
    if ((line[end] == delim) || (line[end] == '\n')) {
      tokens[ndx] = line.substr(start, end - start);
      start       = end + 1;
      ndx++;
    }
  }

  tokens[size - 1] = line.substr(start, end - start); // flush last token
  return tokens;
}

inline void insertVertex(GlobalIDType& GlobalIDS, const uint64_t& key,
                         const uint64_t& num_edges, const TYPES& type,
                         uint64_t& id_counter) {
  auto found = GlobalIDS.find(key);
  if (found == GlobalIDS.end()) {
    GlobalIDS[key] = Vertex(id_counter++, num_edges, type);
  } else {
    found->second.edges += num_edges;
  }
}

void readFile(const RF_args_t& args) {
  std::string line;
  struct stat stats;
  std::string filename = args.filename;
  // uint64_t this_locale = (uint32_t) shad::rt::thisLocality();
  // uint64_t num_locales = (uint64_t) shad::rt::numLocalities();

  std::ifstream file(filename);
  if (!file.is_open()) {
    printf("cannot open file %s\n", filename.c_str());
    exit(-1);
  }

  stat(filename.c_str(), &stats);

  // uint64_t num_bytes = stats.st_size / num_locales;                      //
  // file size / number of locales uint64_t start = this_locale * num_bytes;
  // uint64_t end = start + num_bytes;
  uint64_t start = 0;
  uint64_t end   = stats.st_size;

  file.seekg(start);

  EdgeType& Edges         = *((EdgeType*)args.Edges_OID);
  GlobalIDType& GlobalIDS = *((GlobalIDType*)args.GlobalIDS_OID);

  uint64_t id_counter = 0;
  while (start < end) {
    getline(file, line);
    start += line.size() + 1;
    if (line[0] == '#')
      continue; // skip comments
    std::vector<std::string> tokens =
        split(line, ',', 10); // delimiter and # tokens set for wmd data file

    if (tokens[0] == "Person") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[1]), 0,
                   TYPES::PERSON, id_counter);
    } else if (tokens[0] == "ForumEvent") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[4]), 0,
                   TYPES::FORUMEVENT, id_counter);
    } else if (tokens[0] == "Forum") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[3]), 0,
                   TYPES::FORUM, id_counter);
    } else if (tokens[0] == "Publication") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[5]), 0,
                   TYPES::PUBLICATION, id_counter);
    } else if (tokens[0] == "Topic") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[6]), 0,
                   TYPES::TOPIC, id_counter);
    } else if (tokens[0] == "Sale") {
      Edge sale(tokens);
      Edges.insert({sale.src, sale});

      Edge purchase = sale;
      purchase.type = TYPES::PURCHASE;
      std::swap(purchase.src, purchase.dst);
      Edges.insert({purchase.src, purchase});

      insertVertex(GlobalIDS, sale.src, 1, sale.src_type, id_counter);
      insertVertex(GlobalIDS, sale.dst, 0, sale.dst_type, id_counter);
      insertVertex(GlobalIDS, purchase.src, 1, purchase.src_type, id_counter);
      insertVertex(GlobalIDS, purchase.dst, 0, purchase.dst_type, id_counter);
    } else if (tokens[0] == "Author") {
      Edge authors(tokens);
      Edges.insert({authors.src, authors});

      Edge writtenBY = authors;
      writtenBY.type = TYPES::WRITTENBY;
      std::swap(writtenBY.src, writtenBY.dst);
      std::swap(writtenBY.src_type, writtenBY.dst_type);
      Edges.insert({writtenBY.src, writtenBY});

      insertVertex(GlobalIDS, authors.src, 1, authors.src_type, id_counter);
      insertVertex(GlobalIDS, authors.dst, 0, authors.dst_type, id_counter);
      insertVertex(GlobalIDS, writtenBY.src, 1, writtenBY.src_type, id_counter);
      insertVertex(GlobalIDS, writtenBY.dst, 0, writtenBY.dst_type, id_counter);
    } else if (tokens[0] == "Includes") {
      Edge includes(tokens);
      Edges.insert({includes.src, includes});

      Edge includedIN = includes;
      includedIN.type = TYPES::INCLUDEDIN;
      std::swap(includedIN.src, includedIN.dst);
      std::swap(includedIN.src_type, includedIN.dst_type);
      Edges.insert({includedIN.src, includedIN});

      insertVertex(GlobalIDS, includes.src, 1, includes.src_type, id_counter);
      insertVertex(GlobalIDS, includes.dst, 0, includes.dst_type, id_counter);
      insertVertex(GlobalIDS, includedIN.src, 1, includedIN.src_type,
                   id_counter);
      insertVertex(GlobalIDS, includedIN.dst, 0, includedIN.dst_type,
                   id_counter);
    } else if (tokens[0] == "HasTopic") {
      Edge hasTopic(tokens);
      Edges.insert({hasTopic.src, hasTopic});

      Edge topicIN = hasTopic;
      topicIN.type = TYPES::TOPICIN;
      std::swap(topicIN.src, topicIN.dst);
      std::swap(topicIN.src_type, topicIN.dst_type);
      Edges.insert({topicIN.src, topicIN});

      insertVertex(GlobalIDS, hasTopic.src, 1, hasTopic.src_type, id_counter);
      insertVertex(GlobalIDS, hasTopic.dst, 0, hasTopic.dst_type, id_counter);
      insertVertex(GlobalIDS, topicIN.src, 1, topicIN.src_type, id_counter);
      insertVertex(GlobalIDS, topicIN.dst, 0, topicIN.dst_type, id_counter);
    } else if (tokens[0] == "HasOrg") {
      Edge hasOrg(tokens);
      Edges.insert({hasOrg.src, hasOrg});

      Edge orgIN = hasOrg;
      orgIN.type = TYPES::ORGIN;
      std::swap(orgIN.src, orgIN.dst);
      std::swap(orgIN.src_type, orgIN.dst_type);
      Edges.insert({orgIN.src, orgIN});

      insertVertex(GlobalIDS, hasOrg.src, 1, hasOrg.src_type, id_counter);
      insertVertex(GlobalIDS, hasOrg.dst, 0, hasOrg.dst_type, id_counter);
      insertVertex(GlobalIDS, orgIN.src, 1, orgIN.src_type, id_counter);
      insertVertex(GlobalIDS, orgIN.dst, 0, orgIN.dst_type, id_counter);
    }
  }
}

} // namespace agile::workflow1
