#ifndef _WF2_GRAPH_HPP_
#define _WF2_GRAPH_HPP_

#include "galois/wmd/graphTypes.h"
#include "galois/wmd/schema.h"
#include <ctime>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace wf2 {

class PersonVertex {
public:
  uint64_t id;
  uint64_t glbid;

  PersonVertex () {
    id    = 0;
    glbid = 0;
  }

  PersonVertex (std::vector <std::string> & tokens) {
    id    = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
    glbid = 0;
  }

  uint64_t key() { return id; }
};

class ForumEventVertex {
public:
  uint64_t id;
  uint64_t forum;
  time_t   date;
  uint64_t glbid;

  ForumEventVertex () {
    id    = 0;
    forum = 0;
    date  = 0;
    glbid = 0;
  }

  ForumEventVertex (std::vector <std::string> & tokens) {
    id    = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[4]);
    forum = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[3]);
    date  = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
    glbid = 0;
  }

  uint64_t key() { return id; }
};

class ForumVertex {
public:
  uint64_t id;
  uint64_t glbid; 

  ForumVertex () {
    id    = 0;
    glbid = 0;
  }

  ForumVertex (std::vector <std::string> & tokens) {
    id   = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
    glbid = 0;
  }

  uint64_t key() { return id; }
};

class PublicationVertex {
public:
  uint64_t id;
  time_t   date;
  uint64_t glbid;

  PublicationVertex () {
    id    = 0;
    date  = 0;
    glbid = 0;
  }

  PublicationVertex (std::vector <std::string> & tokens) {
    id    = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
    date  = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
    glbid = 0;
  }

  uint64_t key() { return id; }
};

class TopicVertex {
public:
  uint64_t id;
  double   lat;
  double   lon;
  uint64_t glbid;

  TopicVertex () {
    id    = 0;
    lat   = 0;
    lon   = 0;
    glbid = 0;
  }

  TopicVertex (std::vector <std::string> & tokens) {
    id    = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
    lat   = shad::data_types::encode<double, std::string, DOUBLE>(tokens[8]);
    lon   = shad::data_types::encode<double, std::string, DOUBLE>(tokens[9]);
    glbid = 0;
  }

  uint64_t key() { return id; }
};

class NoneVertex {
public:
  NoneVertex () {}
};


class PurchaseEdge {
public:
  uint64_t buyer;            // vertex id
  uint64_t seller;           // vertex id
  uint64_t product;
  time_t   date;
  agile::workflow1::TYPES    src_type;
  agile::workflow1::TYPES    dst_type;

  PurchaseEdge () {
    buyer   = 0;
    seller  = 0;
    product = 0;
    date    = 0;
    src_type = agile::workflow1::TYPES::NONE;
    dst_type = agile::workflow1::TYPES::NONE;
  }

  PurchaseEdge (std::vector <std::string> & tokens) {
    buyer    = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[2]);
    seller   = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[1]);
    product  = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[6]);
    date     = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
    src_type = agile::workflow1::TYPES::PERSON;
    dst_type = agile::workflow1::TYPES::PERSON;
  }

  uint64_t key() { return buyer; }
  uint64_t src() { return buyer; }
  uint64_t dst() { return seller; }
};

class SaleEdge {
public:
  uint64_t seller;           // vertex id
  uint64_t buyer;            // vertex id
  uint64_t product;
  time_t   date;
  agile::workflow1::TYPES    src_type;
  agile::workflow1::TYPES    dst_type;

  SaleEdge () {
    seller   = 0;
    buyer    = 0;
    product  = 0;
    date     = 0;
    src_type = agile::workflow1::TYPES::NONE;
    dst_type = agile::workflow1::TYPES::NONE;
  }

  SaleEdge (std::vector <std::string> & tokens) {
    seller   = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[1]);
    buyer    = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[2]);
    product  = shad::data_types::encode<uint64_t, std::string, UINT>  (tokens[6]);
    date     = shad::data_types::encode<time_t, std::string, USDATE>(tokens[7]);
    src_type = agile::workflow1::TYPES::PERSON;
    dst_type = agile::workflow1::TYPES::PERSON;
  }

  uint64_t key() { return seller; }
  uint64_t src() { return seller; }
  uint64_t dst() { return buyer; }
};

class AuthorEdge {
public:
  uint64_t author;     // vertex id
  uint64_t item;       // vertex id
  agile::workflow1::TYPES    src_type;
  agile::workflow1::TYPES    dst_type;

