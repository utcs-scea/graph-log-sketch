// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2023. University of Texas at Austin. All rights reserved.

#ifndef GRAPH_LOG_SKETCH_WF2_INCLUDE_GRAPH_DS_HPP_
#define GRAPH_LOG_SKETCH_WF2_INCLUDE_GRAPH_DS_HPP_

#include <ctime>
#include <vector>
#include <utility>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "graph_ds.hpp"
#include "galois/wmd/graphTypes.h"
#include "galois/wmd/schema.h"
#include "galois/wmd/graph.h"
#include "galois/wmd/WMDPartitioner.h"
#include "galois/graphs/GenericPartitioners.h"

#define DOUBLE shad::data_types::DOUBLE

namespace wf2 {
using GlobalNodeID = uint64_t;

class BaseVertex {
public:
  uint64_t token;
};

class PersonVertex : public BaseVertex {
public:
  time_t trans_date;

  PersonVertex() { trans_date = shad::data_types::kNullValue<time_t>; }

  explicit PersonVertex(std::vector<std::string>& tokens) {
    token = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
  }
};

class ForumEventVertex : public BaseVertex {
public:
  uint64_t forum;
  time_t date;

  ForumEventVertex() {
    forum = shad::data_types::kNullValue<uint64_t>;
    date  = shad::data_types::kNullValue<time_t>;
  }

  explicit ForumEventVertex(std::vector<std::string>& tokens) {
    forum = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
    date  = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
    token = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
  }

  // uint64_t key() { return id; }
};

class ForumVertex : public BaseVertex {
public:
  ForumVertex() {}

  explicit ForumVertex(std::vector<std::string>& tokens) {
    token = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
  }

  // uint64_t key() { return id; }
};

class PublicationVertex : public BaseVertex {
public:
  time_t date;

  PublicationVertex() { date = shad::data_types::kNullValue<time_t>; }

  explicit PublicationVertex(std::vector<std::string>& tokens) {
    date  = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
    token = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
  }

  // uint64_t key() { return id; }
};

class TopicVertex : public BaseVertex {
public:
  double lat;
  double lon;

  TopicVertex() {
    lat = shad::data_types::kNullValue<double>;
    lon = shad::data_types::kNullValue<double>;
  }

  explicit TopicVertex(std::vector<std::string>& tokens) {
    lat   = shad::data_types::encode<double, std::string, DOUBLE>(tokens[8]);
    lon   = shad::data_types::encode<double, std::string, DOUBLE>(tokens[9]);
    token = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
  }

  // uint64_t key() { return id; }
};

class NoneVertex {
public:
  NoneVertex() {}
};

class Vertex {
  union VertexUnion {
    PersonVertex person;
    ForumEventVertex forum_event;
    ForumVertex forum;
    PublicationVertex publication;
    TopicVertex topic;
    NoneVertex none;

    VertexUnion() : none(NoneVertex()) {}
    VertexUnion(PersonVertex& p) : person(p) {}
    VertexUnion(ForumEventVertex& fe) : forum_event(fe) {}
    VertexUnion(ForumVertex& f) : forum(f) {}
    VertexUnion(PublicationVertex& pub) : publication(pub) {}
    VertexUnion(TopicVertex& top) : topic(top) {}
  };

public:
  uint64_t glbid;
  uint64_t id;    // GlobalIDS: global id ... Vertices: vertex id
  uint64_t edges; // number of edges
  uint64_t start; // start index in compressed edge list
  agile::workflow1::TYPES type;
  VertexUnion v;

  Vertex() {
    id    = shad::data_types::kNullValue<uint64_t>;
    edges = 0;
    start = 0;
    type  = agile::workflow1::TYPES::NONE;
  }

  Vertex(uint64_t id_, uint64_t edges_, agile::workflow1::TYPES type_) {
    id    = id_;
    edges = edges_;
    start = 0;
    type  = type_;
  }

