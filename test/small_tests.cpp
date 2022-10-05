#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>

#include "graph-mutable-va.hpp"

TEST_CASE("Adding Nodes and Edges Test", "[graph]")
{
  Graph* g = new Graph();

  REQUIRE( g != nullptr);
  REQUIRE( g->get_num_nodes() == 0 );
  REQUIRE( g->get_edge_end() == 0 );
  REQUIRE( g->get_node(0) == nullptr);
  REQUIRE( g->get_edge(0) == nullptr);

  SECTION( "Add One Node" )
  {
    auto n = g->ingestNode();

    REQUIRE( n == 0 );

    REQUIRE( g->get_num_nodes() == 1 );
    REQUIRE( g->get_node(0) != nullptr );
    REQUIRE( g->get_node(0)->start.get_value() == 0 );
    REQUIRE( g->get_node(0)->stop == 0 );

    //Check that everything else is the same
    REQUIRE( g != nullptr );
    REQUIRE( g->get_edge_end() == 0 );
    REQUIRE( g->get_edge(0) == nullptr );
  }
  SECTION( "Add One Self Edge" )
  {
    REQUIRE( g != nullptr );
    auto n_ind = g->ingestNode();
    REQUIRE( n_ind == 0 );
    uint64_t d[1] = {0};
    g->ingestEdges(1, 0, d);

    REQUIRE( g->get_edge_end() == 1 );

    REQUIRE( g->get_node(0) != nullptr );
    auto n = g->get_node(0);
    REQUIRE( n->start.get_value() == 0 );
    REQUIRE( n->stop == 1 );
    REQUIRE( g->get_edge(0) != nullptr );
    REQUIRE( g->get_edge(0)->get_dest() == 0 );

    //Check that everything else is the same
    REQUIRE( g != nullptr );
    REQUIRE( g->get_num_nodes() == 1 );
  }

  delete g;
}