  AuthorEdge () {
    author   = 0;
    item     = 0;
    src_type = agile::workflow1::TYPES::NONE;
    dst_type = agile::workflow1::TYPES::NONE;
  }

  AuthorEdge (std::vector <std::string> & tokens) {
    if (tokens[4] != "") {
      author   = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      item     = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      src_type = agile::workflow1::TYPES::PERSON;
      dst_type = agile::workflow1::TYPES::FORUMEVENT;
    } else {
      author   = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      item     = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      src_type = agile::workflow1::TYPES::PERSON;
      dst_type = agile::workflow1::TYPES::PUBLICATION;
    } 
  }

  uint64_t key() { return author; }
  uint64_t src() { return author; }
  uint64_t dst() { return item; }
};

class IncludesEdge {
public:
  uint64_t forum;            // vertex id
  uint64_t forum_event;      // vertex id
  agile::workflow1::TYPES    src_type;
  agile::workflow1::TYPES    dst_type;

  IncludesEdge () {
    forum       = 0;
    forum_event = 0;
    src_type    = agile::workflow1::TYPES::NONE;
    dst_type    = agile::workflow1::TYPES::NONE;
  }

  IncludesEdge (std::vector <std::string> & tokens) {
    forum       = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
    forum_event = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
    src_type    = agile::workflow1::TYPES::FORUM;
    dst_type    = agile::workflow1::TYPES::FORUMEVENT;
  }

  uint64_t key() { return forum; }
  uint64_t src() { return forum; }
  uint64_t dst() { return forum_event; }
};

class HasTopicEdge {
public:
  uint64_t item;      // vertex id
  uint64_t topic;     // vertex id
  agile::workflow1::TYPES    src_type;
  agile::workflow1::TYPES    dst_type;

  HasTopicEdge () {
    item     = 0;
    topic    = 0;
    src_type = agile::workflow1::TYPES::NONE;
    dst_type = agile::workflow1::TYPES::NONE;
  }

  HasTopicEdge (std::vector <std::string> & tokens) {
    if (tokens[3] != "") {
      item     = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
      topic    = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      src_type = agile::workflow1::TYPES::FORUM;
      dst_type = agile::workflow1::TYPES::TOPIC;
    } else if (tokens[4] != "") {
      item     = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      topic    = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      src_type = agile::workflow1::TYPES::FORUMEVENT;
      dst_type = agile::workflow1::TYPES::TOPIC;
    } else {
      item     = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      topic    = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      src_type = agile::workflow1::TYPES::PUBLICATION;
      dst_type = agile::workflow1::TYPES::TOPIC;
    } 
  }

  uint64_t key() { return item; }
  uint64_t src() { return item; }
  uint64_t dst() { return topic; }
};

class HasOrgEdge {
public:
  uint64_t publication;      // vertex id
  uint64_t organization;     // vertex id
  agile::workflow1::TYPES    src_type;
  agile::workflow1::TYPES    dst_type;

  HasOrgEdge () {
    publication  = 0;
    organization = 0;
    src_type     = agile::workflow1::TYPES::NONE;
    dst_type     = agile::workflow1::TYPES::NONE;
  }

  HasOrgEdge (std::vector <std::string> & tokens) {
    publication  = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
    organization = shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
    src_type     = agile::workflow1::TYPES::PUBLICATION;
    dst_type     = agile::workflow1::TYPES::TOPIC;
  }

  uint64_t key() { return publication; }
  uint64_t src() { return publication; }
  uint64_t dst() { return organization; }
};

class NoneEdge {
public:
  NoneEdge () {}
};


struct Vertex {
  union VertexUnion {
    PersonVertex person;
    ForumEventVertex forum_event;
    ForumVertex forum;
    PublicationVertex publication;
    TopicVertex topic;
    NoneVertex none;

    VertexUnion () : none(NoneVertex()) {}
    VertexUnion (PersonVertex& p) : person(p) {}
    VertexUnion (ForumEventVertex& fe) : forum_event(fe) {}
    VertexUnion (ForumVertex& f) : forum(f) {}
    VertexUnion (PublicationVertex& pub) : publication(pub) {}
    VertexUnion (TopicVertex& top) : topic(top) {}
  };

  agile::workflow1::TYPES v_type;
  VertexUnion v;
  std::uint64_t id;