  Vertex(uint64_t id_, uint64_t edges_, agile::workflow1::TYPES type_,
         std::vector<std::string>& tokens) {
    id    = id_;
    edges = edges_;
    start = 0;
    type  = type_;

    switch (type_) {
    case agile::workflow1::TYPES::PERSON:
      v.person = PersonVertex(tokens);
      break;
    case agile::workflow1::TYPES::FORUMEVENT:
      v.forum_event = ForumEventVertex(tokens);
      break;
    case agile::workflow1::TYPES::FORUM:
      v.forum = ForumVertex(tokens);
      break;
    case agile::workflow1::TYPES::PUBLICATION:
      v.publication = PublicationVertex(tokens);
      break;
    case agile::workflow1::TYPES::TOPIC:
      v.topic = TopicVertex(tokens);
      break;
    default:
      v.none = NoneVertex();
    }
  }

  Vertex(uint64_t id_, uint64_t edges_, agile::workflow1::TYPES type_,
         PersonVertex& p)
      : id(id_), edges(edges_), start(0), type(type_), v(p) {}
  Vertex(uint64_t id_, uint64_t edges_, agile::workflow1::TYPES type_,
         ForumVertex& p)
      : id(id_), edges(edges_), start(0), type(type_), v(p) {}
  Vertex(uint64_t id_, uint64_t edges_, agile::workflow1::TYPES type_,
         ForumEventVertex& p)
      : id(id_), edges(edges_), start(0), type(type_), v(p) {}
  Vertex(uint64_t id_, uint64_t edges_, agile::workflow1::TYPES type_,
         TopicVertex& p)
      : id(id_), edges(edges_), start(0), type(type_), v(p) {}
  Vertex(uint64_t id_, uint64_t edges_, agile::workflow1::TYPES type_,
         PublicationVertex& p)
      : id(id_), edges(edges_), start(0), type(type_), v(p) {}

  uint64_t getToken() {
    switch (type) {
    case agile::workflow1::TYPES::PERSON:
      return this->v.person.token;
    case agile::workflow1::TYPES::FORUMEVENT:
      return this->v.forum_event.token;
    case agile::workflow1::TYPES::FORUM:
      return this->v.forum.token;
    case agile::workflow1::TYPES::PUBLICATION:
      return this->v.publication.token;
    case agile::workflow1::TYPES::TOPIC:
      return this->v.topic.token;
    }
    return 0;
  }

  void set_id(uint64_t id_) { id = id_; }
};

class PurchaseEdge {
public:
  uint64_t buyer;  // vertex id
  uint64_t seller; // vertex id
  uint64_t product;
  time_t date;

  PurchaseEdge() {
    buyer   = shad::data_types::kNullValue<uint64_t>;
    seller  = shad::data_types::kNullValue<uint64_t>;
    product = shad::data_types::kNullValue<uint64_t>;
    date    = shad::data_types::kNullValue<time_t>;
  }

  explicit PurchaseEdge(std::vector<std::string>& tokens) {
    buyer   = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[2]);
    seller  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
    product = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
    date    = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
  }

  uint64_t key() { return buyer; }
  uint64_t src() { return buyer; }
  uint64_t dst() { return seller; }
};

class SaleEdge {
public:
  uint64_t seller; // vertex id
  uint64_t buyer;  // vertex id
  uint64_t product;
  time_t date;

  SaleEdge() {
    seller  = shad::data_types::kNullValue<uint64_t>;
    buyer   = shad::data_types::kNullValue<uint64_t>;
    product = shad::data_types::kNullValue<uint64_t>;
    date    = shad::data_types::kNullValue<time_t>;
  }

  explicit SaleEdge(std::vector<std::string>& tokens) {
    seller  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
    buyer   = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[2]);
    product = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
    date    = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
  }

  uint64_t key() { return seller; }
  uint64_t src() { return seller; }
  uint64_t dst() { return buyer; }
};

class AuthorEdge {
public:
  uint64_t author; // vertex id
  uint64_t item;   // vertex id

