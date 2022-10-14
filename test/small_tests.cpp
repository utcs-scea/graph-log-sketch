#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>
#include <read_file_bench.hpp>

TEST_CASE("Adding Nodes and Edges Test", "[graph]")
{
  Graph* g = new Graph();

  REQUIRE( g != nullptr);
  REQUIRE( g->get_num_nodes() == 0 );
  REQUIRE( g->get_edge_end() == 0 );
  REQUIRE( g->get_node(0) == nullptr );
  REQUIRE( g->get_edge(0) == nullptr );

  pa c = create_counters();

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

    benchmark(c, "Add One Node", 1, [](pa c)
    {
      Graph* p = new Graph();
      reset_counters(c);
      start_counters(c);
      auto n = p->ingestNode();
      stop_counters(c);
      REQUIRE( n == 0 );
      delete p;
    });
  }

  SECTION( "Add 100 Nodes One at a Time" )
  {
    for(uint32_t i = 0 ; i < 100; i++)
    {
      auto n = g->ingestNode();

      REQUIRE( n == i );

      REQUIRE( g->get_num_nodes() == 1 + i );
      REQUIRE( g->get_node(i) != nullptr );
      REQUIRE( g->get_node(i)->start.get_value() == 0 );
      REQUIRE( g->get_node(i)->stop == 0 );

      //Check that everything else is the same
      REQUIRE( g != nullptr );
      REQUIRE( g->get_edge_end() == 0 );
      REQUIRE( g->get_edge(0) == nullptr );
    }

    benchmark(c, "Add 100 Nodes One at a Time", 2,[](pa c)
    {
      Graph* p = new Graph();
      reset_counters(c);
      start_counters(c);
      for(int i = 0; i < 100; i++) p->ingestNode();
      stop_counters(c);
      delete p;
    });
  }

  SECTION( "Bulk Add 100 Nodes")
  {
    uint64_t end;
    uint64_t s = g->ingestNodes(100, *&end);
    REQUIRE( g->get_num_nodes() == 100 );
    REQUIRE( s == 0 );
    REQUIRE( end == 100 );
    REQUIRE( g->get_edge_end() == 0 );
    REQUIRE( g->get_edge(0) == nullptr );

    for(uint32_t i = 0; i < 100; i++)
    {
      REQUIRE( g->get_node(i) != nullptr );
      REQUIRE( g->get_node(i)->start.get_value() == 0 );
      REQUIRE( g->get_node(i)->stop == 0 );
    }

    benchmark(c, "Bulk Add 100 Nodes", 3, [](pa c)
    {
      Graph* p = new Graph();
      uint64_t e;
      reset_counters(c);
      start_counters(c);
      p->ingestNodes(100, *&e);
      stop_counters(c);
      delete p;
    });
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
    REQUIRE( g->get_edge(0)->is_tomb() == false );

    //Check that everything else is the same
    REQUIRE( g != nullptr );
    REQUIRE( g->get_num_nodes() == 1 );

    benchmark(c, "Add One Self Edge", 4, [](pa c)
    {
      Graph* p = new Graph();
      p->ingestNode();
      uint64_t d[1] = {0};
      reset_counters(c);
      start_counters(c);
      p->ingestEdges(1, 0, d);
      stop_counters(c);
      delete p;
    });
  }

  SECTION( "Add One Normal Edge" )
  {
    for(uint32_t i = 0 ; i < 2; i++)
    {
      auto n = g->ingestNode();
      REQUIRE( n == i );
    }
    uint64_t d[1] = {1};
    g->ingestEdges(1, 0, d);
    REQUIRE( g->get_node(0)->start.get_value() == 0 );
    REQUIRE( g->get_node(0)->stop == 1 );
    REQUIRE( g->get_edge(0) != nullptr );
    REQUIRE( g->get_edge(0)->get_dest() == 1 );
    REQUIRE( g->get_edge(0)->is_tomb() == false );
    benchmark(c, "Add One Normal Edge", 5, [](pa c)
    {
      Graph* p = new Graph();
      p->ingestNode();
      p->ingestNode();
      uint64_t d[1] = {1};
      reset_counters(c);
      start_counters(c);
      p->ingestEdges(1, 0, d);
      stop_counters(c);
      delete p;
    });
  }

  SECTION( "Add the same Edge Twice" )
  {
    for(uint32_t i = 0 ; i < 2; i++)
    {
      auto n = g->ingestNode();
      REQUIRE( n == i );
    }
    uint64_t d[1] = {1};
    g->ingestEdges(1, 0, d);
    /* ingestEdges should not have duplicates in any one invocation */
    uint64_t e[1] = {1};
    g->ingestEdges(1, 0, e);

    REQUIRE( g->get_node(0)->start.get_value() == 0 );
    REQUIRE( g->get_node(0)->stop >= 1 );
    REQUIRE( g->get_node(0)->stop <= 2 );
    REQUIRE( g->get_edge(0)->is_tomb() == false );
    REQUIRE( g->get_edge(0)->get_dest() == 1 );
    if(g->get_edge(1)) REQUIRE( g->get_edge(1)->is_tomb() == true );

    benchmark(c, "Add Same Edge", 6, [](pa c)
    {
      Graph* p = new Graph();
      p->ingestNode();
      p->ingestNode();
      uint64_t d[1] = {1};
      p->ingestEdges(1, 0, d);
      uint64_t e[1] = {1};
      reset_counters(c);
      start_counters(c);
      p->ingestEdges(1, 0, e);
      stop_counters(c);
      delete p;
    });
  }

  SECTION( "Ingest Citeseer Graph from Edge List" )
  {
    check_el_file_and_benchmark(g, c, "../graphs/citeseer.el");
  }

  SECTION( "Ingest Cora Graph from Edge List" )
  {
    check_el_file_and_benchmark(g, c, "../graphs/cora.el");
  }


  delete g;
}
