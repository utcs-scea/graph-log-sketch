#include <catch2/catch_test_macros.hpp>
#include "graph-mutable-va.hpp"
#include <test_bench.hpp>
#include <galois/graphs/MorphGraph.h>

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