  AuthorEdge() {
    author = shad::data_types::kNullValue<uint64_t>;
    item   = shad::data_types::kNullValue<uint64_t>;
  }

  explicit AuthorEdge(std::vector<std::string>& tokens) {
    if (tokens[4] != "") {
      author = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      item   = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
    } else {
      author = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      item   = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
    }
  }

  uint64_t key() { return author; }
  uint64_t src() { return author; }
  uint64_t dst() { return item; }
};

class IncludesEdge {
public:
  uint64_t forum;       // vertex id
  uint64_t forum_event; // vertex id

  IncludesEdge() {
    forum       = shad::data_types::kNullValue<uint64_t>;
    forum_event = shad::data_types::kNullValue<uint64_t>;
  }

  explicit IncludesEdge(std::vector<std::string>& tokens) {
    forum = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
    forum_event =
        shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
  }

  uint64_t key() { return forum; }
  uint64_t src() { return forum; }
  uint64_t dst() { return forum_event; }
};

class HasTopicEdge {
public:
  uint64_t item;  // vertex id
  uint64_t topic; // vertex id

  HasTopicEdge() {
    item  = shad::data_types::kNullValue<uint64_t>;
    topic = shad::data_types::kNullValue<uint64_t>;
  }

  explicit HasTopicEdge(std::vector<std::string>& tokens) {
    if (tokens[3] != "") {
      item  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
      topic = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
    } else if (tokens[4] != "") {
      item  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      topic = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
    } else {
      item  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      topic = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
    }
  }

  uint64_t key() { return item; }
  uint64_t src() { return item; }
  uint64_t dst() { return topic; }
};

class HasOrgEdge {
public:
  uint64_t publication;  // vertex id
  uint64_t organization; // vertex id

  HasOrgEdge() {
    publication  = shad::data_types::kNullValue<uint64_t>;
    organization = shad::data_types::kNullValue<uint64_t>;
  }

  explicit HasOrgEdge(std::vector<std::string>& tokens) {
    publication =
        shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
    organization =
        shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
  }

  uint64_t key() { return publication; }
  uint64_t src() { return publication; }
  uint64_t dst() { return organization; }
};

class NoneEdge {
public:
  NoneEdge() {}
};

class Edge {
  union EdgeUnion {
    PurchaseEdge purchase;
    SaleEdge sale;
    AuthorEdge author;
    IncludesEdge includes;
    HasTopicEdge has_topic;
    HasOrgEdge has_org;
    NoneEdge none;

    EdgeUnion() : none(NoneEdge()) {}
    EdgeUnion(PurchaseEdge& p) : purchase(p) {}
    EdgeUnion(SaleEdge& s) : sale(s) {}
    EdgeUnion(AuthorEdge& a) : author(a) {}
    EdgeUnion(IncludesEdge& inc) : includes(inc) {}
    EdgeUnion(HasTopicEdge& top) : has_topic(top) {}
    EdgeUnion(HasOrgEdge& org) : has_org(org) {}
  };

public:
  uint64_t src; // vertex id of src
  uint64_t dst; // vertex id of dst
  agile::workflow1::TYPES type;
  agile::workflow1::TYPES src_type;
  agile::workflow1::TYPES dst_type;
  uint64_t src_glbid;
  uint64_t dst_glbid;
  EdgeUnion e;

  Edge() {
    src       = shad::data_types::kNullValue<uint64_t>;
    dst       = shad::data_types::kNullValue<uint64_t>;
    type      = agile::workflow1::TYPES::NONE;
    src_type  = agile::workflow1::TYPES::NONE;
    dst_type  = agile::workflow1::TYPES::NONE;
    src_glbid = shad::data_types::kNullValue<uint64_t>;
    dst_glbid = shad::data_types::kNullValue<uint64_t>;
  }