  Vertex () : v_type(agile::workflow1::TYPES::NONE) {}
  Vertex (std::uint64_t uid, PersonVertex& p) : v_type(agile::workflow1::TYPES::PERSON), v(p) {id = uid;}
  Vertex (std::uint64_t uid, ForumEventVertex& fe) : v_type(agile::workflow1::TYPES::FORUMEVENT), v(fe) {id = uid;}
  Vertex (std::uint64_t uid, ForumVertex& f) : v_type(agile::workflow1::TYPES::FORUM), v(f) {id = uid;}
  Vertex (std::uint64_t uid, PublicationVertex& pub) : v_type(agile::workflow1::TYPES::PUBLICATION), v(pub) {id = uid;}
  Vertex (std::uint64_t uid, TopicVertex& top) : v_type(agile::workflow1::TYPES::TOPIC), v(top) {id = uid;}

};

template<typename T>
T get_node_data(const Vertex& ver) { return T(); }

template <>
PersonVertex get_node_data(const Vertex& ver) { return ver.v.person; }

template <>
ForumVertex get_node_data(const Vertex& ver) { return ver.v.forum; }

template <>
ForumEventVertex get_node_data(const Vertex& ver) { return ver.v.forum_event; }

template <>
PublicationVertex get_node_data(const Vertex& ver) { return ver.v.publication; }

template <>
TopicVertex get_node_data(const Vertex& ver) { return ver.v.topic; }


struct Edge {
  union EdgeUnion {
    PurchaseEdge purchase;
    SaleEdge sale;
    AuthorEdge author;
    IncludesEdge includes;
    HasTopicEdge has_topic;
    HasOrgEdge has_org;
    NoneEdge none;

    EdgeUnion () : none(NoneEdge()) {}
    EdgeUnion (PurchaseEdge& p) : purchase(p) {}
    EdgeUnion (SaleEdge& s) : sale(s) {}
    EdgeUnion (AuthorEdge& a) : author(a) {}
    EdgeUnion (IncludesEdge& inc) : includes(inc) {}
    EdgeUnion (HasTopicEdge& top) : has_topic(top) {}
    EdgeUnion (HasOrgEdge& org) : has_org(org) {}
  };

  agile::workflow1::TYPES type;
  EdgeUnion e;
  agile::workflow1::TYPES src_type;
  agile::workflow1::TYPES dst_type;
  std::uint64_t src;
  std::uint64_t dst;

  Edge () : type(agile::workflow1::TYPES::NONE) {}
  Edge (PurchaseEdge& p) : type(agile::workflow1::TYPES::PURCHASE), e(p) {
    src = e.purchase.src();
    src_type = e.purchase.src_type;
    dst = e.purchase.dst();
    dst_type = e.purchase.dst_type;
  }
  Edge (SaleEdge& s) : type(agile::workflow1::TYPES::SALE), e(s) {
    src = e.sale.src();
    src_type = e.sale.src_type;
    dst = e.sale.dst();
    dst_type = e.sale.dst_type;
  }
  Edge (AuthorEdge& a) : type(agile::workflow1::TYPES::AUTHOR), e(a) {
    src = e.author.src();
    src_type = e.author.src_type;
    dst = e.author.dst();
    dst_type = e.author.dst_type;
  }
  Edge (IncludesEdge& pub) : type(agile::workflow1::TYPES::INCLUDES), e(pub) {
    src = e.includes.src();
    src_type = e.includes.src_type;
    dst = e.includes.dst();
    dst_type = e.includes.dst_type;
  }
  Edge (HasTopicEdge& top) : type(agile::workflow1::TYPES::HASTOPIC), e(top) {
    src = e.has_topic.src();
    src_type = e.has_topic.src_type;
    dst = e.has_topic.dst();
    dst_type = e.has_topic.dst_type;
  }
  Edge (HasOrgEdge& org) : type(agile::workflow1::TYPES::HASORG), e(org) {
    src = e.has_org.src();
    src_type = e.has_org.src_type;
    dst = e.has_org.dst();
    dst_type = e.has_org.dst_type;
  }

};

template<typename T>
T get_edge_data(const Edge& edge) { return T(); }

template <>
PurchaseEdge get_edge_data(const Edge& edge) { return edge.e.purchase; }

template <>
SaleEdge get_edge_data(const Edge& edge) { return edge.e.sale; }

template <>
AuthorEdge get_edge_data(const Edge& edge) { return edge.e.author; }

template <>
IncludesEdge get_edge_data(const Edge& edge) { return edge.e.includes; }

template <>
HasTopicEdge get_edge_data(const Edge& edge) { return edge.e.has_topic; }

template <>
HasOrgEdge get_edge_data(const Edge& edge) { return edge.e.has_org; }


