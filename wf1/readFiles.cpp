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
#include <stdio.h>

#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/serialization/unordered_map.hpp>

#include "galois/runtime/Network.h"

#include "main.h"
#include "graph.h"

namespace agile::workflow1 {

static uint32_t VERTEX_SIZE_TAG = 1;
static uint32_t VERTEX_GATHER_TAG = 2;

std::vector <std::string> split(std::string & line, char delim, uint64_t size = 0) {
  uint64_t ndx = 0, start = 0, end = 0;
  std::vector <std::string> tokens(size);

  for ( ; end < line.length(); end ++)  {
    if ( (line[end] == delim) || (line[end] == '\n') ) {
       tokens[ndx] = line.substr(start, end - start);
       start = end + 1;
       ndx ++;
  } }

  tokens[size - 1] = line.substr(start, end - start);     // flush last token
  return tokens;
}

inline void insertVertex(GlobalIDType * GlobalIDS, const uint64_t & key, const uint64_t & num_edges, const TYPES & type, uint64_t & id_counter) {
  auto found = GlobalIDS->find(key);
  if (found == GlobalIDS->end()) {
    (*GlobalIDS)[key] = Vertex(id_counter++, num_edges, type);
  } else {
    found->second.edges += num_edges;
  }
}

// Relabel id of vertex so local id becomes global id
void relabelVertexID(GlobalIDType * GlobalIDS, galois::runtime::NetworkInterface& net) {
  uint32_t this_locale = net.ID;
  uint32_t num_locales = net.Num;

  size_t offset = 0;

  // send vertex size to processors after itself
  for (uint32_t h = this_locale + 1; h < num_locales; ++h) {
    // serialize size_t
    size_t local_vertex_size = GlobalIDS->size();
    galois::runtime::SendBuffer sendBuffer((char *) &local_vertex_size, sizeof(size_t));
    net.sendTagged(h, VERTEX_SIZE_TAG, sendBuffer);
  }

  // recv vertex size to processors before itself
  for (uint32_t h = 0; h < this_locale; h++) {
    decltype(net.recieveTagged(VERTEX_SIZE_TAG, nullptr)) p;
    do {
      p = net.recieveTagged(VERTEX_SIZE_TAG, nullptr);
    } while (!p.has_value());
    auto & recvBuffer = p.value().second;
    // deserialize size_t
    size_t local_vertex_size;
    recvBuffer.extract((uint8_t *) &local_vertex_size, sizeof(size_t));
    offset += local_vertex_size;
    std::cout << "local_vertex_size: " <<  local_vertex_size << std::endl;
  }

  // add offset to local id so now it is global id
  galois::do_all(
    galois::iterate(*GlobalIDS),
    [offset](GlobalIDType::value_type& p) {
      p.second.id += offset;
    }
  );
}

// Gather data from other processors to processor 0
template <class T>
void gatherData(T * data, galois::runtime::NetworkInterface& net) {
  uint32_t this_locale = net.ID;
  uint32_t num_locales = net.Num;

  // serilize data on processors other than 0
  if (this_locale != 0) {
    std::string serial_str;
    {
      boost::iostreams::back_insert_device<std::string> inserter(serial_str);
      boost::iostreams::stream<boost::iostreams::back_insert_device<std::string>> stream(inserter);
      boost::archive::binary_oarchive out_archive(stream);
      out_archive << *data;
      stream.flush();
    }

    galois::runtime::SendBuffer sendBuffer(serial_str.c_str(), serial_str.length());
    net.sendTagged(0, VERTEX_GATHER_TAG, sendBuffer);
  } else {
    for (uint32_t h = 1; h < num_locales; h++) {
      decltype(net.recieveTagged(VERTEX_GATHER_TAG, nullptr)) p;
      do {
        p = net.recieveTagged(VERTEX_GATHER_TAG, nullptr);
      } while (!p.has_value());
      auto & recvBuffer = p.value().second;

      // deserialize data
      T restored;
      {
        boost::iostreams::basic_array_source<char> device((char *) recvBuffer.r_linearData(), recvBuffer.r_size());
        boost::iostreams::stream<boost::iostreams::basic_array_source<char> > stream(device);
        boost::archive::binary_iarchive in_archive(stream);
        in_archive >> restored;
      }

      // merge data
      std::cout << "old size: " << data->size() << std::endl;
      std::cout << "restored size: " << restored.size() << std::endl;
      data->merge(restored);
      std::cout << "new size: " << data->size() << std::endl;
    }
  }
}

void readFile(const RF_args_t & args) {
  auto& net = galois::runtime::getSystemNetworkInterface();
  uint32_t this_locale = net.ID;
  uint32_t num_locales = net.Num;

  std::string line;
  struct stat stats;
  std::string filename = args.filename;

  std::ifstream file(filename);
  if (! file.is_open()) { printf("cannot open file %s\n", filename.c_str()); exit(-1); }

  stat(filename.c_str(), & stats);

  uint64_t num_bytes = stats.st_size / num_locales;                      // file size / number of locales
  uint64_t start = this_locale * num_bytes;
  uint64_t end = start + num_bytes;

  if (this_locale != 0) {                                       // check for partial line
     file.seekg(start - 1);
     getline(file, line);
     if (line[0] != '\n') start += line.size();                 // if not at start of a line, discard partial line
  }

  if (this_locale == num_locales - 1) end = stats.st_size;      // last locale processes to end of file

  file.seekg(start);

  EdgeType * Edges = (EdgeType *) args.Edges_OID;
  GlobalIDType * GlobalIDS = (GlobalIDType *) args.GlobalIDS_OID;

  uint64_t id_counter = 0; 
  while (start < end) {
    getline(file, line);
    start += line.size() + 1;
    if (line[0] == '#') continue;                                // skip comments
    std::vector <std::string> tokens = split(line, ',', 10);     // delimiter and # tokens set for wmd data file

    if (tokens[0] == "Person") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[1]), 0, TYPES::PERSON, id_counter);
    } else if (tokens[0] == "ForumEvent") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[4]), 0, TYPES::FORUMEVENT, id_counter);
    } else if (tokens[0] == "Forum") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[3]), 0, TYPES::FORUM, id_counter);
    } else if (tokens[0] == "Publication") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[5]), 0, TYPES::PUBLICATION, id_counter);
    } else if (tokens[0] == "Topic") {
      insertVertex(GlobalIDS, ENCODE<uint64_t, std::string, UINT>(tokens[6]), 0, TYPES::TOPIC, id_counter);
    } else if (tokens[0] == "Sale") {
      Edge sale(tokens);
      Edges->insert({sale.src, sale});

      Edge purchase = sale;
      purchase.type = TYPES::PURCHASE;
      std::swap(purchase.src, purchase.dst);
      Edges->insert({purchase.src, purchase});
    } else if (tokens[0] == "Author") {
      Edge authors(tokens);
      Edges->insert({authors.src, authors});

      Edge writtenBY = authors;
      writtenBY.type = TYPES::WRITTENBY;
      std::swap(writtenBY.src, writtenBY.dst);
      std::swap(writtenBY.src_type, writtenBY.dst_type);
      Edges->insert({writtenBY.src, writtenBY});
    } else if (tokens[0] == "Includes") {
      Edge includes(tokens);
      Edges->insert({includes.src, includes});

      Edge includedIN = includes;
      includedIN.type = TYPES::INCLUDEDIN;
      std::swap(includedIN.src, includedIN.dst);
      std::swap(includedIN.src_type, includedIN.dst_type);
      Edges->insert({includedIN.src, includedIN});
    } else if (tokens[0] == "HasTopic") {
      Edge hasTopic(tokens);
      Edges->insert({hasTopic.src, hasTopic});

      Edge topicIN = hasTopic;
      topicIN.type = TYPES::TOPICIN;
      std::swap(topicIN.src, topicIN.dst);
      std::swap(topicIN.src_type, topicIN.dst_type);
      Edges->insert({topicIN.src, topicIN});
    } else if (tokens[0] == "HasOrg") {
      Edge hasOrg(tokens);
      Edges->insert({hasOrg.src, hasOrg});

      Edge orgIN = hasOrg;
      orgIN.type = TYPES::ORGIN;
      std::swap(orgIN.src, orgIN.dst);
      std::swap(orgIN.src_type, orgIN.dst_type);
      Edges->insert({orgIN.src, orgIN});
    }
  }

  std::cout << "Edges->size() " << Edges->size() << std::endl;
  std::cout << "GlobalIDS->size() " << GlobalIDS->size() << std::endl;

  relabelVertexID(GlobalIDS, net);

  gatherData<GlobalIDType>(GlobalIDS, net);
  gatherData<EdgeType>(Edges, net);
}

} // namespace agile::workflow1