  explicit Edge(std::vector<std::string>& tokens) {
    if (tokens[0] == "Sale") {
      src  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      dst  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[2]);
      type = agile::workflow1::TYPES::SALE;
      src_type  = agile::workflow1::TYPES::PERSON;
      dst_type  = agile::workflow1::TYPES::PERSON;
      src_glbid = shad::data_types::kNullValue<uint64_t>;
      dst_glbid = shad::data_types::kNullValue<uint64_t>;
      e.sale    = SaleEdge(tokens);
    } else if (tokens[0] == "Purchase") {
      src  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      dst  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[2]);
      type = agile::workflow1::TYPES::PURCHASE;
      src_type   = agile::workflow1::TYPES::PERSON;
      dst_type   = agile::workflow1::TYPES::PERSON;
      src_glbid  = shad::data_types::kNullValue<uint64_t>;
      dst_glbid  = shad::data_types::kNullValue<uint64_t>;
      e.purchase = PurchaseEdge(tokens);
    } else if (tokens[0] == "Author") {
      src  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      type = agile::workflow1::TYPES::AUTHOR;
      src_type  = agile::workflow1::TYPES::PERSON;
      src_glbid = shad::data_types::kNullValue<uint64_t>;
      dst_glbid = shad::data_types::kNullValue<uint64_t>;
      if (tokens[3] != "")
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
      else if (tokens[4] != "")
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      else if (tokens[5] != "")
        dst = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      if (tokens[3] != "")
        dst_type = agile::workflow1::TYPES::FORUM;
      else if (tokens[4] != "")
        dst_type = agile::workflow1::TYPES::FORUMEVENT;
      else if (tokens[5] != "")
        dst_type = agile::workflow1::TYPES::PUBLICATION;
      e.author = AuthorEdge(tokens);
    } else if (tokens[0] == "Includes") {
      src  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
      dst  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      type = agile::workflow1::TYPES::INCLUDES;
      src_type   = agile::workflow1::TYPES::FORUM;
      dst_type   = agile::workflow1::TYPES::FORUMEVENT;
      src_glbid  = shad::data_types::kNullValue<uint64_t>;
      dst_glbid  = shad::data_types::kNullValue<uint64_t>;
      e.includes = IncludesEdge(tokens);
    } else if (tokens[0] == "HasTopic") {
      dst  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      type = agile::workflow1::TYPES::HASTOPIC;
      dst_type  = agile::workflow1::TYPES::TOPIC;
      src_glbid = shad::data_types::kNullValue<uint64_t>;
      dst_glbid = shad::data_types::kNullValue<uint64_t>;
      if (tokens[3] != "")
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
      else if (tokens[4] != "")
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      else if (tokens[5] != "")
        src = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      if (tokens[3] != "")
        src_type = agile::workflow1::TYPES::FORUM;
      else if (tokens[4] != "")
        src_type = agile::workflow1::TYPES::FORUMEVENT;
      else if (tokens[5] != "")
        src_type = agile::workflow1::TYPES::PUBLICATION;
      e.has_topic = HasTopicEdge(tokens);
    } else if (tokens[0] == "HasOrg") {
      src  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      dst  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      type = agile::workflow1::TYPES::HASORG;
      src_type  = agile::workflow1::TYPES::PUBLICATION;
      dst_type  = agile::workflow1::TYPES::TOPIC;
      src_glbid = shad::data_types::kNullValue<uint64_t>;
      dst_glbid = shad::data_types::kNullValue<uint64_t>;
      e.has_org = HasOrgEdge(tokens);
    }
  }
  Edge(std::vector<std::string>& tokens, PurchaseEdge& p) : Edge(tokens) {
    e.purchase = p;
  }
  Edge(std::vector<std::string>& tokens, SaleEdge& p) : Edge(tokens) {
    e.sale = p;
  }
  Edge(std::vector<std::string>& tokens, AuthorEdge& p) : Edge(tokens) {
    e.author = p;
  }
  Edge(std::vector<std::string>& tokens, IncludesEdge& p) : Edge(tokens) {
    e.includes = p;
  }
  Edge(std::vector<std::string>& tokens, HasTopicEdge& p) : Edge(tokens) {
    e.has_topic = p;
  }
  Edge(std::vector<std::string>& tokens, HasOrgEdge& p) : Edge(tokens) {
    e.has_org = p;
  }

