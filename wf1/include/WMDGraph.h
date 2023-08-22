/**
 * @file WMDGraph.h
 *
 * Contains the implementation of WMDBufferedGraph and WMDOfflineGraph which is a galois graph constructed from WMD dataset
 */

#ifndef WMD_BUFFERED_GRAPH_H
#define WMD_BUFFERED_GRAPH_H

#include <fstream>
#include <unordered_map>
#include <atomic>
#include <iterator>
#include <sys/stat.h>

#include <boost/iterator/counting_iterator.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
// #include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include "galois/runtime/Network.h"
#include "galois/config.h"
#include "galois/gIO.h"
#include "galois/Reduction.h"

#include "graphTypes.h"
#include "data_types.h"
#include "graph.h"

namespace galois {
namespace graphs {

void inline increment_evilPhase() {
  ++galois::runtime::evilPhase;
  if (galois::runtime::evilPhase >=
      static_cast<uint32_t>(
          std::numeric_limits<int16_t>::max())) { // limit defined by MPI or
                                                  // LCI
    galois::runtime::evilPhase = 1;
  }
}

std::vector<std::string> split(std::string & line, char delim, uint64_t size = 0) {
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


// https://stackoverflow.com/questions/42114044/how-to-release-unordered-map-memory
template<typename T>
inline void freeContainer(T& p_container)
{
    T empty;
    std::swap(p_container, empty);
}

/**
 * Inherit from OffilineGraph only to make it compatible with Partitioner 
 * Internal logit are completed different
 * TODO: make this template on EdgeDataType
 */
template<typename NodeType, typename EdgeType>
class WMDOfflineGraph : public OfflineGraph {
protected:
  typedef boost::counting_iterator<uint64_t> iterator;
  typedef boost::counting_iterator<uint64_t> edge_iterator;

  // uint64_t numNodes;  // num of global nodes
  // uint64_t numEdges;  // num of global edges

  // TODO: make this a concurrent map
  std::unordered_map<uint64_t, uint64_t> local_tokenToID;  // map node token to local ID
  std::vector<uint64_t> offset;  // each hosts' local ID offset wrt global ID
  std::vector<uint64_t> localNodeSize;  // number of local node in each hosts
  std::vector<uint64_t> localEdgeSize;

  std::unordered_map<uint64_t, size_t> local_tokenToEdgesIdx;  // map local node token to idx in localEdges 
  std::vector<uint64_t> local_EdgesIdxToID;  // map idx in localEdges to global node ID
  std::vector<std::vector<EdgeType>> localEdges;  // edges list of local nodes, idx is local ID 
  std::vector<NodeType> localNodes; // nodes in this host, index by local ID

  std::vector<uint64_t> global_EdgePrefixSum;  // a prefix sum of degree of each global nodes

  uint32_t hostID;
  uint32_t numHosts;

  inline void InsertlocalEdges(uint64_t token, EdgeType& edge) {
    if (auto search = local_tokenToEdgesIdx.find(token); 
        search != local_tokenToEdgesIdx.end()) {  // if token already exists
      localEdges[search->second].push_back(std::move(edge));
    } else {  // not exist, make a new one
      local_tokenToEdgesIdx.insert({token, localEdges.size()});
      std::vector<EdgeType> v;
      v.push_back(std::move(edge));
      localEdges.push_back(std::move(v));
    }
  }

  /**
   * Load graph info from the file.
   * Expect a WMD format csv
   *
   * @param filename loaded file for the graph
   * 
   * TODO: make this parallel
   */
  void loadGraphFile(const std::string& filename) {
    std::string line;
    struct stat stats;

    std::ifstream graphFile = std::ifstream(filename);
    if (! graphFile.is_open()) { printf("cannot open file %s\n", filename.c_str()); exit(-1); }
    stat(filename.c_str(), & stats);
  
    uint64_t id_counter = 0;
    uint64_t edge_counter = 0;

    uint64_t num_bytes = stats.st_size / numHosts;                      // file size / number of locales
    uint64_t start = hostID * num_bytes;
    uint64_t end = start + num_bytes;

    if (hostID != 0) {                                       // check for partial line
      graphFile.seekg(start - 1);
      getline(graphFile, line);
      if (line[0] != '\n') start += line.size();                 // if not at start of a line, discard partial line
    }

    if (hostID == numHosts - 1) end = stats.st_size;      // last locale processes to end of file

    graphFile.seekg(start);

    // get token to global id mapping
    // get token to edges mapping
    while (start < end) {
      #ifdef GRAPH_PROFILE
      remote_file_read_size += line.size();
      #endif

      getline(graphFile, line);
      start += line.size() + 1;
      if (line[0] == '#') continue;                                // skip comments
      std::vector<std::string> tokens = split(line, ',', 10);     // delimiter and # tokens set for wmd data file

      #ifdef GRAPH_PROFILE
      local_rand_write_count += tokens.size() + std::ceil(line.size() / 8) + 2;
      local_rand_write_size += (tokens.size() + std::ceil(line.size() / 8) + 2) * 8;
      local_rand_read_count += 3 + std::ceil(tokens[0].size() / 8);
      local_rand_read_size += (3 + std::ceil(tokens[0].size() / 8)) * 8;
      #endif
      if (tokens[0] == "Person") {
        localNodes.emplace_back(id_counter, 0, agile::workflow1::TYPES::PERSON);
        local_tokenToID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1])] = id_counter++;
      } else if (tokens[0] == "ForumEvent") {
        localNodes.emplace_back(id_counter, 0, agile::workflow1::TYPES::FORUMEVENT);
        local_tokenToID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4])] = id_counter++;
      } else if (tokens[0] == "Forum") {
        localNodes.emplace_back(id_counter, 0, agile::workflow1::TYPES::FORUM);
        local_tokenToID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3])] = id_counter++;
      } else if (tokens[0] == "Publication") {
        localNodes.emplace_back(id_counter, 0, agile::workflow1::TYPES::PUBLICATION);
        local_tokenToID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5])] = id_counter++;
      } else if (tokens[0] == "Topic") {
        localNodes.emplace_back(id_counter, 0, agile::workflow1::TYPES::TOPIC);
        local_tokenToID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6])] = id_counter++;
      } else { // edge type
        agile::workflow1::TYPES inverseEdgeType;
        if (tokens[0] == "Sale") {
          inverseEdgeType = agile::workflow1::TYPES::PURCHASE;
        } else if (tokens[0] == "Author") {
          inverseEdgeType = agile::workflow1::TYPES::WRITTENBY;
        } else if (tokens[0] == "Includes") {
          inverseEdgeType = agile::workflow1::TYPES::INCLUDEDIN;
        } else if (tokens[0] == "HasTopic") {
          inverseEdgeType = agile::workflow1::TYPES::TOPICIN;
        } else if (tokens[0] == "HasOrg") {
          inverseEdgeType = agile::workflow1::TYPES::ORGIN;
        } else {
          // skip nodes
          continue;
        }
        EdgeType edge(tokens);

        InsertlocalEdges(edge.src, edge);

        // insert inverse edges to the graph
        EdgeType inverseEdge = edge;
        inverseEdge.type = inverseEdgeType;
        std::swap(inverseEdge.src, inverseEdge.dst);
        std::swap(inverseEdge.src_type, inverseEdge.dst_type);
        InsertlocalEdges(inverseEdge.src, inverseEdge);

        edge_counter += 2;
      }
    }

    localEdgeSize.resize(numHosts);
    localEdgeSize[hostID] = edge_counter;
    graphFile.close();
  }

  /**
   * Compute offset and global node size by exchange node size with other hosts
  */
  void exchangeLocalNodeSize() {
    auto& net = galois::runtime::getSystemNetworkInterface();

    offset.resize(numHosts);
    localNodeSize.resize(numHosts);

    // send vertex size to other hosts
    for (uint32_t h = 0; h < numHosts; ++h) {
      if (h == hostID) {
        continue;
      }

      // serialize size_t
      uint64_t sizeToSend = local_tokenToID.size();
      galois::runtime::SendBuffer sendBuffer;
      galois::runtime::gSerialize(sendBuffer, sizeToSend);
      net.sendTagged(h, galois::runtime::evilPhase, sendBuffer);
    }

    // recv node size from other hosts
    for (uint32_t h = 0; h < numHosts - 1; h++) {
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);
      uint32_t sendingHost = p->first;
      // deserialize local_node_size
      galois::runtime::gDeserialize(p->second, localNodeSize[sendingHost]);
    }

    // compute prefix sum to get offset
    offset[0] = 0;
    localNodeSize[hostID] = local_tokenToID.size();
    for (size_t h = 1; h < numHosts; h++) {
      offset[h] = localNodeSize[h-1] + offset[h-1];
    }

    // set numNodes (global size)
    setSize(offset[numHosts - 1] + localNodeSize[numHosts - 1]);

    increment_evilPhase();
  }

  /**
   * Compute global ID of edges by exchange local tokenToID
  */
  void exchangeLocalID() {
    auto& net = galois::runtime::getSystemNetworkInterface();

    // relabel Local ID to Global ID
    if (hostID != 0) {
      uint64_t this_offset = offset[hostID];

      galois::do_all(
        galois::iterate(local_tokenToID),
        [this_offset](std::unordered_map<uint64_t, uint64_t>::value_type& p) {
          p.second += this_offset;
        }
      );

      galois::do_all(
        galois::iterate(localNodes),
        [this_offset](NodeType& node) {
          node.id += this_offset;
        }
      );
    }

    // send sorted token list to other hosts (they could compute its global id by offset)
    {
      uint64_t this_offset = offset[hostID];
      std::vector<uint64_t> tokens(local_tokenToID.size());
      galois::do_all(
        galois::iterate(local_tokenToID),
        [&tokens, this_offset](std::unordered_map<uint64_t, uint64_t>::value_type& p) {
          tokens[p.second - this_offset] = p.first;
        }
      );

      galois::runtime::SendBuffer sendBuffer;
      galois::runtime::gSerialize(sendBuffer, tokens);

      uint32_t counter = 1;
      for (uint32_t h = 0; h < numHosts; h++) {
        if (h == hostID) continue;
        if (counter == numHosts - 1) {
          net.sendTagged(h, galois::runtime::evilPhase, sendBuffer);
        } else {
          galois::runtime::SendBuffer b;
          galois::runtime::gSerialize(b, sendBuffer);
          net.sendTagged(h, galois::runtime::evilPhase, b);
          counter++;
        }  
      }
    }

    // This field will be extended to a global view of NodeID
    local_tokenToID.reserve(size());

    // recv sorted token list from other hosts
    for (uint32_t i = 0; i < (numHosts - 1); i++) {
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);
      uint32_t sendingHost = p->first;

      // deserialize data
      std::vector<uint64_t> tokens;
      galois::runtime::gDeserialize(p->second, tokens);

      uint64_t sender_offset = offset[sendingHost];
      // save the token list to GlobalID mapping
      for (uint64_t i = 0; i < tokens.size(); i++) {
        local_tokenToID[tokens[i]] = i + sender_offset;
      }
    }

    increment_evilPhase();
  }

  /**
   * Relabel token to Global ID
   * local_tokenToID and local_tokenToEdgesIdx will be cleared since then
  */
  void relabelTokenToID() {
    // fulfill global ID in edges object
    galois::do_all(
      galois::iterate(localEdges),
      [this](std::vector<EdgeType>& v) {
        std::for_each(
          v.begin(), v.end(), 
          [this](EdgeType& edge) { 
            edge.src_glbid = local_tokenToID[edge.src]; 
            edge.dst_glbid = local_tokenToID[edge.dst];
          }
        );
      }
    );

    // make a maping from localEdges idx to ID
    local_EdgesIdxToID.resize(local_tokenToEdgesIdx.size());
    galois::do_all(
      galois::iterate(local_tokenToEdgesIdx),
      [this](std::unordered_map<uint64_t, size_t>::value_type& p) {
        local_EdgesIdxToID[p.second] = local_tokenToID[p.first];
      }
    );

    // release feilds that won't be used anymore to save memory
    freeContainer(local_tokenToID);
    freeContainer(local_tokenToEdgesIdx);
  }

  /**
   * Compute prefix sum of the size of edges of nodes in the graph
  */
  void computeEdgePrefixSum() {
    auto& net = galois::runtime::getSystemNetworkInterface();

    size_t numLocalNodes = localEdges.size();
    uint64_t numGlobalNodes = size();
    std::vector<uint64_t> localNodeDegree(numLocalNodes);

    galois::do_all(
      galois::iterate((size_t) 0, numLocalNodes),
      [this, &localNodeDegree](size_t n) {
        localNodeDegree[n] = localEdges[n].size();
      }
    );

    // broadcast node degrees and its global ID to other hosts
    {
      galois::runtime::SendBuffer sendBuffer;
      galois::runtime::gSerialize(sendBuffer, localNodeDegree);
      galois::runtime::gSerialize(sendBuffer, local_EdgesIdxToID);  // global ID of the localNodeDegree

      for (uint32_t h = 0; h < numHosts; ++h) {
        if (h == hostID) {
          continue;
        }

        galois::runtime::SendBuffer b;
        galois::runtime::gSerialize(b, sendBuffer);
        net.sendTagged(h, galois::runtime::evilPhase, b);
      }
    }

    // init edge prefix sum
    global_EdgePrefixSum.resize(numGlobalNodes);
    galois::do_all(
        galois::iterate((size_t) 0, local_EdgesIdxToID.size()),
        [this, &localNodeDegree](size_t n) {
          global_EdgePrefixSum[local_EdgesIdxToID[n]] += localNodeDegree[n];
        }
    );
    localNodeDegree.clear();
    localNodeDegree.shrink_to_fit();

    // recv node degrees and its global ID from other hosts
    // build a list of degree of all global nodes on `global_EdgePrefixSum`
    for (uint32_t h = 0; h < numHosts - 1; h++) {
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);
      uint32_t sendingHost = p->first;
      // deserialize 
      std::vector<uint64_t> recvNodeDegree;
      std::vector<uint64_t> recvNodeGlobalID;
      galois::runtime::gDeserialize(p->second, recvNodeDegree);
      galois::runtime::gDeserialize(p->second, recvNodeGlobalID);

      galois::do_all(
        galois::iterate((size_t) 0, recvNodeDegree.size()),
        [this, &recvNodeDegree, &recvNodeGlobalID](size_t n) {
          global_EdgePrefixSum[recvNodeGlobalID[n]] += recvNodeDegree[n];
        }
      );
    }

    // global_EdgePrefixSum has degree info now, so could compute prefixsum in place
    for (size_t h = 1; h < numGlobalNodes; h++) {
      global_EdgePrefixSum[h] += global_EdgePrefixSum[h-1];
    }

    // set numEdges (global size)
    setSizeEdges(global_EdgePrefixSum[numGlobalNodes-1]);

    galois::gDebug("global_EdgePrefixSum size: ", global_EdgePrefixSum.size());
    galois::gDebug("global_EdgePrefixSum value: ", global_EdgePrefixSum[99218]);

    increment_evilPhase();
  }

