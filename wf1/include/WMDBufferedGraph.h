/**
 * @file WMDBufferedGraph.h
 *
 * Contains the implementation of WMDBufferedGraph which is a galois BufferedGraph constructed from WMD dataset
 */

#ifndef WMD_BUFFERED_GRAPH_H
#define WMD_BUFFERED_GRAPH_H

#include <fstream>
#include <unordered_map>
#include <atomic>

#include <boost/iterator/counting_iterator.hpp>

#include "galois/config.h"
#include "galois/gIO.h"
#include "galois/Reduction.h"

#include "graphTypes.h"
#include "data_types.h"

namespace galois {
namespace graphs {

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

/**
 * Inherit from OffilineGraph only to make it compatible with Partitioner 
 * Internal logit are completed different
 */
class WMDOfflineGraph : public OfflineGraph {
public:
  #ifdef GRAPH_PROFILE
  std::atomic<std::uint64_t> remote_file_access=0;
  std::atomic<std::uint64_t> local_file_access=0;
  std::atomic<std::uint64_t> local_seq_write=0;
  std::atomic<std::uint64_t> local_rand_write=0;
  std::atomic<std::uint64_t> local_seq_read=0;
  std::atomic<std::uint64_t> local_rand_read=0;
  std::atomic<std::uint64_t> remote_seq_read=0;
  std::atomic<std::uint64_t> remote_seq_write=0;
  std::atomic<std::uint64_t> remote_rand_read=0;
  std::atomic<std::uint64_t> remote_rand_write=0;
  #endif

  std::unordered_map<uint64_t, uint64_t> tokenToGlobalID;

  typedef boost::counting_iterator<uint64_t> edge_iterator;
  typedef uint64_t GraphNode;

  WMDOfflineGraph() {}

  /**
   * Load the out indices (i.e. where a particular node's edges begin in the
   * array of edges) from the file.
   *
   * @param graphFile loaded file for the graph
   */
  void loadMetaData(std::ifstream& graphFile) {
    std::string line;
    uint64_t id_counter = 0;
    uint64_t edge_counter = 0;

    // Pass 1: get token to global id mapping
    while(getline(graphFile, line)) {
      if (line[0] == '#') continue;                                // skip comments
      std::vector<std::string> tokens = split(line, ',', 10);     // delimiter and # tokens set for wmd data file

      #ifdef GRAPH_PROFILE
      local_rand_write += tokens.size();
      local_rand_read += 3;
      local_rand_write += 1;
      #endif
      if (tokens[0] == "Person") {
        tokenToGlobalID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1])] = id_counter++;
      } else if (tokens[0] == "ForumEvent") {
        tokenToGlobalID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4])] = id_counter++;
      } else if (tokens[0] == "Forum") {
        tokenToGlobalID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3])] = id_counter++;
      } else if (tokens[0] == "Publication") {
        tokenToGlobalID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5])] = id_counter++;
      } else if (tokens[0] == "Topic") {
        tokenToGlobalID[shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6])] = id_counter++;
      } else {
        edge_counter += 2;
        #ifdef GRAPH_PROFILE
        local_rand_read -= 2;
        local_rand_write -= 1;
        #endif
      }
    }
    setSize(id_counter);
    setSizeEdges(edge_counter);

    graphFile.clear();
    graphFile.seekg(0);

    #ifdef GRAPH_PROFILE
    remote_file_access++;
    #endif
  }

    /**
   * Deleted API
   */
  edge_iterator edge_begin(GraphNode N) {
    GALOIS_DIE("not allowed to call a deleted API");
  }
  
  /**
   * Deleted API
   */
  edge_iterator edge_end(GraphNode N) { 
    GALOIS_DIE("not allowed to call a deleted API");
  }

  uint64_t operator[](uint64_t n) { GALOIS_DIE("not allowed to call a deleted API"); }
};

/**
 * Class that loads a portion of a Galois graph from WMD dataset file into
 * memory buffers for access.
 *
 * @tparam EdgeDataType type of the edge data
 */
template <typename EdgeDataType>
class WMDBufferedGraph : public BufferedGraph<EdgeDataType> {
private:
  #ifdef GRAPH_PROFILE
  std::atomic<std::uint64_t> remote_file_access=0;
  std::atomic<std::uint64_t> local_file_access=0;
  std::atomic<std::uint64_t> local_seq_write=0;
  std::atomic<std::uint64_t> local_rand_write=0;
  std::atomic<std::uint64_t> local_seq_read=0;
  std::atomic<std::uint64_t> local_rand_read=0;
  std::atomic<std::uint64_t> remote_seq_read=0;
  std::atomic<std::uint64_t> remote_seq_write=0;
  std::atomic<std::uint64_t> remote_rand_read=0;
  std::atomic<std::uint64_t> remote_rand_write=0;
  #endif

  std::ifstream graphFile;
  
  //! size of the entire graph (not just locallly loaded portion)
  uint64_t globalSize = 0;
  //! number of edges in the entire graph (not just locallly loaded portion)
  uint64_t globalEdgeSize = 0;