private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    ar& src;
    ar& dst;
    ar& type;
    ar& src_type;
    ar& dst_type;
    ar& src_glbid;
    ar& dst_glbid;
  }
};

template <typename V, typename E>
class Wf2WMDParser : public galois::graphs::FileParser<V, E> {
public:
  explicit Wf2WMDParser(std::vector<std::string> files)
      : csvFields_(10), files_(files) {}
  Wf2WMDParser(uint64_t csvFields, std::vector<std::string> files)
      : csvFields_(csvFields), files_(files) {}

  const std::vector<std::string>& GetFiles() override { return files_; }
  galois::graphs::ParsedGraphStructure<V, E>
  ParseLine(char* line, uint64_t lineLength) override {
    std::vector<std::string> tokens =
        this->SplitLine(line, lineLength, ',', csvFields_);

    if (tokens[0] == "Person") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      auto pv = PersonVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, 0, agile::workflow1::TYPES::PERSON, pv));
    } else if (tokens[0] == "ForumEvent") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      auto fev = ForumEventVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, 0, agile::workflow1::TYPES::FORUMEVENT, fev));
    } else if (tokens[0] == "Forum") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
      auto fv = ForumVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, 0, agile::workflow1::TYPES::FORUM, fv));
    } else if (tokens[0] == "Publication") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      auto pv = PublicationVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, 0, agile::workflow1::TYPES::PUBLICATION, pv));
    } else if (tokens[0] == "Topic") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      auto tv = TopicVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, 0, agile::workflow1::TYPES::TOPIC, tv));
    } else { // edge type
      agile::workflow1::TYPES inverseEdgeType;
      E edge;
      if (tokens[0] == "Sale") {
        inverseEdgeType = agile::workflow1::TYPES::PURCHASE;
        auto e          = PurchaseEdge(tokens);
        edge            = E(tokens);
      } else if (tokens[0] == "Author") {
        inverseEdgeType = agile::workflow1::TYPES::WRITTENBY;
        auto e          = AuthorEdge(tokens);
        edge            = E(tokens);
      } else if (tokens[0] == "Includes") {
        inverseEdgeType = agile::workflow1::TYPES::INCLUDEDIN;
        auto e          = IncludesEdge(tokens);
        edge            = E(tokens);
      } else if (tokens[0] == "HasTopic") {
        inverseEdgeType = agile::workflow1::TYPES::TOPICIN;
        auto e          = HasTopicEdge(tokens);
        edge            = E(tokens);
      } else if (tokens[0] == "HasOrg") {
        inverseEdgeType = agile::workflow1::TYPES::ORGIN;
        auto e          = HasOrgEdge(tokens);
        edge            = E(tokens);
      } else {
        // skip nodes
        return galois::graphs::ParsedGraphStructure<V, E>();
      }
      std::vector<E> edges;

      // insert inverse edges to the graph
      E inverseEdge    = edge;
      inverseEdge.type = inverseEdgeType;
      std::swap(inverseEdge.src, inverseEdge.dst);
      std::swap(inverseEdge.src_type, inverseEdge.dst_type);

      edges.emplace_back(edge);
      edges.emplace_back(inverseEdge);

      return galois::graphs::ParsedGraphStructure<V, E>(edges);
    }
  }

private:
  uint64_t csvFields_;
  std::vector<std::string> files_;
};

typedef galois::graphs::WMDGraph<agile::workflow1::Vertex,
                                 agile::workflow1::Edge, OECPolicy>
    Graph;

} // namespace wf2

#endif // GRAPH_LOG_SKETCH_WF2_INCLUDE_GRAPH_DS_HPP_