public:
  template<typename WMDBufferedGraph_EdgeType, typename WMDBufferedGraph_NodeType>
  friend class WMDBufferedGraph;

  #ifdef GRAPH_PROFILE
  std::atomic<std::uint64_t> remote_file_read_size=0;
  std::atomic<std::uint64_t> local_file_read_size=0;
  std::atomic<std::uint64_t> local_seq_write_size=0;
  std::atomic<std::uint64_t> local_rand_write_size=0;
  std::atomic<std::uint64_t> local_seq_read_size=0;
  std::atomic<std::uint64_t> local_rand_read_size=0;
  std::atomic<std::uint64_t> local_seq_write_count=0;
  std::atomic<std::uint64_t> local_rand_write_count=0;
  std::atomic<std::uint64_t> local_seq_read_count=0;
  std::atomic<std::uint64_t> local_rand_read_count=0;
  // remote fields are omit since there is no MPI in this file
  #endif

  WMDOfflineGraph() {}

  WMDOfflineGraph(const std::string& name) : OfflineGraph() {
    auto& net = galois::runtime::getSystemNetworkInterface();
    hostID = net.ID;
    numHosts = net.Num;

    galois::gDebug("[", hostID, "] loadGraphFile!");
    loadGraphFile(name);
    galois::gDebug("[", hostID, "] exchangeLocalNodeSize!");
    exchangeLocalNodeSize();
    galois::gDebug("[", hostID, "] exchangeLocalID!");
    exchangeLocalID();
    galois::gDebug("[", hostID, "] relabelTokenToID!");
    relabelTokenToID();  // local_tokenToID and local_tokenToEdgesIdx is cleared since then
    galois::gDebug("[", hostID, "] computeEdgePrefixSum!");
    computeEdgePrefixSum();
  }

  /**
   * Accesses the prefix sum of degree up to node `n`.
   *
   * @param N global ID of node
   * @returns The value located at index n in the edge prefix sum array
   */
  uint64_t operator[](uint64_t N) { return global_EdgePrefixSum[N]; }
 
  size_t edgeSize() const { return sizeof(EdgeType); }

  iterator begin() { return iterator(0); }

  iterator end() { return iterator(size()); }
 
  /**
   * return the end idx of edges of node N
   * 
   * @param N global ID of node
   * @return edge_iterator
   */
  edge_iterator edge_begin(uint64_t N) {
    if (N == 0)
      return edge_iterator(0);
    else
      return edge_iterator(global_EdgePrefixSum[N - 1]);
  }
  
  /**
   * return the begin idx of edges of node N
   * 
   * @param N global ID of node
   * @return edge_iterator 
   */
  edge_iterator edge_end(uint64_t N) { 
    return edge_iterator(global_EdgePrefixSum[N]);
  }

  /**
   * Returns 2 ranges (one for nodes, one for edges) for a particular division.
   * The ranges specify the nodes/edges that a division is responsible for. The
   * function attempts to split them evenly among threads given some kind of
   * weighting
   *
   * @param nodeWeight weight to give to a node in division
   * @param edgeWeight weight to give to an edge in division
   * @param id Division number you want the ranges for
   * @param total Total number of divisions
   * @param scaleFactor Vector specifying if certain divisions should get more
   * than other divisions
   */
  auto divideByNode(size_t nodeWeight, size_t edgeWeight, size_t id,
                    size_t total,
                    std::vector<unsigned> scaleFactor = std::vector<unsigned>())
      -> GraphRange {
    return galois::graphs::divideNodesBinarySearch<WMDOfflineGraph>(
        size(), sizeEdges(), nodeWeight, edgeWeight, id, total, *this,
        scaleFactor);
  }

  /**
   * Release memory used by EdgePrefixSum
   * After that, calls to `edge_begin` and `edge_end` will be invalid
   */
  void clearEdgePrefixSumInfo() {
    global_EdgePrefixSum.clear();
    global_EdgePrefixSum.shrink_to_fit();
  }

  #ifdef GRAPH_PROFILE
  void print_profile(std::ofstream &output) {
    int id = galois::runtime::getSystemNetworkInterface().ID;

    output << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:remote_file_read_size=" << remote_file_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_file_read_size=" << local_file_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_seq_write_size=" << local_seq_write_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_rand_write_size=" << local_rand_write_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_seq_read_size=" << local_seq_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_rand_read_size=" << local_rand_read_size << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_seq_write_count=" << local_seq_write_count << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_rand_write_count=" << local_rand_write_count << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_seq_read_count=" << local_seq_read_count << std::endl;
    output << "PROFILE: " << "[" << id << "] " << "WMDOfflineGraph:local_rand_read_count=" << local_rand_read_count << std::endl;
  }
  #endif
};