template <typename V, typename E>
class Wf2WMDParser : public galois::graphs::FileParser<V,E> {
public:
  Wf2WMDParser(std::vector<std::string> files) : csvFields_(10), files_(files) {}
  Wf2WMDParser(uint64_t csvFields, std::vector<std::string> files)
      : csvFields_(csvFields), files_(files) {}

  virtual const std::vector<std::string>& GetFiles() override { return files_; }
  virtual galois::graphs::ParsedGraphStructure<V, E> ParseLine(char* line,
                                               uint64_t lineLength) override {
    std::vector<std::string> tokens =
        this->SplitLine(line, lineLength, ',', csvFields_);

    if (tokens[0] == "Person") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[1]);
      auto pv = PersonVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, pv));
    } else if (tokens[0] == "ForumEvent") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[4]);
      auto fev = ForumEventVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, fev));
    } else if (tokens[0] == "Forum") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[3]);
      auto fv = ForumVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, fv));
    } else if (tokens[0] == "Publication") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[5]);
      auto pv = PublicationVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, pv));
    } else if (tokens[0] == "Topic") {
      galois::graphs::ParsedUID uid =
          shad::data_types::encode<uint64_t, std::string, UINT>(tokens[6]);
      auto tv = TopicVertex(tokens);
      return galois::graphs::ParsedGraphStructure<V, E>(
          V(uid, tv));
    } else { // edge type
      agile::workflow1::TYPES inverseEdgeType;
      E edge;
      if (tokens[0] == "Sale") {
        inverseEdgeType = agile::workflow1::TYPES::PURCHASE;
        auto e = PurchaseEdge(tokens);
        edge = E(e);
      } else if (tokens[0] == "Author") {
        inverseEdgeType = agile::workflow1::TYPES::WRITTENBY;
        auto e = AuthorEdge(tokens);
        edge = E(e);
      } else if (tokens[0] == "Includes") {
        inverseEdgeType = agile::workflow1::TYPES::INCLUDEDIN;
        auto e = IncludesEdge(tokens);
        edge = E(e);
      } else if (tokens[0] == "HasTopic") {
        inverseEdgeType = agile::workflow1::TYPES::TOPICIN;
        auto e = HasTopicEdge(tokens);
        edge = E(e);
      } else if (tokens[0] == "HasOrg") {
        inverseEdgeType = agile::workflow1::TYPES::ORGIN;
        auto e = HasOrgEdge(tokens);
        edge = E(e);
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
/*using LC_CSR_Graph = galois::graphs::LC_CSR_64_Graph<Vertex, Edge>::with_no_lockable<true>::type;
using LS_LC_CSR_Graph = galois::graphs::LS_LC_CSR_64_Graph<Vertex, Edge>::with_no_lockable<true>::type;

template<typename Graph>
class WF2_Graph {
  Graph* g;
  std::unordered_map<uint64_t, uint64_t> id_to_node_index;

  bool is_node(agile::workflow1::TYPES elem_t) {
    switch (elem_t) {
    case agile::workflow1::TYPES::PERSON:
    case agile::workflow1::TYPES::FORUM:
    case agile::workflow1::TYPES::FORUMEVENT:
    case agile::workflow1::TYPES::PUBLICATION:
    case agile::workflow1::TYPES::TOPIC:
      return true;
    default:
      return false;
    }
  }

  template<typename GraphElemType>
  agile::workflow1::TYPES graph_type_enum() {
    if (std::is_same<GraphElemType, PersonVertex>::value) {
      return agile::workflow1::TYPES::PERSON;
    } else if (std::is_same<GraphElemType, ForumVertex>::value) {
      return agile::workflow1::TYPES::FORUM;
    } else if (std::is_same<GraphElemType, ForumEventVertex>::value) {
      return agile::workflow1::TYPES::FORUMEVENT;
    } else if (std::is_same<GraphElemType, PublicationVertex>::value) {
      return agile::workflow1::TYPES::PUBLICATION;
    } else if (std::is_same<GraphElemType, TopicVertex>::value) {
      return agile::workflow1::TYPES::TOPIC;
    } else if (std::is_same<GraphElemType, PurchaseEdge>::value) {
      return agile::workflow1::TYPES::PURCHASE;
    } else if (std::is_same<GraphElemType, SaleEdge>::value) {
      return agile::workflow1::TYPES::SALE;
    } else if (std::is_same<GraphElemType, AuthorEdge>::value) {
      return agile::workflow1::TYPES::AUTHOR;
    } else if (std::is_same<GraphElemType, IncludesEdge>::value) {
      return agile::workflow1::TYPES::INCLUDES;
    } else if (std::is_same<GraphElemType, HasTopicEdge>::value) {
      return agile::workflow1::TYPES::HASTOPIC;
    } else if (std::is_same<GraphElemType, HasOrgEdge>::value) {
      return agile::workflow1::TYPES::HASORG;
    } else {
      return agile::workflow1::TYPES::NONE;
    }
  }

public:
  WF2_Graph(Graph* g, std::unordered_map<uint64_t, uint64_t>& id_to_node_index) : g(g) {
    this->id_to_node_index.swap(id_to_node_index);
  }

  template<typename NodeType>
  bool lookup_node(uint64_t id, NodeType& node) {
    if (id_to_node_index.find(id) != id_to_node_index.end()) {
      auto node_data = g->getData(id_to_node_index[id]);
      node = get_node_data<NodeType>(node_data); 
      return true;
    }

    return false;
  }

  template<typename EdgeType>
  EdgeType lookup_edge(uint64_t src_id, uint64_t dst_id) {
    using GNode = typename Graph::GraphNode;
    EdgeType edge_data;

    galois::do_all(
      galois::iterate(*g),
      [&] (GNode n) {
        for (auto e : g->edges(n)) {
          auto& e_data = g->getEdgeData(e);
          if (e_data.src() == src_id && e_data.dst() == dst_id) {
            edge_data = get_edge_data<EdgeType>(e_data);
          }
        }
      });

    return edge_data;
  }

  template<typename GraphElemType, typename Op>
  void do_all(agile::workflow1::TYPES ty, Op f) {
    // execute f on each vertex/edge of type GraphElemType
    using GNode = typename Graph::GraphNode;
    if (is_node(ty)) { 
      galois::do_all(
        galois::iterate(*g),
        [&] (GNode n) {
          auto& n_data = g->getData(n);
          if (n_data.v_type == ty) {
            f(get_node_data<GraphElemType>(n_data));
          }
        });
    } else {
      galois::do_all(
        galois::iterate(*g),
        [&] (GNode n) {
          for (auto e : g->edges(n)) {
            auto& e_data = g->getEdgeData(e);
            if (e_data.e_type == ty) {
              f(get_edge_data<GraphElemType>(e_data));
            }
          }
        });
    }
  }

  template<typename NodeType, typename EdgeType, typename Op>
  void iter_edges(agile::workflow1::TYPES node_ty, agile::workflow1::TYPES edge_ty, Op f) {
    // call f on each vertex and its list of out-edges with type EdgeType 
    using GNode = typename Graph::GraphNode; 
    galois::do_all(
      galois::iterate(*g),
      [&] (GNode n) {
        auto& n_data = g->getData(n);
        if (n_data.v_type != node_ty) {
          return;
        }

        std::vector<EdgeType> vec;
        for (auto e : g->edges(n)) {
          auto& e_data = g->getEdgeData(e);
          if (e_data.e_type == edge_ty) {
            vec.push_back(get_edge_data<EdgeType>(e_data));
          }
        }
        
        f(get_node_data<NodeType>(n_data), vec);
      });
  }

  template<typename EdgeType, typename Op>
  void iter_edges(agile::workflow1::TYPES edge_ty, Op f) {
    // call f on each vertex and its list of out-edges with type EdgeType 
    using GNode = typename Graph::GraphNode; 
    galois::do_all(
      galois::iterate(*g),
      [&] (GNode n) {
        auto& n_data = g->getData(n);
        std::vector<EdgeType> vec;
        for (auto e : g->edges(n)) {
          auto& e_data = g->getEdgeData(e);
          if (e_data.e_type == edge_ty) {
            vec.push_back(get_edge_data<EdgeType>(e_data));
          }
        }
        
        f(n_data.id(), vec);
      });
  }

  template<typename NodeType, typename EdgeType, typename Op>
  void iter_edges(uint64_t id, agile::workflow1::TYPES node_ty, agile::workflow1::TYPES edge_ty, Op f) {
    // call f on list of out-edges with type EdgeType of vertex with given id 
    NodeType node;
    std::vector<EdgeType> edges;
    if (id_to_node_index.find(id) != id_to_node_index.end()) {
      uint64_t n = id_to_node_index[id];
      auto node_data = g->getData(n);
      if (node_data.v_type != node_ty) return;

      node = get_node_data<NodeType>(node_data);
      for (auto e : g->edges(n)) {
        auto& edge_data = g->getEdgeData(e);
        if (edge_data.e_type == edge_ty) {
          edges.push_back(get_edge_data<EdgeType>(edge_data));
        }
      }
    }

    f(node, edges);
  }
};*/


} // namespace wf2

#endif
