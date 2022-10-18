#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>
#include <test_bench.hpp>


TEST_CASE("Adding Nodes and Edges Test", "[updates]")
{
  pa c = create_counters();

  SECTION( "Graph Creation" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      run_test_and_benchmark<Graph<par, ehp>**>(s, c, 4,
        []{return (Graph<par, ehp>**) malloc(sizeof(Graph<par, ehp>*));},
        [](Graph<par, ehp>** p){ *p = new Graph<par, ehp>(); },
        [](Graph<par, ehp>** p)
        {
          Graph<par, ehp>* g = *p;
          REQUIRE( g != nullptr);
          REQUIRE( g->get_num_nodes() == 0 );
          REQUIRE( g->get_edge_end() == 0 );
          REQUIRE( g->get_node(0) == nullptr );
          REQUIRE( g->get_edge(0) == nullptr );
        },
        [](Graph<par, ehp>** p){ delete *p; free(p); });
    };
    runner("Graph Creation", f);
  }

  SECTION( "Add One Node" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        Graph<par, ehp>* g;
        uint64_t         n;
      };
      run_test_and_benchmark<RetVal*>(s, c, 4,
        []{RetVal* r = (RetVal*) malloc(sizeof(RetVal)); r->g = new Graph<par, ehp>(); return r; },
        [](RetVal* r){ r->n = r->g->ingestNode(); },
        [](RetVal* r)
        {
          REQUIRE( r->n == 0 );
          Graph<par, ehp>* g = r->g;

          REQUIRE( g->get_num_nodes() == 1 );
          REQUIRE( g->get_node(0) != nullptr );
          REQUIRE( g->get_node(0)->start.get_value() == 0 );
          REQUIRE( g->get_node(0)->stop == 0 );

          //Check that everything else is the same
          REQUIRE( g != nullptr );
          REQUIRE( g->get_edge_end() == 0 );
          REQUIRE( g->get_edge(0) == nullptr );
        },
        [](RetVal* r){ delete r->g; free(r); });
    };
    runner("Add One Node", f);
  }

  SECTION( "Add 100 Nodes One at a Time" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      typedef Graph<par, ehp> RetVal;
      run_test_and_benchmark<RetVal*>(s, c, 4,
        []{ return new Graph<par, ehp>(); },
        [](RetVal* g){ for(uint64_t i = 0; i < 100; i++) g->ingestNode(); },
        [](RetVal* g)
        {
          REQUIRE( g->get_num_nodes() == 100 );
          REQUIRE( g->get_edge_end() == 0 );
          REQUIRE( g->get_edge(0) == nullptr );

          for(uint32_t i = 0; i < 100; i++)
          {
            REQUIRE( g->get_node(i) != nullptr );
            REQUIRE( g->get_node(i)->start.get_value() == 0 );
            REQUIRE( g->get_node(i)->stop == 0 );
          }
        },
        [](RetVal* g){ delete g; });
    };

    runner("Add 100 Nodes 1 at a Time", f);
  }

  SECTION( "Bulk Add 100 Nodes")
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      struct RetVal
      {
        uint64_t          start;
        uint64_t          end;
        Graph<par, ehp>*  g;
      };

      run_test_and_benchmark<RetVal*>(s, c, 3,
        []{ auto r = new RetVal(); r->g = new Graph<par, ehp>(); return r; },
        [](RetVal* r){ r->start = r->g->ingestNodes(100, *&r->end); },
        [](RetVal* r)
        {
          uint64_t end = r->end;
          uint64_t s = r->start;
          auto g = r->g;
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
        },
        [](RetVal* r){ delete r->g; delete r; }
      );
    };

    runner("Bulk Add 100 Nodes", f);
  }

  SECTION( "Add One Self Edge" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      run_test_and_benchmark<Graph<par, ehp>*>(s, c, 4,
        []{ auto g = new Graph<par, ehp>(); auto n_ind = g->ingestNode(); REQUIRE( n_ind == 0 ); return g; },
        [](Graph<par, ehp>* g) { uint64_t d[1] = {0}; g->ingestEdges(1, 0, d); },
        [](Graph<par, ehp>* g)
        {
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
        },
        [](Graph<par, ehp>* g){ delete g; });
    };

    runner("Add One Self Edge", f);
  }

  SECTION( "Add One Normal Edge" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      typedef Graph<par, ehp> RetVal;
      run_test_and_benchmark<RetVal*>(s, c, 4, []
        {
          auto g = new Graph<par, ehp>();
          for(uint64_t i = 0; i < 2; i++)
          {
            auto n = g->ingestNode();
            REQUIRE( n == i );
          }
          return g;
        },
        [](RetVal* g){ uint64_t d[1] = {1}; g->ingestEdges(1, 0, d); },
        [](RetVal* g)
        {
          REQUIRE( g->get_node(0)->start.get_value() == 0 );
          REQUIRE( g->get_node(0)->stop == 1 );
          REQUIRE( g->get_edge(0) != nullptr );
          REQUIRE( g->get_edge(0)->get_dest() == 1 );
          REQUIRE( g->get_edge(0)->is_tomb() == false );
        },
        [](RetVal* g){ delete g; });
    };

    runner("Add One Normal Edge", f);
  }

  SECTION( "Add the same Edge Twice" )
  {
    auto f = [c]<bool par, bool ehp>(std::string s)
    {
      typedef Graph<par, ehp> RetVal;
      run_test_and_benchmark<RetVal*>(s, c, 6, []
        {
          auto g = new Graph<par, ehp>();
          for(uint64_t i = 0; i < 2; i++)
          {
            auto n = g->ingestNode();
            REQUIRE( n == i );
          }
          uint64_t d[1] = {1};
          g->ingestEdges(1, 0, d);
          return g;
        },
        [](RetVal* g){ uint64_t d[1] = {1}; g->ingestEdges(1, 0, d); },
        [](RetVal* g)
        {
          REQUIRE( g->get_node(0)->start.get_value() == 0 );
          REQUIRE( g->get_node(0)->stop >= 1 );
          REQUIRE( g->get_node(0)->stop <= 2 );
          REQUIRE( g->get_edge(0)->is_tomb() == false );
          REQUIRE( g->get_edge(0)->get_dest() == 1 );
          if(g->get_edge(1)) REQUIRE( g->get_edge(1)->is_tomb() == true );
        },
        [](RetVal* g){ delete g; });
    };

    runner("Add the same Edge Twice", f);
  }

}

TEST_CASE( "Generating Graphs from Files", "[files]")
{
  pa c = create_counters();

  SECTION( "Ingest Citeseer Graph from Edge List" )
  {
    check_el_file_and_benchmark(c, "../graphs/citeseer.el");
  }

  SECTION( "Ingest Cora Graph from Edge List" )
  {
    check_el_file_and_benchmark(c, "../graphs/cora.el");
  }
}
