// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace gls {

namespace wmd {

enum class TYPES {
  PERSON,
  FORUMEVENT,
  FORUM,
  PUBLICATION,
  TOPIC,
  PURCHASE,
  SALE,
  AUTHOR,
  WRITTENBY,
  INCLUDES,
  INCLUDEDIN,
  HASTOPIC,
  TOPICIN,
  HASORG,
  ORGIN,
  DEVICE,
  FRIEND,
  USES,
  COMMUNICATION,
  NONE
};

} // namespace wmd

using ParsedUID = uint64_t;

template <typename V, typename E>
struct ParsedGraphStructure {
  ParsedGraphStructure() : isNode(false), isEdge(false) {}
  explicit ParsedGraphStructure(V node_)
      : node(node_), isNode(true), isEdge(false) {}
  explicit ParsedGraphStructure(std::vector<E> edges_)
      : edges(edges_), isNode(false), isEdge(true) {}

  V node;
  std::vector<E> edges;
  bool isNode;
  bool isEdge;
};

template <typename V, typename E>
class FileParser {
public:
  virtual const std::vector<std::string>& GetFiles()                = 0;
  virtual ParsedGraphStructure<V, E> ParseLine(char* line,
                                               uint64_t lineLength) = 0;
  static std::vector<std::string> SplitLine(const char* line,
                                            uint64_t lineLength, char delim,
                                            uint64_t numTokens) {
    uint64_t ndx = 0, start = 0, end = 0;
    std::vector<std::string> tokens(numTokens);

    for (; end < lineLength - 1; end++) {
      if (line[end] == delim) {
        tokens[ndx] = std::string(line + start, end - start);
        start       = end + 1;
        ndx++;
      }
    }

    tokens[numTokens - 1] =
        std::string(line + start, end - start); // flush last token
    return tokens;
  }
};

template <typename V, typename E>
class WMDParser : public FileParser<V, E> {
public:
  explicit WMDParser(std::vector<std::string> files)
      : csvFields_(10), files_(files) {}
  WMDParser(uint64_t csvFields, std::vector<std::string> files)
      : csvFields_(csvFields), files_(files) {}

  const std::vector<std::string>& GetFiles() override { return files_; }
  ParsedGraphStructure<V, E> ParseLine(char* line,
                                       uint64_t lineLength) override {
    std::vector<std::string> tokens =
        this->SplitLine(line, lineLength, ',', csvFields_);

    if (tokens[0] == "Person") {
      ParsedUID uid = std::stoull(tokens[1]);
      return ParsedGraphStructure<V, E>(V(uid, 0, wmd::TYPES::PERSON));
    } else if (tokens[0] == "ForumEvent") {
      ParsedUID uid = std::stoull(tokens[4]);
      return ParsedGraphStructure<V, E>(V(uid, 0, wmd::TYPES::FORUMEVENT));
    } else if (tokens[0] == "Forum") {
      ParsedUID uid = std::stoull(tokens[3]);
      return ParsedGraphStructure<V, E>(V(uid, 0, wmd::TYPES::FORUM));
    } else if (tokens[0] == "Publication") {
      ParsedUID uid = std::stoull(tokens[5]);
      return ParsedGraphStructure<V, E>(V(uid, 0, wmd::TYPES::PUBLICATION));
    } else if (tokens[0] == "Topic") {
      ParsedUID uid = std::stoull(tokens[6]);
      return ParsedGraphStructure<V, E>(V(uid, 0, wmd::TYPES::TOPIC));
    } else { // edge type
      wmd::TYPES inverseEdgeType;
      if (tokens[0] == "Sale") {
        inverseEdgeType = wmd::TYPES::PURCHASE;
      } else if (tokens[0] == "Author") {
        inverseEdgeType = wmd::TYPES::WRITTENBY;
      } else if (tokens[0] == "Includes") {
        inverseEdgeType = wmd::TYPES::INCLUDEDIN;
      } else if (tokens[0] == "HasTopic") {
        inverseEdgeType = wmd::TYPES::TOPICIN;
      } else if (tokens[0] == "HasOrg") {
        inverseEdgeType = wmd::TYPES::ORGIN;
      } else {
        // skip nodes
        return ParsedGraphStructure<V, E>();
      }
      std::vector<E> edges;
      E edge(tokens);

      // insert inverse edges to the graph
      E inverseEdge    = edge;
      inverseEdge.type = inverseEdgeType;
      std::swap(inverseEdge.src, inverseEdge.dst);
      std::swap(inverseEdge.src_type, inverseEdge.dst_type);

      edges.emplace_back(edge);
      edges.emplace_back(inverseEdge);

      return ParsedGraphStructure<V, E>(edges);
    }
  }

private:
  uint64_t csvFields_;
  std::vector<std::string> files_;
};

} // namespace gls