/**
 * Class that loads a portion of a Galois graph from disk directly into
 * memory buffers for access.
 *
 * @tparam EdgeType type of the edge data
 */
template <typename NodeType, typename EdgeType>
class WMDBufferedGraph : public BufferedGraph<EdgeType> {
private:
  typedef boost::counting_iterator<uint64_t> iterator;

  // Edge iterator typedef
  using EdgeIterator = boost::counting_iterator<uint64_t>;

  // specifies whether or not the graph is loaded
  bool graphLoaded = false;

  // size of the entire graph (not just locallly loaded portion)
  uint32_t globalSize = 0;
  // number of edges in the entire graph (not just locallly loaded portion)
  uint64_t globalEdgeSize = 0;

  // number of nodes loaded into this graph
  uint32_t numLocalNodes = 0;
  // number of edges loaded into this graph
  uint64_t numLocalEdges = 0;
  // offset of local to global node id
  uint64_t nodeOffset = 0;

  // start/end global ID of local node in each host 
  std::vector<std::pair<uint64_t, uint64_t>> nodeRange;

  uint32_t hostID;
  uint32_t numHosts;

  // CSR representation of edges
  std::vector<uint64_t> offsets;   // offsets[numLocalNodes] point to end of edges
  std::vector<EdgeType> edges;

