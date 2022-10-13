#include <graph-mutable-va.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <benchmark.hpp>
#include <read_file_bench.hpp>

TEST_CASE( "Generate Large Graphs" )
{
  Graph* g = new Graph();
  pa c = create_counters();

  SECTION( "Ingest Yelp Graph from Edge List" )
  {
    check_el_file_and_benchmark(g, c, "/var/local/adityat/graph_samples_subset/yelp.el");
  }

  delete g;
}