  //! number of nodes loaded into this graph
  uint64_t numLocalNodes = 0;
  //! number of edges loaded into this graph
  uint64_t numLocalEdges = 0;

  //! specifies the node range
  uint64_t nodeStart = 0;
  uint64_t nodeEnd = 0;

  //! specifies whether or not the graph is loaded
  bool graphLoaded = false;

  std::unordered_multimap<uint64_t, EdgeDataType> GlobalIDToEdges;  // key: global node id, value: edges
                        
public:
  // data structure for loading info graph disk
  WMDOfflineGraph offlineGraph;

  WMDBufferedGraph() {}

  ~WMDBufferedGraph() noexcept {}

  /**
   * number of node in complete graph
   */
  uint64_t size() {
    return globalSize;
  }

  /**
   * number of edges in complete graph
   */
  uint64_t sizeEdges() {
    return globalEdgeSize;
  }

   /**
   * number of local edges in complete graph
   */
  uint64_t sizeLocalEdges() {
    return numLocalEdges;
  }

  /**
   * Deleted API
   */
  void loadGraph(const std::string& filename) {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Given a node/edge range to load, loads the specified portion of the graph
   * into memory buffers using read.
   *
   * @param filename name of graph to load; should be in WMD dataset format
   */
  void loadPartialGraph(const std::string& filename) {
    if (graphLoaded) {
      GALOIS_DIE("Cannot load an buffered graph more than once.");
    }
    assert(nodeEnd >= nodeStart);

    graphFile = std::ifstream(filename.c_str());
  
    offlineGraph.loadMetaData(graphFile);
    globalSize = offlineGraph.size();
    globalEdgeSize = offlineGraph.sizeEdges();

    graphLoaded = true;
  }

  /**
   * Deleted API
   */
  void loadPartialGraph(const std::string& filename, uint64_t nodeStart,
                        uint64_t nodeEnd, uint64_t edgeStart, uint64_t edgeEnd,
                        uint64_t numGlobalNodes, uint64_t numGlobalEdges) {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Given a node range to load, loads edges from the specified portion of the graph
   * into memory buffers using read.
   *
   * @param nodeStart First node in the range
   * @param nodeEnd Last node in the range, non-inclusive
   */
  void loadEdges(uint64_t nodeStart, uint64_t nodeEnd) {
    std::string line;

    // update meta info
    this->nodeStart = nodeStart;
    this->nodeEnd = nodeEnd;
    numLocalNodes = nodeEnd - nodeStart;

    while(getline(graphFile, line)) {
      if (line[0] == '#') continue;                                // skip comments
      std::vector<std::string> tokens = split(line, ',', 10);     // delimiter and # tokens set for wmd data file

      #ifdef GRAPH_PROFILE
      local_rand_write += tokens.size();
      local_rand_read += 1;
      #endif
      // prepare the inverse edge type based on its types
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
      EdgeDataType edge(tokens);

      uint64_t src_gid = offlineGraph.tokenToGlobalID[edge.src];
      uint64_t dst_gid = offlineGraph.tokenToGlobalID[edge.dst];
      edge.src_glbid = src_gid;
      edge.dst_glbid = dst_gid;
      #ifdef GRAPH_PROFILE
      local_rand_read += 3*2;
      local_rand_write += 2 + sizeof(EdgeDataType)/8;
      #endif

      if (src_gid >= nodeStart && src_gid < nodeEnd) {
        GlobalIDToEdges.insert({edge.src_glbid, edge});
        #ifdef GRAPH_PROFILE
        local_rand_read += 2;
        local_seq_write += sizeof(EdgeDataType)/8;
        #endif
      }

      if (dst_gid >= nodeStart && dst_gid < nodeEnd) {
        // insert inverse edge if in range too 
        EdgeDataType inverseEdge = edge;
        inverseEdge.type = inverseEdgeType;
        std::swap(inverseEdge.src, inverseEdge.dst);
        std::swap(inverseEdge.src_glbid, inverseEdge.dst_glbid);
        std::swap(inverseEdge.src_type, inverseEdge.dst_type);
        GlobalIDToEdges.insert({inverseEdge.src_glbid, inverseEdge});
        #ifdef GRAPH_PROFILE
        local_rand_write += 2*3 + 1 + 2*(sizeof(EdgeDataType)/8);
        local_rand_read += 1;
        #endif
      }
    }

    numLocalEdges = GlobalIDToEdges.size();

    #ifdef GRAPH_PROFILE
    local_file_access++;
    #endif
  }

  using EdgeIterator = typename std::unordered_multimap<uint64_t, EdgeDataType>::iterator;
  /**
   * Get the index to the first edge of the provided node THAT THIS GRAPH
   * HAS LOADED (not necessary the first edge of it globally).
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns a GLOBAL edge id iterator
   */
  EdgeIterator edgeBegin(uint64_t globalNodeID) {
    #ifdef GRAPH_PROFILE
    local_rand_read += GlobalIDToEdges.count(globalNodeID);
    #endif
    return GlobalIDToEdges.equal_range(globalNodeID).first;
  }

  /**
   * Get the index to the first edge of the node after the provided node.
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns a GLOBAL edge iterator
   */
  EdgeIterator edgeEnd(uint64_t globalNodeID) {
    #ifdef GRAPH_PROFILE
    local_rand_read += GlobalIDToEdges.count(globalNodeID);
    #endif
    return GlobalIDToEdges.equal_range(globalNodeID).second;
  }

  /**
   * Get the range of edge for the node
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns a pair of GLOBAL edge iterator
   */
  std::pair<EdgeIterator, EdgeIterator> edgeRange(uint64_t globalNodeID) {
    #ifdef GRAPH_PROFILE
    local_rand_read += GlobalIDToEdges.count(globalNodeID);
    #endif
    return GlobalIDToEdges.equal_range(globalNodeID);
  }

  /**
   * Get the number of edges of the node
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @returns number of edges
   */
  uint64_t edgeNum(uint64_t globalNodeID) {
    #ifdef GRAPH_PROFILE
    local_rand_read += GlobalIDToEdges.count(globalNodeID);
    #endif
    return GlobalIDToEdges.count(globalNodeID);
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
      auto range = GlobalIDToEdges.equal_range(globalNodeID);
      for (auto itr = range.first; itr != range.second; ++itr) {
          dst.push_back(G2L[itr->second.dst_glbid]);
      }
      return dst;
  }

  /**
   * Get the data of edges of the node
   *
   * @param globalNodeID the global node id of the node to get the edge
   * for
   * @param G2L the global to local id mapping
   * @returns a vector of edge data
   */
  std::vector<EdgeDataType> edgeData(uint64_t globalNodeID, std::unordered_map<uint64_t, uint32_t> &G2L) {
      std::vector<EdgeDataType> data;
      auto range = GlobalIDToEdges.equal_range(globalNodeID);
      for (auto itr = range.first; itr != range.second; ++itr) {
          // update local id
          itr->second.src = G2L[itr->second.src_glbid];
          itr->second.dst = G2L[itr->second.dst_glbid];
          data.push_back(itr->second);
      }
      return data;
  }

  /**
   * Get the offset of node, 
   * which means how many nodes are skipped from the beginning of the graph
   * in this loaded portion of it.
   *
   * @returns node offset
   */
  uint64_t nodeOffset() const {
    return nodeStart;
  }

  /**
   * Deleted API
   */
  uint64_t edgeDestination(uint64_t globalEdgeID) {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Deleted API
   */
  template <typename K = EdgeDataType,
            typename std::enable_if<!std::is_void<K>::value>::type* = nullptr>
  EdgeDataType edgeData(uint64_t globalEdgeID) {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Deleted API
   */
  template <typename K = EdgeDataType,
            typename std::enable_if<std::is_void<K>::value>::type* = nullptr>
  unsigned edgeData(uint64_t) {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Deleted API
   */
  void resetReadCounters() {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Deleted API
   */
  uint64_t getBytesRead() {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Deleted API
   */
  void resetAndFree() {
    GALOIS_DIE("not allowed to call a deleted API");
  }

  /**
   * Deleted API
   */
  uint64_t operator[](uint64_t n) { GALOIS_DIE("not allowed to call a deleted API"); }

  #ifdef GRAPH_PROFILE
  void print_profile() {
    remote_file_access += offlineGraph.remote_file_access;
    local_file_access += offlineGraph.local_file_access;
    local_seq_write += offlineGraph.local_seq_write;
    local_rand_write += offlineGraph.local_rand_write;
    local_seq_read += offlineGraph.local_seq_read;
    local_rand_read += offlineGraph.local_rand_read;
    remote_seq_read += offlineGraph.remote_seq_read;
    remote_seq_write += offlineGraph.remote_seq_write;
    remote_rand_read += offlineGraph.remote_rand_read;
    remote_rand_write += offlineGraph.remote_rand_write;

    std::cout << "WMDBufferedGraph:remote_file_access=" << remote_file_access << std::endl;
    std::cout << "WMDBufferedGraph:local_file_access=" << local_file_access << std::endl;
    std::cout << "WMDBufferedGraph:local_seq_write=" << local_seq_write << std::endl;
    std::cout << "WMDBufferedGraph:local_rand_write=" << local_rand_write << std::endl;
    std::cout << "WMDBufferedGraph:local_seq_read=" << local_seq_read << std::endl;
    std::cout << "WMDBufferedGraph:local_rand_read=" << local_rand_read << std::endl;
    std::cout << "WMDBufferedGraph:remote_seq_read=" << remote_seq_read << std::endl;
    std::cout << "WMDBufferedGraph:remote_seq_write=" << remote_seq_write << std::endl;
    std::cout << "WMDBufferedGraph:remote_rand_read=" << remote_seq_read << std::endl;
    std::cout << "WMDBufferedGraph:remote_rand_write=" << remote_seq_write << std::endl;
  }
  #endif
};
} // namespace graphs
} // namespace galois
#endif