  /**
   * Exchange range of local node with other hosts
  */
  void exchangeNodeRange() {
    auto& net = galois::runtime::getSystemNetworkInterface();

    // send node range to other hosts
    std::pair<uint64_t, uint64_t> nodeRangeToSend = nodeRange[hostID];
    for (uint32_t h = 0; h < numHosts; ++h) {
      if (h == hostID) {
        continue;
      }

      galois::runtime::SendBuffer sendBuffer;
      galois::runtime::gSerialize(sendBuffer, nodeRangeToSend);
      net.sendTagged(h, galois::runtime::evilPhase, sendBuffer);
    }

    // recv node range from other hosts
    for (uint32_t h = 0; h < numHosts - 1; h++) {
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);
      uint32_t sendingHost = p->first;

      galois::runtime::gDeserialize(p->second, nodeRange[sendingHost]);
    }

    increment_evilPhase();
  }

  /**
   * Gather local edges from other hosts to this host
   * Will update numLocalEdges
  */ 
  void gatherEdges(std::vector<std::vector<EdgeType>> &localEdges, std::vector<uint64_t> &local_EdgesIdxToID) {
    auto& net = galois::runtime::getSystemNetworkInterface();

    // prepare data to send for all hosts
    // each host will receive its edges and corresponding node ID list
    galois::gDebug("[", hostID, "] ", "prepare data!");
    std::vector<std::vector<std::vector<EdgeType>>> edgesToSend(numHosts, std::vector<std::vector<EdgeType>>());
    std::vector<std::vector<uint64_t>> IDofEdgesToSend(numHosts, std::vector<uint64_t>());

    galois::do_all(
      galois::iterate((uint64_t) 0, (uint64_t) numHosts),
      [this, &edgesToSend, &IDofEdgesToSend, &local_EdgesIdxToID, &localEdges](uint64_t i) {
        uint64_t start = nodeRange[i].first;
        uint64_t end = nodeRange[i].second;
        for (size_t j = 0; j < local_EdgesIdxToID.size(); j++) {
          auto id = local_EdgesIdxToID[j];
          if (id >= start && id < end) {
            IDofEdgesToSend[i].emplace_back(id);
            edgesToSend[i].push_back(std::move(localEdges[j]));
          }
        };
      }
    );

    // send edges to other hosts
    galois::gDebug("[", hostID, "] ", "send edges!.");

    for (uint32_t h = 0; h < numHosts; h++) {
      if (h == hostID) continue;
      assert(edgesToSend[h].size() == IDofEdgesToSend[h].size());
      galois::runtime::SendBuffer sendBuffer;
      galois::runtime::gSerialize(sendBuffer, edgesToSend[h]);
      galois::runtime::gSerialize(sendBuffer, IDofEdgesToSend[h]);
      galois::gDebug("[", hostID, "] ", "send to ", h, " edgesToSend size: ", edgesToSend[h].size());
      net.sendTagged(h, galois::runtime::evilPhase, sendBuffer);
    }      

    // prepare initial local edges
    // copy avaliable edges to there
    nodeOffset = nodeRange[hostID].first;
    std::vector<std::vector<EdgeType>> newLocalEdges(numLocalNodes);
    galois::do_all(
      galois::iterate((size_t) 0, IDofEdgesToSend[hostID].size()),
      [this, &newLocalEdges, &IDofEdgesToSend, &edgesToSend](size_t i) {
        newLocalEdges[IDofEdgesToSend[hostID][i] - nodeOffset] = std::move(edgesToSend[hostID][i]);
      }
    );

    localEdges = std::move(newLocalEdges);
  
    // recive edge list from other hosts
    for (uint32_t i = 0; i < (numHosts - 1); i++) {
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);
      uint32_t sendingHost = p->first;

      std::vector<std::vector<EdgeType>> edgeList;
      std::vector<uint64_t> IDofEdges;

      galois::runtime::gDeserialize(p->second, edgeList); 
      galois::runtime::gDeserialize(p->second, IDofEdges);

      assert(edgeList.size() == IDofEdges.size());
      galois::gDebug("[", hostID, "] recv from ", sendingHost, " edgeList size: ", edgeList.size());

      // merge edge list
      // ref: https://stackoverflow.com/questions/9778238/move-two-vectors-togethe
      // TODO: profile this step
      galois::do_all(
        galois::iterate((size_t) 0, IDofEdges.size()),
        [this, &edgeList, &IDofEdges, &localEdges](size_t i) {
          auto& arrToMerge = localEdges[IDofEdges[i] - nodeOffset];
          arrToMerge.reserve(arrToMerge.size() + edgeList[i].size());
          arrToMerge.insert(arrToMerge.end(), std::make_move_iterator(edgeList[i].begin()),
            std::make_move_iterator(edgeList[i].end()));
        }
      );
    }
    
    increment_evilPhase();
  }


  /**
   * Flatten the 2D vector localEdges into a CSR edge list
   * Will compute edge size and build CSR edge offset mapping
  */
  void flattenEdges(std::vector<std::vector<EdgeType>> &localEdges) {
      // build CSR edge offset
      offsets.resize(numLocalNodes + 1);
      uint64_t counter = 0;
      for (size_t i = 0; i < localEdges.size(); i++) {
        offsets[i+1] += localEdges[i].size() + offsets[i];
      }
      numLocalEdges = offsets[numLocalNodes];

      // build flatten edge list
      edges.resize(numLocalEdges);
      galois::do_all(
        galois::iterate((size_t) 0, localEdges.size()),
        [this, &localEdges](size_t i) {
          std::move(localEdges[i].begin(), localEdges[i].end(), 
            edges.begin() + offsets[i]);
        }
      );
  }

public:
  WMDBufferedGraph() : BufferedGraph<EdgeType>() {}

  // copy not allowed
  //! disabled copy constructor
  WMDBufferedGraph(const WMDBufferedGraph&) = delete;
  //! disabled copy constructor operator
  WMDBufferedGraph& operator=(const WMDBufferedGraph&) = delete;
  // move not allowed
  //! disabled move operator
  WMDBufferedGraph(WMDBufferedGraph&&) = delete;
  //! disabled move constructor operator
  WMDBufferedGraph& operator=(WMDBufferedGraph&&) = delete;

  /**
   * Gets the number of global nodes in the graph
   * @returns the total number of nodes in the graph (not just local loaded
   * nodes)
   */
  uint32_t size() const { return globalSize; }

  /**
   * Gets the number of global edges in the graph
   * @returns the total number of edges in the graph (not just local loaded
   * edges)
   */
  uint32_t sizeEdges() const { return globalEdgeSize; }

  /**
   * Gets the number of local edges in the graph
   * @returns the total number of edges in the local graph
   */
  uint32_t sizeLocalEdges() const { return numLocalEdges; }

  //! @returns node offset of this buffered graph
  uint64_t getNodeOffset() const { return nodeOffset; }

  /**
   * Given a node/edge range to load, loads the specified portion of the graph
   * into memory buffers from OfflineGraph.
   *
   * @param srcGraph the OfflineGraph to load from
   * @param nodeStart First node to load
   * @param nodeEnd Last node to load, non-inclusive
   * @param numGlobalNodes Total number of nodes in the graph
   * @param numGlobalEdges Total number of edges in the graph
   */
  void loadPartialGraph(WMDOfflineGraph<NodeType, EdgeType>& srcGraph, uint64_t nodeStart,
                        uint64_t nodeEnd, uint64_t numGlobalNodes, 
                        uint64_t numGlobalEdges) {
    if (graphLoaded) {
      GALOIS_DIE("Cannot load an buffered graph more than once.");
    }

    assert(nodeEnd >= nodeStart);

    // prepare meta data
    auto& net = galois::runtime::getSystemNetworkInterface();
    hostID = net.ID;
    numHosts = net.Num;

    this->nodeRange.resize(numHosts);
    this->nodeRange[hostID] = std::make_pair(nodeStart, nodeEnd);

    numLocalNodes = nodeEnd - nodeStart;
    globalSize = numGlobalNodes;
    globalEdgeSize = numGlobalEdges;

    // build local buffered graph 
    galois::gDebug("[", hostID, "] ", "exchangeNodeRange!");
    exchangeNodeRange();
    galois::gDebug("[", hostID, "] ", "gatherEdges!");
    gatherEdges(srcGraph.localEdges, srcGraph.local_EdgesIdxToID);
    galois::gDebug("[", hostID, "] ", "flattenEdges!");
    flattenEdges(srcGraph.localEdges);

    // clean unused data
    srcGraph.local_EdgesIdxToID.clear();
    srcGraph.local_EdgesIdxToID.shrink_to_fit();
    srcGraph.localEdges.clear();
    srcGraph.localEdges.shrink_to_fit();

    graphLoaded = true;

    galois::gDebug("[", hostID, "] ", "exchangeNodeRange!");
    galois::gDebug("[", hostID, "] ", "BufferedGraph built, nodes: ", numLocalNodes, ", edges: ", numLocalEdges);
  }

  /**
   * Gather local nodes data (mirror + master nodes) from other hosts to this host
   * And save data to graph
   * 
   * @param srcGraph the OfflineGraph owns node data (will be cleared after this call)
   * @param proxiesOnHosts a list of bit vector which indicates node on that hosts (include mirror and master nodes)
   * @param totalLocalNodes the total number of local nodes this host should have (include mirror and master nodes)
  */ 
  void gatherNodes(WMDOfflineGraph<NodeType, EdgeType>& srcGraph, 
                  galois::graphs::LS_LC_CSR_64_Graph<NodeType, EdgeType, true>& dstGraph, 
                  std::vector<galois::DynamicBitSet>& proxiesOnHosts, uint64_t totalLocalNodes, 
                  std::unordered_map<uint64_t, uint32_t> globalToLocalMap) {
    auto& net = galois::runtime::getSystemNetworkInterface();
    auto& localNodes = srcGraph.localNodes;

    // prepare data to send for all hosts
    // each host will receive its nodes and corresponding node global ID list
    galois::gDebug("[", hostID, "] ", "prepare node data!");
    std::vector<std::vector<NodeType>> nodesToSend(numHosts, std::vector<NodeType>());
    std::vector<std::vector<uint64_t>> IDofNodesToSend(numHosts, std::vector<uint64_t>());

    uint64_t globalIDOffset = srcGraph.offset[hostID];
    uint64_t numNodes = srcGraph.localNodeSize[hostID];
    galois::do_all(
      galois::iterate((uint64_t) 0, (uint64_t) numHosts),
      [this, &nodesToSend, &IDofNodesToSend, &localNodes, &proxiesOnHosts, globalIDOffset, numNodes](uint64_t i) {
        if (i != hostID) {
          auto& proxiesOnThatHost = proxiesOnHosts[i];
          for (uint64_t j = 0; j < numNodes; j++) {
            uint64_t gid = j + globalIDOffset;
            if (proxiesOnThatHost.test(gid)) {
              IDofNodesToSend[i].emplace_back(gid);
              nodesToSend[i].push_back(std::move(localNodes[j]));
            }
          };
        }
      }
    );

    // send nodes to other hosts
    galois::gDebug("[", hostID, "] ", "send nodes!");

    for (uint32_t h = 0; h < numHosts; h++) {
      if (h == hostID) continue;
      assert(nodesToSend[h].size() == IDofNodesToSend[h].size());
      galois::runtime::SendBuffer sendBuffer;
      galois::runtime::gSerialize(sendBuffer, nodesToSend[h]);
      galois::runtime::gSerialize(sendBuffer, IDofNodesToSend[h]);
      galois::gDebug("[", hostID, "] ", "send to ", h, " nodesToSend size: ", nodesToSend[h].size());
      net.sendTagged(h, galois::runtime::evilPhase, sendBuffer);
    }      

    // prepare initial local nodes
    // copy avaliable nodes to there
    #ifndef NDEBUG
    uint64_t addedData = 0;
    #endif

    auto& proxiesOnThisHost = proxiesOnHosts[hostID];
    galois::do_all(
      galois::iterate((uint64_t) 0, numNodes),
      [globalIDOffset, &proxiesOnThisHost, &dstGraph, &globalToLocalMap, &localNodes
      #ifndef NDEBUG
      , &addedData
      #endif
      ](uint64_t i) {
        uint64_t gid = i + globalIDOffset;
        if (proxiesOnThisHost.test(gid)) {
          dstGraph.getData(globalToLocalMap[gid]) = std::move(localNodes[i]);
          #ifndef NDEBUG
          addedData++;
          #endif
        } 
      }
    );
  
    // recive nodes from other hosts
    for (uint32_t i = 0; i < (numHosts - 1); i++) {
      decltype(net.recieveTagged(galois::runtime::evilPhase, nullptr)) p;
      do {
        p = net.recieveTagged(galois::runtime::evilPhase, nullptr);
      } while (!p);
      uint32_t sendingHost = p->first;

      std::vector<NodeType> nodeRecv;
      std::vector<uint64_t> IDofNodeRecv;

      galois::runtime::gDeserialize(p->second, nodeRecv); 
      galois::runtime::gDeserialize(p->second, IDofNodeRecv);

      assert(nodeRecv.size() == IDofNodeRecv.size());
      galois::gDebug("[", hostID, "] recv from ", sendingHost, " nodeRecv size: ", nodeRecv.size());

      galois::do_all(
        galois::iterate((size_t) 0, IDofNodeRecv.size()),
        [this, &nodeRecv, &IDofNodeRecv, &dstGraph, &globalToLocalMap
        #ifndef NDEBUG
        , &addedData
        #endif
        ](size_t i) {
          dstGraph.getData(globalToLocalMap[IDofNodeRecv[i]]) = std::move(nodeRecv[i]);
          #ifndef NDEBUG
          addedData++;
          #endif
        }
      );
    }

    #ifndef NDEBUG
    assert(addedData == totalLocalNodes);
    #endif
    
    increment_evilPhase();

    // clean unused memory
    srcGraph.localNodes.clear();
    srcGraph.localNodes.shrink_to_fit();
    srcGraph.offset.clear();
    srcGraph.offset.shrink_to_fit();
  }

  // NOTE: for below methods, it return local edge id instead of global id

  /**
   * Get the index to the first edge of the provided node THAT THIS GRAPH
   * HAS LOADED (not necessary the first edge of it globally).
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns a LOCAL edge id iterator
   */
  EdgeIterator edgeBegin(uint64_t globalNodeID) {
    assert(nodeOffset <= globalNodeID);
    assert(globalNodeID < (nodeOffset + numLocalNodes));
    return EdgeIterator(offsets[globalNodeID - nodeOffset]);
  }

  /**
   * Get the index to the first edge of the node after the provided node.
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns a LOCAL edge id iterator
   */
  EdgeIterator edgeEnd(uint64_t globalNodeID) {
    assert(nodeOffset <= globalNodeID);
    assert(globalNodeID < (nodeOffset + numLocalNodes));
    return EdgeIterator(offsets[globalNodeID - nodeOffset + 1]);
  }

  /**
   * Get the global node id of the destination of the provided edge.
   *
   * @param localEdgeID the local edge id of the edge to get the destination
   * for (should obtain from edgeBegin/End)
   */
  uint64_t edgeDestination(uint64_t localEdgeID) {
    assert(localEdgeID < numLocalEdges);
    return edges[localEdgeID].dst_glbid;
  }

  /**
   * Get the edge data of some edge.
   *
   * @param localEdgeID the local edge id of the edge to get the data of
   * @returns the edge data of the requested edge id
   */
  template <typename K = EdgeType,
            typename std::enable_if<!std::is_void<K>::value>::type* = nullptr>
  EdgeType edgeData(uint64_t localEdgeID) {
    assert(localEdgeID < numLocalEdges);
    return edges[localEdgeID];
  }

  /**
   * Version of above function when edge data type is void.
   */
  template <typename K = EdgeType,
            typename std::enable_if<std::is_void<K>::value>::type* = nullptr>
  unsigned edgeData(uint64_t) {
    galois::gWarn("Getting edge data on graph when it doesn't exist\n");
    return 0;
  }

  /**
   * Get the number of edges of the node
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns number of edges
   */
  uint64_t edgeNum(uint64_t globalNodeID) {
    return offsets[globalNodeID - nodeOffset + 1] - offsets[globalNodeID - nodeOffset];
  }

  /**
   * Get the dst of edges of the node
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @param G2L the global to local id mapping
   * @returns a vector of dst local node id
   */
  std::vector<uint64_t> edgeLocalDst(uint64_t globalNodeID, std::unordered_map<uint64_t, uint32_t> &G2L) {
    std::vector<uint64_t> dst;
    auto end = offsets[globalNodeID - nodeOffset + 1];
    for (auto itr = offsets[globalNodeID - nodeOffset]; itr != end; ++itr) {
        dst.push_back(G2L[edges[itr].dst_glbid]);
    }
    return std::move(dst);
  }

  /**
   * Get the data of edges of the node
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns a pointer to the first edges of the node in the buffer
   */
  EdgeType *edgeDataPtr(uint64_t globalNodeID) {
    return edges.data() + offsets[globalNodeID - nodeOffset];
  }

  /**
   * Free all of the in memory buffers in this object.
   */
  void resetAndFree() {
    offsets.clear();
    offsets.shrink_to_fit();
    edges.clear();
    edges.shrink_to_fit();
  }
};
} // namespace graphs
} // namespace galois
#endif
